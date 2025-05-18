  #include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// Chân servo
#define SERVO1_PIN 15      // Servo lối vào 
#define SERVO2_PIN 17      // Servo lối ra

// Cảm biến IR
#define IR_SENSOR_ENTRY 14
#define IR_SENSOR_EXIT 32
#define IR_SENSOR_IN_A 27
#define IR_SENSOR_OUT_A 26
#define IR_SENSOR_IN_B 25
#define IR_SENSOR_OUT_B 33

Servo servo1;
Servo servo2;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Biến đếm xe
int total = 0;
int total_A = 0;
int total_B = 0;

// Cờ và thời gian
bool requestEntry = false;
bool requestExit = false;
unsigned long exitRequestTime = 0;
const unsigned long EXIT_TIMEOUT = 5000; // 5 giây timeout

void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Khoi dong...");
  delay(1000);

  pinMode(IR_SENSOR_ENTRY, INPUT);
  pinMode(IR_SENSOR_EXIT, INPUT);
  pinMode(IR_SENSOR_IN_A, INPUT);
  pinMode(IR_SENSOR_OUT_A, INPUT);
  pinMode(IR_SENSOR_IN_B, INPUT);
  pinMode(IR_SENSOR_OUT_B, INPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);
  servo1.write(0);
  servo2.write(0);
}

void openGate(Servo& s, const char* msgDone) {
  s.write(90); // mở barrier
  delay(3000); // đợi xe đi qua
  s.write(0);  // đóng lại
  Serial.println(msgDone); // gửi tín hiệu hoàn thành
}

void loop() {
  static int lastEntry = HIGH, lastExit = HIGH;
  static int lastInA = HIGH, lastOutA = HIGH;
  static int lastInB = HIGH, lastOutB = HIGH;

  int currEntry = digitalRead(IR_SENSOR_ENTRY);
  int currExit  = digitalRead(IR_SENSOR_EXIT);
  int currInA   = digitalRead(IR_SENSOR_IN_A);
  int currOutA  = digitalRead(IR_SENSOR_OUT_A);
  int currInB   = digitalRead(IR_SENSOR_IN_B);
  int currOutB  = digitalRead(IR_SENSOR_OUT_B);

  // PHÁT HIỆN XE VÀO
  if (currEntry == LOW && lastEntry == HIGH && !requestEntry) {
    Serial.println("CAR_ENTRY"); // Gửi yêu cầu đến Python
    requestEntry = true;
  }

  // PHÁT HIỆN XE RA
  if (currExit == LOW && lastExit == HIGH && !requestExit) {
    Serial.println("CAR_EXIT"); // Gửi yêu cầu đến Python
    requestExit = true;
    exitRequestTime = millis();
  }

  // TIMEOUT nếu không có phản hồi từ Python
  if (requestExit && millis() - exitRequestTime > EXIT_TIMEOUT) {
    Serial.println("TIMEOUT_EXIT");
    requestExit = false;
  }

  // VÀO BÃI A
  if (currInA == LOW && lastInA == HIGH) {
    total_A++;
    Serial.println("Vao Bai A");
    delay(1000);
  }

  // RA BÃI A
  if (currOutA == LOW && lastOutA == HIGH && total_A > 0) {
    total_A--;
    Serial.println("Ra Bai A");
    delay(1000);
  }

  // VÀO BÃI B
  if (currInB == LOW && lastInB == HIGH) {
    total_B++;
    Serial.println("Vao Bai B");
    delay(1000);
  }

  // RA BÃI B
  if (currOutB == LOW && lastOutB == HIGH && total_B > 0) {
    total_B--;
    Serial.println("Ra Bai B");
    delay(1000);
  }

  // NHẬN LỆNH TỪ PYTHON
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "RUN_SERVO1") {
      openGate(servo1, "SERVO1_DONE");
      if (requestEntry) {
        total++;
        requestEntry = false;
      }
    }

    if (cmd == "RUN_SERVO2") {
      openGate(servo2, "SERVO2_DONE");
      if (requestExit && total > 0) {
        total--;
        requestExit = false;
      }
    }
  }

  // Lưu trạng thái cảm biến
  lastEntry = currEntry;
  lastExit  = currExit;
  lastInA   = currInA;
  lastOutA  = currOutA;
  lastInB   = currInB;
  lastOutB  = currOutB;

  // Hiển thị LCD
  static int lastTotal = -1, lastA = -1, lastB = -1;
  if (total != lastTotal || total_A != lastA || total_B != lastB || requestExit) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tong so xe: ");
    lcd.print(total);
    lcd.setCursor(0, 1);
    lcd.print("A:");
    lcd.print(total_A);
    lcd.print(" B:");
    lcd.print(total_B);

    if (requestExit) {
      lcd.setCursor(0, 1);
      lcd.print("Dang doi bien so");
    }

    lastTotal = total;
    lastA = total_A;
    lastB = total_B;
  }

  delay(200);
}
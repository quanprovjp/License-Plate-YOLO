#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

// Định nghĩa chân GPIO
#define SERVO1_PIN 16     // Servo lối vào
#define SERVO2_PIN 18     // Servo lối ra
#define IR_SENSOR_ENTRY 14 // IR lối vào
#define IR_SENSOR_EXIT 32  // IR lối ra
#define IR_SENSOR_IN_A 27  // IR vào bãi A
#define IR_SENSOR_OUT_A 26 // IR ra bãi A
#define IR_SENSOR_IN_B 25  // IR vào bãi B
#define IR_SENSOR_OUT_B 33 // IR ra bãi B
#define LED_A_PIN 12       // LED chỉ hướng bãi A
#define LED_B_PIN 13       // LED chỉ hướng bãi B

Servo servo1;
Servo servo2;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ I2C LCD

// Biến toàn cục
int total = 0;
int total_A = 0;
int total_B = 0;
const int max_A = 10;
const int max_B = 10;

void updateLCD() {
  lcd.setCursor(0, 0);
  char line1[16];
  sprintf(line1, "Total: %d      ", total); // thêm khoảng trắng để xóa ký tự thừa
  lcd.print(line1);

  lcd.setCursor(0, 1);
  char line2[16];
  sprintf(line2, "A:%d B:%d      ", total_A, total_B);
  lcd.print(line2);
}

void updateLED() {
  int slots_A = max_A - total_A;
  int slots_B = max_B - total_B;
  if (slots_A > slots_B) {
    digitalWrite(LED_A_PIN, HIGH);
    digitalWrite(LED_B_PIN, LOW);
  } else if (slots_B > slots_A) {
    digitalWrite(LED_A_PIN, LOW);
    digitalWrite(LED_B_PIN, HIGH);
  } else {
    digitalWrite(LED_A_PIN, LOW);
    digitalWrite(LED_B_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("");
  delay(1000);
  lcd.clear();

  // Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);
  servo1.write(90); // Đặt góc ban đầu của servo1 là 90 độ

  // IR sensor
  pinMode(IR_SENSOR_ENTRY, INPUT);
  pinMode(IR_SENSOR_EXIT, INPUT);
  pinMode(IR_SENSOR_IN_A, INPUT);
  pinMode(IR_SENSOR_OUT_A, INPUT);
  pinMode(IR_SENSOR_IN_B, INPUT);
  pinMode(IR_SENSOR_OUT_B, INPUT);

  // LED
  pinMode(LED_A_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  digitalWrite(LED_A_PIN, LOW);
  digitalWrite(LED_B_PIN, LOW);
}

void loop() {
  static int lastState_Entry = HIGH;
  static int lastState_Exit = HIGH;
  static int lastState_InA = HIGH;
  static int lastState_OutA = HIGH;
  static int lastState_InB = HIGH;
  static int lastState_OutB = HIGH;

  int state_Entry = digitalRead(IR_SENSOR_ENTRY);
  int state_Exit = digitalRead(IR_SENSOR_EXIT);
  int state_InA = digitalRead(IR_SENSOR_IN_A);
  int state_OutA = digitalRead(IR_SENSOR_OUT_A);
  int state_InB = digitalRead(IR_SENSOR_IN_B);
  int state_OutB = digitalRead(IR_SENSOR_OUT_B);

  // Tự động mở SERVO1 khi có xe vào
  if (state_Entry == LOW && lastState_Entry == HIGH) {
    Serial.println("CAR_ENTRY");
    total++; // Tăng total khi xe vào
  }

  // Tự động mở SERVO2 khi có xe ra, không giảm total dưới 0
  if (state_Exit == LOW && lastState_Exit == HIGH) {
    Serial.println("CAR_EXIT");
    if (total > 0) {
      total--; // Chỉ giảm total nếu total lớn hơn 0
    }
  }

  // Cập nhật số xe bãi A, không giảm total_A dưới 0
  if (state_InA == LOW && lastState_InA == HIGH) {
    total_A++;
    delay(100);
  }
  if (state_OutA == LOW && lastState_OutA == HIGH) {
    if (total_A > 0) {
      total_A--; // Chỉ giảm total_A nếu total_A lớn hơn 0
    }
    delay(100);
  }

  // Cập nhật số xe bãi B, không giảm total_B dưới 0
  if (state_InB == LOW && lastState_InB == HIGH) {
    total_B++;
    delay(100);
  }
  if (state_OutB == LOW && lastState_OutB == HIGH) {
    if (total_B > 0) {
      total_B--; // Chỉ giảm total_B nếu total_B lớn hơn 0
    }
    delay(100);
  }

  // Nhận lệnh từ Python (nếu cần giữ lại)
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "RUN_SERVO1") {
      servo1.write(0);  // Vị trí ban đầu (đóng)
      delay(5000);       // Chờ một chút để ổn định
      servo1.write(90); // Đảo chiều, quay đến 180° (mở)
      Serial.println("SERVO1_DONE");
    } else if (command == "RUN_SERVO2") {
      servo2.write(90);
      delay(5000);
      servo2.write(0);
      Serial.println("SERVO2_DONE");
    }
  }

  // Cập nhật trạng thái cảm biến
  lastState_Entry = state_Entry;
  lastState_Exit = state_Exit;
  lastState_InA = state_InA;
  lastState_OutA = state_OutA;
  lastState_InB = state_InB;
  lastState_OutB = state_OutB;

  updateLCD();
  updateLED();

  // Debug Serial
  Serial.print("Total: ");
  Serial.print(total);
  Serial.print(" | A: ");
  Serial.print(total_A);
  Serial.print(" | B: ");
  Serial.println(total_B);

  delay(100);
}
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// Chân servo
#define SERVO1_PIN 15
#define SERVO2_PIN 17

// Cảm biến IR
#define IR_SENSOR_ENTRY 14
#define IR_SENSOR_EXIT 32
#define IR_SENSOR_IN_A 27
#define IR_SENSOR_OUT_A 26
#define IR_SENSOR_IN_B 25
#define IR_SENSOR_OUT_B 33

Servo servo1, servo2;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Biến đếm
int total = 0, total_A = 0, total_B = 0;

// Cờ và thời gian
bool requestEntry = false, requestExit = false;
unsigned long exitRequestTime = 0;
const unsigned long EXIT_TIMEOUT = 5000;

// Servo state machine
enum ServoState {IDLE, OPENING, WAITING, CLOSING};
ServoState servo1State = IDLE, servo2State = IDLE;
unsigned long servo1Timer = 0, servo2Timer = 0;
const unsigned long SERVO_WAIT_TIME = 3000;

// Serial buffer
String serialBuffer = "";

// Hiển thị LCD
unsigned long lastLCDUpdate = 0;
int lastTotal = -1, lastA = -1, lastB = -1;

void setup() {
  Serial.begin(115200);
  lcd.begin(); lcd.backlight(); lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Khoi dong...");
  delay(1000);

  pinMode(IR_SENSOR_ENTRY, INPUT);
  pinMode(IR_SENSOR_EXIT, INPUT);
  pinMode(IR_SENSOR_IN_A, INPUT);
  pinMode(IR_SENSOR_OUT_A, INPUT);
  pinMode(IR_SENSOR_IN_B, INPUT);
  pinMode(IR_SENSOR_OUT_B, INPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50); servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);
  servo1.write(0); servo2.write(0);
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

  // XE VÀO
  if (currEntry == LOW && lastEntry == HIGH && !requestEntry) {
    Serial.println("CAR_ENTRY");
    requestEntry = true;
  }

  // XE RA
  if (currExit == LOW && lastExit == HIGH && !requestExit) {
    Serial.println("CAR_EXIT");
    requestExit = true;
    exitRequestTime = millis();
  }

  if (requestExit && millis() - exitRequestTime > EXIT_TIMEOUT) {
    Serial.println("TIMEOUT_EXIT");
    requestExit = false;
  }

  // VÀO RA BÃI A
  if (currInA == LOW && lastInA == HIGH) {
    total_A++; Serial.println("Vao Bai A");
  }
  if (currOutA == LOW && lastOutA == HIGH && total_A > 0) {
    total_A--; Serial.println("Ra Bai A");
  }

  // VÀO RA BÃI B
  if (currInB == LOW && lastInB == HIGH) {
    total_B++; Serial.println("Vao Bai B");
  }
  if (currOutB == LOW && lastOutB == HIGH && total_B > 0) {
    total_B--; Serial.println("Ra Bai B");
  }

  // NON-BLOCKING SERIAL
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processSerialCommand(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }

  // SERVO 1 STATE MACHINE
  handleServo(servo1, servo1State, servo1Timer, requestEntry, total, "SERVO1_DONE");

  // SERVO 2 STATE MACHINE
  handleServo(servo2, servo2State, servo2Timer, requestExit, total, "SERVO2_DONE");

  // Cập nhật LCD mỗi 300ms nếu thay đổi
  if (millis() - lastLCDUpdate > 300) {
    if (total != lastTotal || total_A != lastA || total_B != lastB || requestExit) {
      lcd.setCursor(0, 0);
      lcd.print("Tong so xe: ");
      lcd.print(total);
      lcd.print("   ");

      lcd.setCursor(0, 1);
      if (requestExit) {
        lcd.print("Dang doi bien so ");
      } else {
        lcd.print("A:");
        lcd.print(total_A);
        lcd.print(" B:");
        lcd.print(total_B);
        lcd.print("     ");
      }

      lastTotal = total;
      lastA = total_A;
      lastB = total_B;
      lastLCDUpdate = millis();
    }
  }

  // Cập nhật trạng thái cảm biến
  lastEntry = currEntry; lastExit = currExit;
  lastInA = currInA; lastOutA = currOutA;
  lastInB = currInB; lastOutB = currOutB;
}

void processSerialCommand(const String& cmd) {
  if (cmd == "RUN_SERVO1" && servo1State == IDLE) {
    servo1State = OPENING;
    servo1Timer = millis();
  } else if (cmd == "RUN_SERVO2" && servo2State == IDLE) {
    servo2State = OPENING;
    servo2Timer = millis();
  }
}

void handleServo(Servo& s, ServoState& state, unsigned long& timer, bool& requestFlag, int& total, const char* doneMsg) {
  switch (state) {
    case OPENING:
      s.write(90);
      timer = millis();
      state = WAITING;
      break;

    case WAITING:
      if (millis() - timer >= SERVO_WAIT_TIME) {
        s.write(0);
        timer = millis();
        state = CLOSING;
      }
      break;

    case CLOSING:
      if (millis() - timer >= 500) {
        Serial.println(doneMsg);
        if (requestFlag) {
          if (String(doneMsg) == "SERVO1_DONE") total++;
          else if (String(doneMsg) == "SERVO2_DONE" && total > 0) total--;
          requestFlag = false;
        }
        state = IDLE;
      }
      break;

    case IDLE:
    default:
      break;
  }
}


#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

// Định nghĩa chân GPIO
#define SERVO1_PIN 15      // Servo lối vào (đổi từ 21)
#define SERVO2_PIN 17      // Servo lối ra
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
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ I2C: 0x27 (có thể là 0x3F, kiểm tra)

// Biến toàn cục
int total = 0;     // Tổng số xe
int total_A = 0;   // Số xe bãi A
int total_B = 0;   // Số xe bãi B
const int max_A = 10; // Sức chứa bãi A
const int max_B = 10; // Sức chứa bãi B
unsigned long lastDisplayTime = 0;
int displayState = 0; // 0: Total, 1: Bãi A, 2: Bãi B

void updateLCD() {
  if (millis() - lastDisplayTime >= 2000) { // Đổi hiển thị mỗi 2 giây
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Parking");
    
    lcd.setCursor(0, 1);
    char buffer[16];
    switch (displayState) {
      case 0:
        sprintf(buffer, "T:%d", total);
        lcd.print(buffer);
        displayState = 1;
        break;
      case 1:
        sprintf(buffer, "A:%d/%d", total_A, max_A);
        lcd.print(buffer);
        displayState = 2;
        break;
      case 2:
        sprintf(buffer, "B:%d/%d", total_B, max_B);
        lcd.print(buffer);
        displayState = 0;
        break;
    }
    lastDisplayTime = millis();
  }
}

void updateLED() {
  int slots_A = max_A - total_A; // Chỗ trống bãi A
  int slots_B = max_B - total_B; // Chỗ trống bãi B
  if (slots_A > slots_B) {
    digitalWrite(LED_A_PIN, HIGH); // Chỉ hướng bãi A
    digitalWrite(LED_B_PIN, LOW);
  } else if (slots_B > slots_A) {
    digitalWrite(LED_B_PIN, HIGH); // Chỉ hướng bãi B
    digitalWrite(LED_A_PIN, LOW);
  } else {
    digitalWrite(LED_A_PIN, LOW); // Tắt cả hai nếu bằng nhau
    digitalWrite(LED_B_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);

  // Khởi động LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Parking System");
  delay(1000);

  // Cấu hình servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);
  servo1.write(0); // Đóng barrier lối vào
  servo2.write(0); // Đóng barrier lối ra

  // Cấu hình cảm biến IR
  pinMode(IR_SENSOR_ENTRY, INPUT);
  pinMode(IR_SENSOR_EXIT, INPUT);
  pinMode(IR_SENSOR_IN_A, INPUT);
  pinMode(IR_SENSOR_OUT_A, INPUT);
  pinMode(IR_SENSOR_IN_B, INPUT);
  pinMode(IR_SENSOR_OUT_B, INPUT);


  // Cấu hình LED
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

  // Kiểm tra cảm biến IR lối vào
  if (state_Entry == LOW && lastState_Entry == HIGH) {
    Serial.println("CAR_ENTRY");
    total += 1;
    delay(1000);
  }

  // Kiểm tra cảm biến IR lối ra
  if (state_Exit == LOW && lastState_Exit == HIGH) {
    Serial.println("CAR_EXIT");
    total -= 1;
    delay(1000);
  }

  // Cập nhật số xe bãi A
  if (state_InA == LOW && lastState_InA == HIGH) {
    total_A += 1;
    delay(100);
  }
  if (state_OutA == LOW && lastState_OutA == HIGH) {
    total_A -= 1;
    delay(100);
  }

  // Cập nhật số xe bãi B
  if (state_InB == LOW && lastState_InB == HIGH) {
    total_B += 1;
    delay(100);
  }
  if (state_OutB == LOW && lastState_OutB == HIGH) {
    total_B -= 1;
    delay(100);
  }

  // Nhận lệnh từ Python
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "RUN_SERVO1") {
      servo1.write(90);
      delay(5000);
      servo1.write(0);
      Serial.println("SERVO1_DONE");
    }
    else if (command == "RUN_SERVO2") {
      servo2.write(90);
      delay(5000);
      servo2.write(0);
      Serial.println("SERVO2_DONE");
    }
  }

  // Cập nhật trạng thái trước
  lastState_Entry = state_Entry;
  lastState_Exit = state_Exit;
  lastState_InA = state_InA;
  lastState_OutA = state_OutA;
  lastState_InB = state_InB;
  lastState_OutB = state_OutB;

  // Cập nhật LCD và LED
  updateLCD();
  updateLED();

  // In Serial Monitor
  Serial.print("Total: ");
  Serial.print(total);
  Serial.print(" | Total A: ");
  Serial.print(total_A);
  Serial.print(" | Total B: ");
  Serial.println(total_B);

  delay(100);
}
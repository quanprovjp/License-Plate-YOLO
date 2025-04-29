#include <ESP32Servo.h>

// Định nghĩa chân GPIO cho servo
#define SERVO1_PIN 21  // Servo lối vào
#define SERVO2_PIN 18  // Servo lối ra

// Định nghĩa chân GPIO cho cảm biến hồng ngoại (IR)
#define IR_SENSOR_ENTRY 15  // IR lối vào
#define IR_SENSOR_EXIT  16  // IR lối ra

Servo servo1;
Servo servo2;

void setup() {
  Serial.begin(115200);
  
  // Cấu hình servo
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);
  servo1.write(0);  // Đóng barrier lối vào
  servo2.write(0);  // Đóng barrier lối ra

  // Cấu hình chân cảm biến IR
  pinMode(IR_SENSOR_ENTRY, INPUT);
  pinMode(IR_SENSOR_EXIT, INPUT);
}

void loop() {
  // Kiểm tra cảm biến IR lối vào
  if (digitalRead(IR_SENSOR_ENTRY) == LOW) { // LOW khi vật chắn IR
    Serial.println("CAR_ENTRY");
    delay(1000);  // Tránh gửi tín hiệu liên tục
  }

  // Kiểm tra cảm biến IR lối ra
  if (digitalRead(IR_SENSOR_EXIT) == LOW) {
    Serial.println("CAR_EXIT");
    delay(1000);
  }

  // Nhận lệnh từ Python để điều khiển servo
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "RUN_SERVO1") {
      servo1.write(90);    // Mở barrier lối vào
      delay(5000);         // Giữ mở 5 giây
      servo1.write(0);     // Đóng barrier
      Serial.println("SERVO1_DONE");
    }
    else if (command == "RUN_SERVO2") {
      servo2.write(90);    // Mở barrier lối ra
      delay(5000);         // Giữ mở 5 giây
      servo2.write(0);     // Đóng barrier
      Serial.println("SERVO2_DONE");
    }
  }
}

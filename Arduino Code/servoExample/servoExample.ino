#include <ESP32Servo.h>

// Pin connected to the servo signal wire
const int servoPin = 6;

// Create a Servo object
Servo myServo;

void setup() {
  // Attach the servo to the pin (with default range of 0° to 180°)
  myServo.attach(servoPin);

  // Move the servo to the initial position (90°)
  myServo.write(90);  // Set to middle position
  delay(1000);  // Wait for the servo to move
}

void loop() {
  // Sweep the servo from 0° to 180°
  for (int angle = 0; angle <= 180; angle += 1) {
    myServo.write(angle);  // Set servo position
    delay(15);  // Wait 15ms for the servo to move
  }
  delay(1000);

  // Sweep the servo back from 180° to 0°
  for (int angle = 180; angle >= 0; angle -= 1) {
    myServo.write(angle);  // Set servo position
    delay(15);  // Wait 15ms for the servo to move
  }
  delay(1000);
}

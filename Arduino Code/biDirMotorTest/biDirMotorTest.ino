#include <ESP32Servo.h>           // Servo and ESC (brushless motor) control
Servo escWeapon; 

void setup() {

  Serial.begin(115200);
  Serial.printf("Hello I am a combat robot SW: %s\n", 1);
  //escWeapon.attach(5, 1000, 2000);
  //escWeapon.writeMicroseconds(1500);
  //delay(3000);
  Serial.printf("armed");


}

void loop() {
  //escWeapon.writeMicroseconds(1600);
  delay(3000);
  //escWeapon.writeMicroseconds(1500);
  //delay(3000);
  //SescWeapon.writeMicroseconds(1400);
  //delay(3000);
}

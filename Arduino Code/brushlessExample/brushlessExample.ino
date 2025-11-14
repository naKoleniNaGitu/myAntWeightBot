#include <ESP32Servo.h>

// Pin connected to the ESC signal wire
const int escPin = 4;

// Create a Servo object to control the ESC
Servo esc;

void setup() {
  // Attach the ESC to the pin (50Hz for ESCs)
  esc.attach(escPin, 1000, 2000);  // min and max pulse width in microseconds (1ms to 2ms)
  
  // Start the ESC with minimum throttle to arm it
  setThrottle(0);  // 0% throttle
  delay(5000);     // Wait 3 seconds to let the ESC arm
}

void loop() {
  // Gradually increase throttle to 100%
  for (int i = 0; i <= 100; i++) {
    setThrottle(i);  // Set throttle percentage
    delay(50);
  }
  delay(10000);

  // Gradually decrease throttle back to 0%
  for (int i = 100; i >= 0; i--) {
    setThrottle(i);  // Set throttle percentage
    delay(50);
  }

  delay(2000);  // Pause before repeating
}

// Function to set the throttle of the ESC (0 to 100%)
void setThrottle(int throttlePercentage) {
  int pulseWidth = map(throttlePercentage, 0, 100, 1000, 2000);  // Map percentage to pulse width
  esc.writeMicroseconds(pulseWidth);  // Set ESC pulse width
}

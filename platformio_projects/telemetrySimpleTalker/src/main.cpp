#include <Arduino.h>

const int ADC_PIN = 6;          // JP14 -> GPIO6
const int MOSFET_CTRL_PIN = 18; // JP14 gate control via Q1 (2N7000)
const int ADC_SAMPLES = 20;     // Number of ADC readings for better stability

// Divider resistor values (Ohms)
const float R_TOP = 33000.0;
const float R_BOTTOM = 7500.0;

// put function declarations here:
// int myFunction(int, int);

void setup() {
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  pinMode(MOSFET_CTRL_PIN, OUTPUT);
  digitalWrite(MOSFET_CTRL_PIN, LOW); // Divider OFF initially

  //set the resolution to 12 bits (0-4095)
  analogReadResolution(12);
}

void loop() {

  digitalWrite(MOSFET_CTRL_PIN, HIGH);
  delay(5); // Wait for RC filter to settle (5 ms)
  // read the analog / millivolts value for pin 2:
  int analogValue = analogRead(ADC_PIN);
  int analogVolts = analogReadMilliVolts(ADC_PIN);

  // Take multiple samples for stability
  int sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogReadMilliVolts(ADC_PIN);
  }
  int analogVoltsStable = sum / ADC_SAMPLES;




  float batteryVoltage = (analogVolts*(R_TOP+R_BOTTOM))/R_BOTTOM;
  float batteryVoltageStable = (analogVoltsStable*(R_TOP+R_BOTTOM))/R_BOTTOM;

  // print out the values you read:
  Serial.printf("ADC analog value = %d\n", analogValue);
  Serial.printf("ADC millivolts value = %d\n", analogVolts);
  Serial.printf("ADC millivolts stable value = %d\n", analogVoltsStable);
  Serial.printf("Battery voltage value = %.2f\n", batteryVoltage);
  Serial.printf("Battery voltage stable value = %.2f\n", batteryVoltageStable);
  Serial.println();

  // 3️⃣ Disable the divider again to save power
  digitalWrite(MOSFET_CTRL_PIN, LOW);

  delay(100);  // delay in between reads for clear read from serial
}

// // put function definitions here:
// int myFunction(int x, int y) {
//   return x + y;
// }
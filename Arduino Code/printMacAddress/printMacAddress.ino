#include "WiFi.h"

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA); // Set WiFi mode to station
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress()); // Print the MAC address
}

void loop() {
  // Empty loop
}
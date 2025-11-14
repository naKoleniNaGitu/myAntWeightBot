#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include <SSD1306Wire.h>

// --- Replace with actual MAC addresses of your two talkers ---
uint8_t TALKER1_MAC[] = {0x24, 0xEC, 0x4A, 0x30, 0x61, 0x70};
uint8_t TALKER2_MAC[] = {0x7C, 0xDF, 0xA1, 0x65, 0x43, 0x21};

// --- Global state ---
float value_talker1 = 0;
float value_talker2 = 0;
unsigned long lastMsg_talker1 = 0;
unsigned long lastMsg_talker2 = 0;

// --- Message structure ---
typedef struct struct_message {
  float value;
} struct_message;

struct_message incomingData;


// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, 2, 1);   // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h e.g. https://github.com/esp8266/Arduino/blob/master/variants/nodemcu/pins_arduino.h


void updateDisplayValue() {
  display.clear();

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  if (value_talker1==-100) {
    display.drawString(0, 0, "SSB: NC");
  }
  else {
    display.drawString(0, 0, "SSB: " + String(value_talker1, 2));
  }
  display.drawString(0, 25, "UFO: NC");
  
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 54, String(millis()));
  display.display();

}


// ✅ New-STYLE CALLBACK (Arduino core 3.x compatible)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  
  // Identify sender
  if (memcmp(info->src_addr, TALKER1_MAC, 6) == 0) {
    value_talker1 = incomingData.value;
    lastMsg_talker1 = millis();
  } else if (memcmp(info->src_addr, TALKER2_MAC, 6) == 0) {
    value_talker2 = incomingData.value;
    lastMsg_talker2 = millis();
  }

  // Serial.print("Received from: ");
  // for (int i = 0; i < 6; i++) {
  //   Serial.printf("%02X", mac[i]);
  //   if (i < 5) Serial.print(":");
  // }
  // Serial.printf(" -> %.2f\n", incomingData.value);

  updateDisplayValue();
}
void setup() {
  Serial.begin(115200);

  // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();              // Important: prevent connection to any AP

  // esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // Use channel 1 for all devices
  // Serial.println(WiFi.macAddress());
  // esp_wifi_set_ps(WIFI_PS_NONE);  // Disable Wi-Fi power saving
  WiFi.setSleep(WIFI_PS_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // ✅ Register old-style callback
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Listener ready. Waiting for messages...");
}

void loop() {
  unsigned long now = millis();

  if (now - lastMsg_talker1 > 5000) value_talker1 = -100;
  if (now - lastMsg_talker2 > 5000) value_talker2 = -100;

  static unsigned long lastPrint = 0;
  if (now - lastPrint > 1000) {
    lastPrint = now;
    updateDisplayValue();
  }
}

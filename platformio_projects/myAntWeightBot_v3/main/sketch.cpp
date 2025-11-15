// myAntWeightBot is based on a Bluepad32 template.
// https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template?tab=readme-ov-file
// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"

#include <Arduino.h>
#include <Bluepad32.h>                      // Bluetooth controller and BL stack
#include <math.h>                           // Math functions i.e. for smoothing filter
#include <ESP32Servo.h>                     // Servo and ESC (brushless motor) control
#include <SparkFun_TB6612.h>                // DC motor control
#include <WiFi.h>                           // For telemetry talker
#include <esp_now.h>                        // For telemetry talker


// Battery measurement configuration
const int ADC_PIN = 6;                      // GPIO6
const int MOSFET_CTRL_PIN = 18;             // GPIO18 gate control MOSFET transistors
const int ADC_SAMPLES = 10;                 // Number of ADC readings for better stability
const int RC_FILTER_DELAY_MS = 5;           // Number of ADC readings for better stability
const int N_CELLS = 3;                      // Number of battery cells in series (designed for 3S 11.4V)

// Divider resistor values (Ohms)
const float R_TOP = 33000.0;
const float R_BOTTOM = 7500.0;

// Telemetry talker configuration
const int TELEMETRY_UPDATE_PERIOD_MS = 1000;
uint8_t LISTENER_MAC[] = {0x34, 0xB7, 0xDA, 0x5B, 0x6F, 0xA8};  // Listener's MAC address

// Telemetry Talker global variables
typedef struct telemetry_struct_message {
  float value;
} telemetry_struct_message;

telemetry_struct_message outgoingData;
esp_now_peer_info_t peerInfo;
unsigned long lastMsgTalker = 0;



ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// Function declarations
void batteryMeasurementSetup(void);
float batteryMeasurementSingleCell(void);
void telemtryOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void telemetrySetup();
void telemetrySend();


void batteryMeasurementSetup() {
    pinMode(MOSFET_CTRL_PIN, OUTPUT);       // Control pin 
    digitalWrite(MOSFET_CTRL_PIN, LOW);     // Turn Off measurement circuit initially
    analogReadResolution(12);               // Set the resolution to 12 bits (0-4095)
}

float batteryMeasurementSingleCell() {
    digitalWrite(MOSFET_CTRL_PIN, HIGH);    // Turn On measurement circuit
    delay(RC_FILTER_DELAY_MS);              // Wait for RC filter to settle (5 ms)

                                            // Take multiple ADC samples for stability
    int sum = 0;
        for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogReadMilliVolts(ADC_PIN);
    }
    digitalWrite(MOSFET_CTRL_PIN, LOW);     // Turn Off measurement circuit to save power

    float adcVoltageStable = sum / ADC_SAMPLES;
    float batteryVoltageStable = (adcVoltageStable*(R_TOP+R_BOTTOM))/R_BOTTOM;
    float cellVoltage = batteryVoltageStable / N_CELLS;

    // print out the values you read:
    Console.printf("ADC voltage = %.2fmV\n", adcVoltageStable);
    Console.printf("Battery voltage = %.2fmV\n", batteryVoltageStable);
    Console.printf("Cell voltage = %.2fmV\n", cellVoltage);

    return cellVoltage / 1000;
}

void telemtryOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Console.print("Send Status: ");
    Console.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void telemetrySetup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();                      // Important: prevent connection to any AP
    WiFi.setSleep(false);                   // No power save mode
    delay(100);                             // Delay due to compatibility
    Console.println(WiFi.macAddress());     // Print out MAC address of the talker

    if (esp_now_init() != ESP_OK) {
        Console.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_send_cb(telemtryOnDataSent);

    memcpy(peerInfo.peer_addr, LISTENER_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Console.println("Failed to add peer");
        return;
    }
    Console.println("Telemetry talker ready!");
}

void telemetrySend() {
    // Send telemetry data every TELEMETRY_UPDATE_PERIOD_MS
    if (lastMsgTalker+TELEMETRY_UPDATE_PERIOD_MS <= millis()) {
        lastMsgTalker = millis();           // Update timestamp
        outgoingData.value = batteryMeasurementSingleCell(); 

        esp_err_t result = esp_now_send(LISTENER_MAC, (uint8_t *)&outgoingData, sizeof(outgoingData));

        if (result == ESP_OK) {
            Console.printf("Sent value %.2f\n", outgoingData.value);
        } else {
            Console.println("Error sending data");
        }
    }
}

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Console.printf("CALLBACK: Controller is connected, index=%d\n", i);
            // Additionally, you can get certain gamepad properties like:
            // Model, VID, PID, BTAddr, flags, etc.
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Console.println("CALLBACK: Controller connected, but could not found empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Console.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }

    if (!foundController) {
        Console.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void dumpGamepad(ControllerPtr ctl) {
    Console.printf(
        "idx=%d, dpad: 0x%02x, buttons: 0x%04x, axis L: %4d, %4d, axis R: %4d, %4d, brake: %4d, throttle: %4d, "
        "misc: 0x%02x, gyro x:%6d y:%6d z:%6d, accel x:%6d y:%6d z:%6d\n",
        ctl->index(),        // Controller Index
        ctl->dpad(),         // D-pad
        ctl->buttons(),      // bitmask of pressed buttons
        ctl->axisX(),        // (-511 - 512) left X Axis
        ctl->axisY(),        // (-511 - 512) left Y axis
        ctl->axisRX(),       // (-511 - 512) right X axis
        ctl->axisRY(),       // (-511 - 512) right Y axis
        ctl->brake(),        // (0 - 1023): brake button
        ctl->throttle(),     // (0 - 1023): throttle (AKA gas) button
        ctl->miscButtons(),  // bitmask of pressed "misc" buttons
        ctl->gyroX(),        // Gyro X
        ctl->gyroY(),        // Gyro Y
        ctl->gyroZ(),        // Gyro Z
        ctl->accelX(),       // Accelerometer X
        ctl->accelY(),       // Accelerometer Y
        ctl->accelZ()        // Accelerometer Z
    );
}

void dumpMouse(ControllerPtr ctl) {
    Console.printf("idx=%d, buttons: 0x%04x, scrollWheel=0x%04x, delta X: %4d, delta Y: %4d\n",
                   ctl->index(),        // Controller Index
                   ctl->buttons(),      // bitmask of pressed buttons
                   ctl->scrollWheel(),  // Scroll Wheel
                   ctl->deltaX(),       // (-511 - 512) left X Axis
                   ctl->deltaY()        // (-511 - 512) left Y axis
    );
}

void dumpKeyboard(ControllerPtr ctl) {
    static const char* key_names[] = {
        // clang-format off
        // To avoid having too much noise in this file, only a few keys are mapped to strings.
        // Starts with "A", which is offset 4.
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
        "W", "X", "Y", "Z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
        // Special keys
        "Enter", "Escape", "Backspace", "Tab", "Spacebar", "Underscore", "Equal", "OpenBracket", "CloseBracket",
        "Backslash", "Tilde", "SemiColon", "Quote", "GraveAccent", "Comma", "Dot", "Slash", "CapsLock",
        // Function keys
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        // Cursors and others
        "PrintScreen", "ScrollLock", "Pause", "Insert", "Home", "PageUp", "Delete", "End", "PageDown",
        "RightArrow", "LeftArrow", "DownArrow", "UpArrow",
        // clang-format on
    };
    static const char* modifier_names[] = {
        // clang-format off
        // From 0xe0 to 0xe7
        "Left Control", "Left Shift", "Left Alt", "Left Meta",
        "Right Control", "Right Shift", "Right Alt", "Right Meta",
        // clang-format on
    };
    Console.printf("idx=%d, Pressed keys: ", ctl->index());
    for (int key = Keyboard_A; key <= Keyboard_UpArrow; key++) {
        if (ctl->isKeyPressed(static_cast<KeyboardKey>(key))) {
            const char* keyName = key_names[key - 4];
            Console.printf("%s,", keyName);
        }
    }
    for (int key = Keyboard_LeftControl; key <= Keyboard_RightMeta; key++) {
        if (ctl->isKeyPressed(static_cast<KeyboardKey>(key))) {
            const char* keyName = modifier_names[key - 0xe0];
            Console.printf("%s,", keyName);
        }
    }
    Console.printf("\n");
}

void dumpBalanceBoard(ControllerPtr ctl) {
    Console.printf("idx=%d,  TL=%u, TR=%u, BL=%u, BR=%u, temperature=%d\n",
                   ctl->index(),        // Controller Index
                   ctl->topLeft(),      // top-left scale
                   ctl->topRight(),     // top-right scale
                   ctl->bottomLeft(),   // bottom-left scale
                   ctl->bottomRight(),  // bottom-right scale
                   ctl->temperature()   // temperature: used to adjust the scale value's precision
    );
}

void processGamepad(ControllerPtr ctl) {
    // There are different ways to query whether a button is pressed.
    // By query each button individually:
    //  a(), b(), x(), y(), l1(), etc...
    if (ctl->a()) {
        static int colorIdx = 0;
        // Some gamepads like DS4 and DualSense support changing the color LED.
        // It is possible to change it by calling:
        switch (colorIdx % 3) {
            case 0:
                // Red
                ctl->setColorLED(255, 0, 0);
                break;
            case 1:
                // Green
                ctl->setColorLED(0, 255, 0);
                break;
            case 2:
                // Blue
                ctl->setColorLED(0, 0, 255);
                break;
        }
        colorIdx++;
    }

    if (ctl->b()) {
        // Turn on the 4 LED. Each bit represents one LED.
        static int led = 0;
        led++;
        // Some gamepads like the DS3, DualSense, Nintendo Wii, Nintendo Switch
        // support changing the "Player LEDs": those 4 LEDs that usually indicate
        // the "gamepad seat".
        // It is possible to change them by calling:
        ctl->setPlayerLEDs(led & 0x0f);
    }

    if (ctl->x()) {
        // Some gamepads like DS3, DS4, DualSense, Switch, Xbox One S, Stadia support rumble.
        // It is possible to set it by calling:
        // Some controllers have two motors: "strong motor", "weak motor".
        // It is possible to control them independently.
        ctl->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
    }

    // Another way to query controller data is by getting the buttons() function.
    // See how the different "dump*" functions dump the Controller info.
    dumpGamepad(ctl);

    // See ArduinoController.h for all the available functions.
}

void processMouse(ControllerPtr ctl) {
    // This is just an example.
    if (ctl->scrollWheel() > 0) {
        // Do Something
    } else if (ctl->scrollWheel() < 0) {
        // Do something else
    }

    // See "dumpMouse" for possible things to query.
    dumpMouse(ctl);
}

void processKeyboard(ControllerPtr ctl) {
    if (!ctl->isAnyKeyPressed())
        return;

    // This is just an example.
    if (ctl->isKeyPressed(Keyboard_A)) {
        // Do Something
        Console.println("Key 'A' pressed");
    }

    // Don't do "else" here.
    // Multiple keys can be pressed at the same time.
    if (ctl->isKeyPressed(Keyboard_LeftShift)) {
        // Do something else
        Console.println("Key 'LEFT SHIFT' pressed");
    }

    // Don't do "else" here.
    // Multiple keys can be pressed at the same time.
    if (ctl->isKeyPressed(Keyboard_LeftArrow)) {
        // Do something else
        Console.println("Key 'Left Arrow' pressed");
    }

    // See "dumpKeyboard" for possible things to query.
    dumpKeyboard(ctl);
}

void processBalanceBoard(ControllerPtr ctl) {
    // This is just an example.
    if (ctl->topLeft() > 10000) {
        // Do Something
    }

    // See "dumpBalanceBoard" for possible things to query.
    dumpBalanceBoard(ctl);
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else if (myController->isMouse()) {
                processMouse(myController);
            } else if (myController->isKeyboard()) {
                processKeyboard(myController);
            } else if (myController->isBalanceBoard()) {
                processBalanceBoard(myController);
            } else {
                Console.printf("Unsupported controller\n");
            }
        }
    }
}

// Arduino setup function. Runs in CPU 1
void setup() {
    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // Setup the Bluepad32 callbacks, and the default behavior for scanning or not.
    // By default, if the "startScanning" parameter is not passed, it will do the "start scanning".
    // Notice that "Start scanning" will try to auto-connect to devices that are compatible with Bluepad32.
    // E.g: if a Gamepad, keyboard or mouse are detected, it will try to auto connect to them.
    bool startScanning = true;
    BP32.setup(&onConnectedController, &onDisconnectedController, startScanning);

    // Notice that scanning can be stopped / started at any time by calling:
    // BP32.enableNewBluetoothConnections(enabled);

    // "forgetBluetoothKeys()" should be called when the user performs
    // a "device factory reset", or similar.
    // Calling "forgetBluetoothKeys" in setup() just as an example.
    // Forgetting Bluetooth keys prevents "paired" gamepads to reconnect.
    // But it might also fix some connection / re-connection issues.
    BP32.forgetBluetoothKeys();

    // Enables mouse / touchpad support for gamepads that support them.
    // When enabled, controllers like DualSense and DualShock4 generate two connected devices:
    // - First one: the gamepad
    // - Second one, which is a "virtual device", is a mouse.
    // By default, it is disabled.
    BP32.enableVirtualDevice(false);

    // Enables the BLE Service in Bluepad32.
    // This service allows clients, like a mobile app, to setup and see the state of Bluepad32.
    // By default, it is disabled.
    BP32.enableBLEService(false);

    delay(200);                             // Delay for BLE and ESP-Now compatibility
    telemetrySetup();                       // Initial telemetry setup
    batteryMeasurementSetup();              // Initial battery measurement setup
    batteryMeasurementSingleCell();         // Measurement test (only in console)
}

// Arduino loop function. Runs in CPU 1.
void loop() {
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();

    // The main loop must have some kind of "yield to lower priority task" event.
    // Otherwise, the watchdog will get triggered.
    // If your main loop doesn't have one, just add a simple `vTaskDelay(1)`.
    // Detailed info here:
    // https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time

    //     vTaskDelay(1);
    delay(150);

    telemetrySend();                        // Call telemetry
}

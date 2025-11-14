#include <Bluepad32.h>
#include <math.h>
#include <ESP32Servo.h>


#define BOT_MAX_GAMEPADS 1 
#define MIN_PROPULSION_SPEED 0
#define MAX_PROPULSION_SPEED 254
#define MIN_PROPULSION_SPEED_MARGIN 30
#define MAX_GAMEPAD_SPEED 512
#define SCALE_FACTOR MAX_PROPULSION_SPEED/MAX_GAMEPAD_SPEED  // Scale down the input range from [-512, 512] to [-255, 255] for motor control

#define LEFT_MOTOR_CTR_PIN_1 1
#define LEFT_MOTOR_CTR_PIN_2 2
#define RIGHT_MOTOR_CTR_PIN_1 3
#define RIGHT_MOTOR_CTR_PIN_2 4
#define WEAPON_MOTOR_CTR_PIN 6
#define SERVO_CTR_PIN 5

// Create a Servo object to control the ESC and servo
Servo esc;
Servo myServo;

int disabled = 1;


#define PROPULSION_SMOOTHING_FACTOR 0.5

int axisY = 0;  // Forward and Bakward joystick movement
int axisX = 0;  // Left and Right joystick movement
int previousAxisY = 0;
int previousAxisX = 0;
int filteredAxisY = 0;
int filteredAxisX = 0;

int throttleRight = 0;
int throttleLeft = 0;

ControllerPtr myControllers[BOT_MAX_GAMEPADS];



/**
 * Exponential Smoothing
 * Function to filter input values.
 * 
 * @param[in] currentDataSample .
 * @param[in] previousFilteredResult .
 * @param[in] alpha .
 * @param[out] limitedSpeed Speed limited into valid range.
 *
 */
int exponentialSmoothing(int currentDataSample, int previousFilteredResult, float alpha) {
  // Apply exponential smoothing formula
  float filtered = alpha * currentDataSample + (1.0 - alpha) * previousFilteredResult;
  return (int)filtered;
}

/**
 * Smooth Gamepad
 * Function to filter input values.

 *
 */
void smoothGamepad(void) {
  // Apply exponential smoothing formula

  // Smooth Y axis gamepad
  filteredAxisY = exponentialSmoothing(axisY,previousAxisY, PROPULSION_SMOOTHING_FACTOR);
  // Smooth X axis gamepad
  filteredAxisX = exponentialSmoothing(axisX,previousAxisX, PROPULSION_SMOOTHING_FACTOR);

  previousAxisY = filteredAxisY;
  previousAxisX = filteredAxisX;


}



/**
 * Limit Speed
 * Function to limit calculated motor speed to correct range.
 * 
 * @param[in] speed Calculated speed of rotation.
 * @param[out] limitedSpeed Speed limited into valid range.
 *
 */
uint8_t limitSpeed(float inputSpeed) {
  uint8_t limitedSpeed = 0;
  if (inputSpeed <= MIN_PROPULSION_SPEED + MIN_PROPULSION_SPEED_MARGIN)
  {
    limitedSpeed = MIN_PROPULSION_SPEED;
  }
  else if (inputSpeed >= MAX_PROPULSION_SPEED)
  {
    limitedSpeed = MAX_PROPULSION_SPEED;
  }
  else
  {
    limitedSpeed = (int)inputSpeed;
  }
  return limitedSpeed;
}

/**
 * Move Motor
 * Function to move a motor with defined speed and direction using TC1508A module with two control inputs per controled motor.
 * 
 * @param[in] ctrPin1 Control output pin of the ESP32 board connected to INT1.
 * @param[in] ctrPin2 Control output pin of the ESP32 board connected to INT2.
 * @param[in] speed Calculated speed of rotation.
 *
 */
void moveMotor(uint8_t ctrPin1, uint8_t ctrPin2, float speed)
{
  uint8_t speedToSet = 0;

  // Limit calculated speed value
  speedToSet = limitSpeed(fabs(speed));

  // Get direction 
  if (speed > 0)
  {
    // Forward
    analogWrite(ctrPin1, speedToSet);
    digitalWrite(ctrPin2, LOW);

    Serial.print(speedToSet);
    Serial.print(",");
  } else if (speed < 0)
  {
    // Backward
    digitalWrite(ctrPin1, LOW);
    analogWrite(ctrPin2, speedToSet);

    Serial.print(speedToSet*(-1));
    Serial.print(",");
  } else
  {
    // Backward
    digitalWrite(ctrPin1, LOW);
    digitalWrite(ctrPin2, LOW);

    Serial.print(0);
    Serial.print(",");
  }
  
}

/**
 * Move Robot
 * Function to move robot based on commands from gamepad.
 * 
 * @param[in] _axisX Gamepad controller input. Left and Right joystick movement.
 * @param[in] _axisY Gamepad controller input. Forward and Bakward joystick movement.
 *
 */
void moveRobot(int _axisX, int _axisY) 
{
  // Mix Y-axis (forward/reverse) and X-axis (turning/rotation)
  float right_speed = -((_axisY + _axisX) * SCALE_FACTOR);
  float left_speed = ((_axisY - _axisX) * SCALE_FACTOR);

  //Serial.print("left_speed:");
  //Serial.print(left_speed);
  //Serial.print(",");
  //Serial.print("right_speed:");
  //Serial.print(right_speed);
  //Serial.print(",");

  //Serial.print("left_speed_to_set:");
  moveMotor(LEFT_MOTOR_CTR_PIN_1, LEFT_MOTOR_CTR_PIN_2, left_speed);
  //Serial.print("right_speed_to_set:");
  moveMotor(RIGHT_MOTOR_CTR_PIN_1, RIGHT_MOTOR_CTR_PIN_2, right_speed);
}

/**
 * Collect Controller
 * Function to collect gamepad status into global variables.
 * 
 * @param[in] ctl Gamepad controller input.
 *
 */
void collectController(ControllerPtr ctl) 
{
  axisY = ctl->axisY();  // Forward and Bakward joystick movement
  axisX = ctl->axisRX();  // Left and Right joystick movement
  throttleRight = ctl->throttle();
  throttleLeft = ctl->brake();

}

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BOT_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
            // Additionally, you can get certain gamepad properties like:
            // Model, VID, PID, BTAddr, flags, etc.
            ControllerProperties properties = ctl->getProperties();
            Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Serial.println("CALLBACK: Controller connected, but could not found empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;
    disabled = 0;

    for (int i = 0; i < BOT_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }

    if (!foundController) {
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void dumpGamepad(ControllerPtr ctl) {
    Serial.printf(
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

void plotGamepad(ControllerPtr ctl) {
  //ctl->axisX(),        // (-511 - 512) left X Axis
  //      ctl->axisY(),        // (-511 - 512) left Y axis
  
  Serial.print("throttle_right:");
  Serial.print(ctl->throttle());
  Serial.print("throttle_left:");
  Serial.print(ctl->brake());
  Serial.print("Move_x:");
  Serial.print(ctl->axisX());
  Serial.print(",");
  Serial.print("Move_y:");
  Serial.println(ctl->axisY());
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
    // dumpGamepad(ctl);
    collectController(ctl); 
    plotGamepad(ctl);
}


void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else {
                Serial.println("Unsupported controller");
            }
        }
    }
}

// Function to set the throttle of the ESC (0 to 1024)
void setThrottle(int throttlePercentage) {
  int pulseWidth = map(throttlePercentage, 0, 1024, 1000, 2000);  // Map percentage to pulse width
  esc.writeMicroseconds(pulseWidth);  // Set ESC pulse width
}

// Function to set the servo position based on analog input (0 to 1023)
void setServoPosition(int analogValue) {
  int angle = map(analogValue, 0, 1023, 0, 180);
  // Move the servo to the corresponding angle
  myServo.write(angle);
}

// Arduino setup function. Runs in CPU 1
void setup() {

  // Propulsion motor output control pins setup
  pinMode(LEFT_MOTOR_CTR_PIN_1, OUTPUT);
  pinMode(LEFT_MOTOR_CTR_PIN_2, OUTPUT);
  pinMode(RIGHT_MOTOR_CTR_PIN_1, OUTPUT);
  pinMode(RIGHT_MOTOR_CTR_PIN_2, OUTPUT);

  // ESC Electronic Speed COntroller
  // Attach the ESC to the pin (50Hz for ESCs)
  esc.attach(WEAPON_MOTOR_CTR_PIN, 1000, 2000);  // min and max pulse width in microseconds (1ms to 2ms)
  
  // Start the ESC with minimum throttle to arm it
  setThrottle(0);  // 0% throttle
  delay(3000);     // Wait 3 seconds to let the ESC arm

  // Attach the servo to the pin (with default range of 0° to 180°)
  myServo.attach(SERVO_CTR_PIN);

  // Move the servo to the initial position (90°)
  myServo.write(90);  // Set to middle position
  delay(1000);  // Wait for the servo to move


  

  // Debug Serial interface setup
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t* addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);

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
}

// Arduino loop function. Runs in CPU 1.
void loop() {
  // This call fetches all the controllers' data.
  // Call this function in your main loop.
  bool dataUpdated = BP32.update();
  if (dataUpdated)
    processControllers();
  smoothGamepad();
  moveRobot(filteredAxisX, filteredAxisY);
  setThrottle(throttleRight);
  setServoPosition(throttleLeft);
  if (disabled == 0)
  {
    
    while(true)
    {
      moveRobot(0, 0);
      setThrottle(0);
      Serial.print("disabled");
    }
  }


  // The main loop must have some kind of "yield to lower priority task" event.
  // Otherwise, the watchdog will get triggered.
  // If your main loop doesn't have one, just add a simple `vTaskDelay(1)`.
  // Detailed info here:
  // https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time

  //     vTaskDelay(1);
  delay(150);
}

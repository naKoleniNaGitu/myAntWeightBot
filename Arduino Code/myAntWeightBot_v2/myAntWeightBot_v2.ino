#include <Bluepad32.h>            // BT gamepad control 
#include <math.h>                 // Math functions i.e. for smoothing filter
#include <ESP32Servo.h>           // Servo and ESC (brushless motor) control
#include <SparkFun_TB6612.h>      // DC motor control

#define SW_VERSION "v2.0.0"

// Configuration
#define BOT_MAX_GAMEPADS 1 
#define MIN_PROPULSION_SPEED -255
#define MAX_PROPULSION_SPEED 255
#define MIN_PROPULSION_SPEED_MARGIN 50
#define MIN_GAMEPAD_SPEED -512
#define MAX_GAMEPAD_SPEED 512
#define GAMEPAD_ZERO_POSITION_ERROR_MAX 40
#define PROPULSION_SMOOTHING_FACTOR 0.5
#define ESC_WEAPON_MIN_PULSE_WIDTH_US 1000        // Min pulse width in microseconds (1ms)
#define ESC_WEAPON_STOP_PULSE_WIDTH_US 1500  // Mid pulse width (to stop) in microseconds (1ms)
#define ESC_WEAPON_MAX_PULSE_WIDTH_US 2000        // Max pulse width in microseconds (2ms)
#define MOTOR_A_DIRECTION 1  // Value can be 1 or -1
#define MOTOR_B_DIRECTION 1  // Value can be 1 or -1

// GPIO PIN definition
#define MOTOR_AIN1 11  // modra ok
#define MOTOR_BIN1 9  // seda ok
#define MOTOR_AIN2 12  // zelena ok
#define MOTOR_BIN2 8  // bila ok
#define MOTOR_PWMA 13  // oranzova ok
#define MOTOR_PWMB 7  // hneda ok
#define MOTOR_STBY 10  // fialova ok
#define WEAPON_MOTOR_CTR_PIN 5
#define SERVO_CTR_PIN 3

// Global variables
bool gamepadDisconnected = false;
int axisY = 0;  // Forward and Bakward joystick movement
int axisX = 0;  // Left and Right joystick movement
int throttleRight = 0;
int throttleLeft = 0;
int previousAxisY = 0;
int previousAxisX = 0;
int filteredAxisY = 0;
int filteredAxisX = 0;
int speedMotorA = 0;
int speedMotorB = 0;

// Objects
Servo escWeapon;        // ESC (Electronic Speed Controller) for weapon motor 
Servo myServo;          // Servo motor for arm
Motor motorA = Motor(MOTOR_AIN1, MOTOR_AIN2, MOTOR_PWMA, MOTOR_A_DIRECTION, MOTOR_STBY);
Motor motorB = Motor(MOTOR_BIN1, MOTOR_BIN2, MOTOR_PWMB, MOTOR_B_DIRECTION, MOTOR_STBY);
ControllerPtr myControllers[BOT_MAX_GAMEPADS];

/**
 * Exponential Smoothing
 * Function to filter input values.
 * 
 * @param[in] currentDataSample
 * @param[in] previousFilteredResult
 * @param[in] alpha
 *
 */
int exponentialSmoothing(int currentDataSample, int previousFilteredResult, float alpha) {
  // Apply exponential smoothing formula
  float filtered = alpha * currentDataSample + (1.0 - alpha) * previousFilteredResult;
  return (int)filtered;
}

/**
 * Smooth Gamepad
 * Function to filter Gamepad values.
 * @param[in] _axisY Raw (unfiltered) axis Y value.
 * @param[in] _axisX Raw (unfiltered) axis X value.
 * @param[in, out] _previousAxisY Previous filtered axis Y value.
 * @param[in, out] _previousAxisX Previous filtered axis X value.
 * @param[out] _filteredAxisY Filtered axis Y value.
 * @param[out] _filteredAxisX Filtered axis X value.
 *
 */
void smoothGamepad(int _axisY, 
                   int _axisX, 
                   int* _previousAxisY, 
                   int* _previousAxisX, 
                   int* _filteredAxisY, 
                   int* _filteredAxisX) {
  // Apply exponential smoothing formula

  // Smooth Y axis gamepad
  *_filteredAxisY = exponentialSmoothing(_axisY, *_previousAxisY, PROPULSION_SMOOTHING_FACTOR);
  // Smooth X axis gamepad
  *_filteredAxisX = exponentialSmoothing(_axisX, *_previousAxisX, PROPULSION_SMOOTHING_FACTOR);

  *_previousAxisY = *_filteredAxisY;
  *_previousAxisX = *_filteredAxisX;
}

/**
 * Calculate motor speed
 * Function to calculate X,Y gamepad inputs to speed of differential stearing motors.
 *
 * @param[in] _axisX Gamepad controller input. Left and Right joystick movement.
 * @param[in] _axisY Gamepad controller input. Forward and Bakward joystick movement.
 * @param[out] _speedA pointer to speed of motor A.
 * @param[out] _speedB pointer to speed of motor B.
 *
 */
void calculateMotorSpeed(int _axisX, int _axisY, int* _speedA, int* _speedB) 
{
  // Mix Y-axis (forward/reverse) and X-axis (turning/rotation)
  int _calculatedSpeedA = _axisY + _axisX;
  int _calculatedSpeedB = _axisY - _axisX;

  // Limit the calculated values to +-512
  // Needed when using two separate gamepad joysticks
  int _limitedSpeedA = constrain(_calculatedSpeedA, MIN_GAMEPAD_SPEED, MAX_GAMEPAD_SPEED);
  int _limitedSpeedB = constrain(_calculatedSpeedB, MIN_GAMEPAD_SPEED, MAX_GAMEPAD_SPEED);

  // Map the calculated speed values to speed values able to set
  *_speedA = map(_limitedSpeedA, MIN_GAMEPAD_SPEED, MAX_GAMEPAD_SPEED, MIN_PROPULSION_SPEED, MAX_PROPULSION_SPEED);
  *_speedB = map(_limitedSpeedB, MIN_GAMEPAD_SPEED, MAX_GAMEPAD_SPEED, MIN_PROPULSION_SPEED, MAX_PROPULSION_SPEED);
}

/**
 * Move Robot 
 * Function to move robot based on input speed.
 *
 * @param[in] _speedA Speed to set for motor A.
 * @param[in] _speedB Speed to set for motor B.
 *
 */
void moveRobot(int _speedA, int _speedB) 
{
  if (abs(_speedA) > MIN_PROPULSION_SPEED_MARGIN) {
    motorA.drive(_speedA);
    delay(1);  // Minimum delay
  } else {
    motorA.brake();
    delay(1);
  }

  if (abs(_speedB) > MIN_PROPULSION_SPEED_MARGIN) {
    motorB.drive(_speedB);
    delay(1);  // Minimum delay
  } else {
    motorB.brake();
    delay(1);
  }
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
  axisY = (ctl->axisY());  // Forward (positive) and Bakward (negative) joystick movement
  axisX = -(ctl->axisRX());  // Left (positive) and Right (negative) joystick movement
  throttleRight = ctl->throttle();
  throttleLeft = ctl->brake();

  if (abs(axisY) < GAMEPAD_ZERO_POSITION_ERROR_MAX) {
    axisY = 0;  // Limit gamepad joistick 0 position error
  }

  if (abs(axisX) < GAMEPAD_ZERO_POSITION_ERROR_MAX) {
    axisX = 0;  // Limit gamepad joistick 0 position error
  }
}

/**
 * On connected Controller
 * This callback gets called any time a new gamepad is connected.
 * 
 * @param[in] ctl Gamepad controller input.
 *
 */
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

/**
 * On disconnected Controller
 * This callback gets called any time a gamepad is disconnected.
 * 
 * @param[in] ctl Gamepad controller input.
 *
 */
void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BOT_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            gamepadDisconnected = true;  // Any disconected gamepad cause entering to robot safe state
            break;
        }
    }

    if (!foundController) {
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

/**
 * Simple debug function supporting Serial Plotter format.
 */
void plotGlobalVariables() {
  Serial.printf("drive_raw:%d,drive_filtered:%d,", axisY, filteredAxisY);
  Serial.printf("turn_raw:%d,turn_filtered:%d,", axisX, filteredAxisX);
  Serial.printf("servo_trhottle:%d,weapon_trhottle:%d,", throttleLeft, throttleRight);
  Serial.printf("speed_motor_A:%d,speed_motor_B:%d,", speedMotorA, speedMotorB);
  // speedMotorA = 0;
  // speedMotorB = 0;

  Serial.println();
}



void processGamepad(ControllerPtr ctl) {

  if (ctl->x()) {
    // Example rumble to test that the gamepad is connected
    ctl->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                        0x40 /* strongMagnitude */);
  }
  collectController(ctl); 
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


/**
 * Set weapon throttle
 * Function to set the throttle of the ESC.
 * 
 * @param[in] throttleValue Input value from 0 to 1024 (typically from gamepad).
 *
 */
void setWeaponThrottle(int throttleValue, bool reverse) {
  // Map gamepad to pulse width
  int pulseWidth = ESC_WEAPON_STOP_PULSE_WIDTH_US; 
  if(reverse == true) {
    pulseWidth = map(throttleValue, 0, 1024, ESC_WEAPON_STOP_PULSE_WIDTH_US, ESC_WEAPON_MIN_PULSE_WIDTH_US);
  } else {
    pulseWidth = map(throttleValue, 0, 1024, ESC_WEAPON_STOP_PULSE_WIDTH_US, ESC_WEAPON_MAX_PULSE_WIDTH_US);
  }
  
  escWeapon.writeMicroseconds(pulseWidth);  // Set ESC pulse width
}

/**
 * Set servo position
 * Function to set the servo position based on analog input (0 to 1024).
 * 
 * @param[in] throttleValue Input value from 0 to 1024 (typically from gamepad).
 *
 */
void setServoPosition(int throttleValue) {
  int angle = map(throttleValue, 0, 1024, 0, 180);  // Map gamepad to angle
  myServo.write(angle);                             // Move the servo to the corresponding angle
}

/**
 * Arm weapon ESC
 * Function to arm weapon ESC - ready to be used. It sets throttle to 0%.
 */
void armWeaponEsc() {
  // Start the ESC with minimum throttle to arm it
  delay(3000);           // Wait to let the ESC power up
  escWeapon.writeMicroseconds(ESC_WEAPON_STOP_PULSE_WIDTH_US);  // 0% throttle
  delay(3000);           // Wait 3 seconds to let the ESC arm
}


// Arduino setup function. Runs in CPU 1
void setup() {

  // Debug Serial interface setup
  Serial.begin(115200);
  Serial.printf("Hello I am a combat robot SW: %s\n", SW_VERSION);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t* addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // ESC Electronic Speed Controller
  // Attach the ESC to the pin (50Hz for ESCs)
  escWeapon.attach(WEAPON_MOTOR_CTR_PIN, ESC_WEAPON_MIN_PULSE_WIDTH_US, ESC_WEAPON_MAX_PULSE_WIDTH_US);
  armWeaponEsc();
  Serial.printf("Weapon motor attached to pin %d", WEAPON_MOTOR_CTR_PIN);

  // Attach the servo to the pin (with default range of 0° to 180°)
  //myServo.attach(SERVO_CTR_PIN);

  // Move the servo to the initial position (0°)
  //myServo.write(0);  // Set to middle position
  delay(1000);  // Wait for the servo to move

  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  // "forgetBluetoothKeys()" should be called when the user performs
  // a "device factory reset", or similar.
  // Calling "forgetBluetoothKeys" in setup() just as an example.
  // Forgetting Bluetooth keys prevents "paired" gamepads to reconnect.
  // But it might also fix some connection / re-connection issues.
  // TODO: Move this to interumt routine of a "paring mode" button press
  BP32.forgetBluetoothKeys();
}

// Arduino loop function. Runs in CPU 1.
void loop() {
  // This call fetches all the controllers' data.
  // Call this function in your main loop.
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();

    smoothGamepad(axisY, axisX, &previousAxisY, &previousAxisX, &filteredAxisY, &filteredAxisX);
    calculateMotorSpeed(filteredAxisX, filteredAxisY, &speedMotorA, &speedMotorB);
    moveRobot(speedMotorA, speedMotorB);

    if(throttleRight < 10) {  // safety margin
      setWeaponThrottle(throttleLeft, true);
    } else {
      setWeaponThrottle(throttleRight, false);
    }
    //setServoPosition(throttleLeft);
    
  }
  //plotGlobalVariables();
  // Only debug delay
  delay(100);
  
  
  if (gamepadDisconnected == true) {
    // Safe state - infinite loop
    // If the gamepad is desconnected during robot operation reboot of the bot is necessary.
    while(true)
    {
      moveRobot(0, 0);
      setWeaponThrottle(0, false);
      Serial.print("Gamepad disconnected");
    }
  }

  // The main loop must have some kind of "yield to lower priority task" event.
  // Otherwise, the watchdog will get triggered.
  vTaskDelay(1);
}

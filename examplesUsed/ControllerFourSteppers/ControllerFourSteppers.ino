#include <Bluepad32.h>
#include <AccelStepper.h>

// ====== Hardware configuration ======
// 28BYJ-48 steppers with ULN2003 driver boards (4-coil control).
// This sketch uses AccelStepper HALF4WIRE for smoother half-stepping.

static const int N_MOTORS = 4;
// Choose ESP32 GPIOs that are safe during boot (prefer avoiding 0, 2, 12, 15). Using 4 & 5 as last resort.
// IMPORTANT: AccelStepper expects pins in this order for 4-wire modes: 1,3,2,4
// So for each motor, map as: {IN1, IN3, IN2, IN4}
static const int MOTOR_PINS[N_MOTORS][4] = {
  // M0: IN1, IN3, IN2, IN4
  {14, 26, 25, 27},
  // M1
  {32, 16, 33, 17},
  // M2
  {13, 12, 18, 21}, // freed GPIO19 for relay; note: GPIO12 is a strapping pin, avoid external pulls at boot
  // M3 (uses 22,23 and 4,5; some boards strap these—adjust if boot issues)
  {22, 4, 23, 5}
};

// Motion tuning
static const float MAX_SPEED_STEPS_PER_SEC = 600.0f;  // per motor (28BYJ-48 typical safe range)
static const float DEADZONE = 0.10f;                  // 10% joystick deadzone

// ====== Globals ======
ControllerPtr gControllers[BP32_MAX_GAMEPADS];
AccelStepper steppers[N_MOTORS] = {
  AccelStepper(AccelStepper::HALF4WIRE, MOTOR_PINS[0][0], MOTOR_PINS[0][1], MOTOR_PINS[0][2], MOTOR_PINS[0][3]),
  AccelStepper(AccelStepper::HALF4WIRE, MOTOR_PINS[1][0], MOTOR_PINS[1][1], MOTOR_PINS[1][2], MOTOR_PINS[1][3]),
  AccelStepper(AccelStepper::HALF4WIRE, MOTOR_PINS[2][0], MOTOR_PINS[2][1], MOTOR_PINS[2][2], MOTOR_PINS[2][3]),
  AccelStepper(AccelStepper::HALF4WIRE, MOTOR_PINS[3][0], MOTOR_PINS[3][1], MOTOR_PINS[3][2], MOTOR_PINS[3][3])
};

bool motorsEnabled = true; // controlled via gamepad 'A' (no dedicated EN on ULN2003)

// Relay control to cut motor power when idle
static const int RELAY_PIN = 19;              // default relay control pin (active HIGH by default)
static const bool RELAY_ACTIVE_HIGH = true;   // set to false if your relay module is active-LOW
static const unsigned long RELAY_IDLE_OFF_MS = 3000; // turn off power after this many ms of idle
static bool relayOn = false;
static unsigned long lastActiveMs = 0;

// ====== Helpers ======
static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline float axisToNorm(int v) {
  // Bluepad32 axes are roughly in [-511, 512]
  float f = (float)clampf(v, -512, 512) / 512.0f;
  // In many controllers, pushing stick up yields negative values: invert so up = +
  return -f;
}

static void applyEnablePin() {
  // ULN2003 boards typically have no enable. We keep this function for compatibility.
}

static void setAllSpeeds(float s0, float s1, float s2, float s3) {
  steppers[0].setSpeed(s0);
  steppers[1].setSpeed(s1);
  steppers[2].setSpeed(s2);
  steppers[3].setSpeed(s3);
}

static void stopAll() {
  setAllSpeeds(0, 0, 0, 0);
}

static void setRelay(bool on) {
  if (RELAY_PIN < 0) return;
  if (!relayOn && on) {
    // turning ON
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_HIGH ? HIGH : LOW);
    relayOn = true;
  } else if (relayOn && !on) {
    // turning OFF
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_HIGH ? LOW : HIGH);
    relayOn = false;
  }
}

// ====== Bluepad32 callbacks ======
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (gControllers[i] == nullptr) {
      gControllers[i] = ctl;
      ControllerProperties p = ctl->getProperties();
      Serial.printf("CALLBACK: Controller connected idx=%d, model=%s, VID=0x%04x, PID=0x%04x\n",
                    i, ctl->getModelName().c_str(), p.vendor_id, p.product_id);
      return;
    }
  }
  Serial.println("CALLBACK: Controller connected but no free slot");
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (gControllers[i] == ctl) {
      gControllers[i] = nullptr;
      Serial.printf("CALLBACK: Controller disconnected idx=%d\n", i);
      return;
    }
  }
  Serial.println("CALLBACK: Controller disconnected but not found");
}

// ====== Controller processing ======
void processGamepad(ControllerPtr ctl) {
  // Buttons for control
  // if (ctl->a()) {
  if (ctl->brake()>800 && ctl->throttle()>800) {
    // static bool prevA = false;
    // if (!prevA) { // edge
      // motorsEnabled = !motorsEnabled;
      motorsEnabled = true;
      applyEnablePin();
      // Visual feedback if supported
      if (motorsEnabled)
        ctl->setColorLED(0, 255, 0); // green
      else
        ctl->setColorLED(255, 0, 0); // red
      // if enabling, ensure relay is ON immediately
      if (motorsEnabled) {
        setRelay(true);
        lastActiveMs = millis();
    // prevA = true;
      }
    // }
  } else {
    // static bool prevA = false; prevA = false;
    motorsEnabled =false;
  }

  if (ctl->b()) {
    stopAll();
    motorsEnabled = false;
  }

  if (ctl->x()) {
    ctl->playDualRumble(0, 200, 0x60, 0x60);
    Serial.println("rumbled.");
  }

  // Read sticks
  float leftY  = axisToNorm(ctl->axisY());
  float rightY = axisToNorm(ctl->axisRY());

  // Deadzone
  if (fabsf(leftY)  < DEADZONE) leftY  = 0;
  if (fabsf(rightY) < DEADZONE) rightY = 0;

  // Scale to steps/s
  float vL = leftY * MAX_SPEED_STEPS_PER_SEC;
  float vR = rightY * MAX_SPEED_STEPS_PER_SEC;

  if (motorsEnabled) {
    // Map: left stick -> motors 0 and 1, right stick -> motors 2 and 3
    steppers[0].setSpeed(vL);
    steppers[1].setSpeed(vL);
    steppers[2].setSpeed(vR);
    steppers[3].setSpeed(vR);
    if (vL != 0.0f || vR != 0.0f) {
      setRelay(true);
      lastActiveMs = millis();
    }
  } else {
    stopAll();
  }
}

void processControllers() {
  for (auto ctl : gControllers) {
    if (ctl && ctl->isConnected() && ctl->hasData()) {
      if (ctl->isGamepad()) {
        processGamepad(ctl);
        // only the first connected gamepad is used to control; remove break to let others override
        break;
      }
    }
  }
}

// ====== Arduino setup/loop ======
void setup() {
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t* addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);
  // Comment the next line after initial pairing is stable
  // BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);

  // Steppers
  for (int i = 0; i < N_MOTORS; i++) {
    steppers[i].setMaxSpeed(MAX_SPEED_STEPS_PER_SEC);
    // Using runSpeed() => acceleration not used. If you prefer acceleration, switch to run() pattern.
    steppers[i].setSpeed(0);
  }

  applyEnablePin();
  stopAll();

  // Relay pin
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false); // start with power off until commanded
}

void loop() {
  // Poll controllers
  if (BP32.update()) {
    processControllers();
  }

  // Run all motors at the set speeds. Must be called as often as possible.
  for (int i = 0; i < N_MOTORS; i++) {
    steppers[i].runSpeed();
  }

  // Power management: if enabled but idle for a while, cut power
  bool anySpeed = false;
  for (int i = 0; i < N_MOTORS; i++) {
    if (steppers[i].speed() != 0.0f) { anySpeed = true; break; }
  }
  if (motorsEnabled && anySpeed) {
    if (!relayOn) setRelay(true);
    lastActiveMs = millis();
  } else {
    if (relayOn && (millis() - lastActiveMs >= RELAY_IDLE_OFF_MS)) {
      setRelay(false);
    }
  }

  // Yield a bit. Keep small to maintain step timing.
  delay(0);
}
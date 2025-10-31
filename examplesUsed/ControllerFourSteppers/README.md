# ControllerFourSteppers (28BYJ-48 + ULN2003 + Gamepad)

Control four 28BYJ-48 stepper motors via ULN2003 driver boards using a Bluetooth gamepad on ESP32 (Bluepad32). Uses AccelStepper in HALF4WIRE mode for smooth half-stepping. Includes a relay output to cut motor power when idle.

## Requirements

- Arduino IDE with ESP32 board support and Bluepad32 package (see repo root `README.md` for board manager URLs)
- Libraries:
  - AccelStepper (install via Library Manager)
- Hardware:
  - ESP32 board (with Bluetooth)
  - 4x 28BYJ-48 stepper motors
  - 4x ULN2003 driver boards
  - 1x relay module to switch motor 5V (optional but supported)

## Wiring

Each ULN2003 has inputs IN1..IN4.
This sketch expects pins in AccelStepper order: IN1, IN3, IN2, IN4 (important).

Default GPIO mapping (adjust in `MOTOR_PINS` in the sketch):

- Motor 0: IN1=GPIO13, IN3=GPIO12, IN2=GPIO18, IN4=GPIO21  
- Motor 1: IN1=GPIO13, IN3=GPIO12, IN2=GPIO18, IN4=GPIO21  GPIO12 is a strapping pin; avoid external pulls at boot)
- Motor 2: IN1=GPIO22, IN3=GPIO4,  IN2=GPIO23, IN4=GPIO5   
- Motor 3: IN1=GPIO22, IN3=GPIO4,  IN2=GPIO23, IN4=GPIO5   (GPIO4/5 can be special on some boards)

Shared:
- All ULN2003 boards: Vcc to 5V, GND to GND
- ESP32 GND to ULN2003 GND

Relay (optional):
- `RELAY_PIN` default GPIO19, active-HIGH by default. Use it to switch the 5V supply to the ULN2003 boards.
- Wire the relay in series with motor 5V supply (Common -> 5V input; NO -> ULN2003 Vcc), or as per your relay module’s instructions.

Notes:
- Avoid boot-strapping pins if possible: 0, 2, 12, 15 can affect boot. If you see boot problems, remap those pins.
- Don’t power 28BYJ-48 from ESP32 3.3V. Use a separate 5V supply with common ground.

## Controls (gamepad)

- throttle + brake: Toggle motors enabled/disabled (logical enable). When enabled or moving, relay turns ON. When idle for `RELAY_IDLE_OFF_MS`, relay turns OFF.
- B: Stop all motors immediately.
- X: Short rumble feedback (if supported by the controller).
- Left stick Y: Controls speed/direction of motors 0 & 1 (up = forward).
- Right stick Y: Controls speed/direction of motors 2 & 3.

## Tuning

In the sketch:
- `MAX_SPEED_STEPS_PER_SEC` (default 600) — increase carefully to avoid stalls.
- `DEADZONE` (default 0.10) — joystick deadzone.
- `RELAY_IDLE_OFF_MS` (default 3000 ms) — idle timeout before relay power-off.

If you prefer acceleration profiles:
- Replace `runSpeed()` with `run()` in `loop()` and set `setAcceleration()` per motor.

## Build & Upload

1. Install the ESP32 and Bluepad32 board packages (see repo `README.md`).
2. Install the AccelStepper library via Library Manager.
3. Open `ControllerFourSteppers.ino` and select your ESP32 board and port.
4. Upload. Open Serial Monitor at 115200 for logs.
5. Pair your gamepad (first time) and test controls.

## Troubleshooting

- IntelliSense include errors in VS Code don’t block Arduino builds. Ensure libraries/boards are installed and build from Arduino IDE.
- If motors twitch or direction is wrong, verify the IN1,IN3,IN2,IN4 order matches your ULN2003 board labeling.
- If the ESP32 fails to boot, move off GPIOs 0/2/12/15 and re-try.

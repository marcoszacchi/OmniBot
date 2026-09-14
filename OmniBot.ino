#include "config.h"
#include "state.h"
#include "motors.h"
#include "kinematics.h"
#include "imu.h"
#include "net.h"
#include "xbox.h"
#include <Preferences.h>
#include <math.h>

RobotState g_state;

namespace {

float g_rampedX = 0.0f, g_rampedY = 0.0f, g_rampedRotation = 0.0f; 
float g_headingTargetDeg = 0.0f;
uint32_t g_lastControlTickUs = 0;
uint32_t g_lastTelemetryMs = 0;
uint32_t g_lastWheelActivityMs = 0;
uint32_t g_compassCalibrationEndMs = 0;
uint32_t g_lastRotationCommandMs = 0;
bool g_isButtonHeld = false;
bool g_ledOffWhileButtonHeld = false;

void ramp(float& current, float target, float dt) {
  const float maxStep = INPUT_SLEW_PER_S * dt;
  current += constrain(target - current, -maxStep, maxStep);
}

int loadControlMode() {
  Preferences prefs;
  if (!prefs.begin("omnibot", true)) return MODE_WIFI;
  const int mode = prefs.getInt("mode", MODE_WIFI);
  prefs.end();
  return (mode == MODE_BLUETOOTH) ? MODE_BLUETOOTH : MODE_WIFI;
}

void saveControlMode(int mode) {
  Preferences prefs;
  if (prefs.begin("omnibot", false)) {
    prefs.putInt("mode", mode);
    prefs.end();
  }
}

void startCompassCalibration() {
  if (!imu::isCompassOk()) return;
  g_state.isArmed = true;
  g_state.isCalibratingCompass = true;
  g_compassCalibrationEndMs = millis() + COMPASS_CALIBRATION_MS;
  imu::beginCompassSampling();
}

void stopCompassCalibration(bool save) {
  if (!g_state.isCalibratingCompass) return;
  g_state.isCalibratingCompass = false;
  if (save) imu::endCompassSampling();
}

void switchControlMode() {
  const int mode = g_state.requestedControlMode;
  g_state.requestedControlMode = -1;
  if (mode != MODE_WIFI && mode != MODE_BLUETOOTH) return;
  if (mode == g_state.controlMode) return;

  g_state.command.x = g_state.command.y = g_state.command.rotation = 0.0f;
  g_state.command.receivedMs = 0;
  g_state.motorTest.receivedMs = 0;
  g_rampedX = g_rampedY = g_rampedRotation = 0.0f;
  stopCompassCalibration(false);
  motors::stop();

  if (mode == MODE_BLUETOOTH) {
    net::stop();
    if (!xbox::init()) {
      net::start();
      g_state.controlMode = MODE_WIFI;
      saveControlMode(MODE_WIFI);
      return;
    }
    g_state.controlMode = MODE_BLUETOOTH;
    saveControlMode(MODE_BLUETOOTH);
  } else {
    xbox::stop();
    net::start();
    g_state.controlMode = MODE_WIFI;
    saveControlMode(MODE_WIFI);
  }
}

void controlTick(float dt) {
  RobotState& state = g_state;
  const uint32_t nowMs = millis();

  if (state.stopRequested) {
    state.stopRequested = false;
    g_rampedX = g_rampedY = g_rampedRotation = 0.0f;
    stopCompassCalibration(false);
    motors::stop();
  }
  if (state.zeroYawRequested) {
    state.zeroYawRequested = false;
    imu::zeroYaw();
    g_headingTargetDeg = 0.0f;
  }
  if (state.compassCalibrationRequested) {
    state.compassCalibrationRequested = false;
    startCompassCalibration();
  }
  if (state.isArmed != motors::isEnabled()) {
    motors::enable(state.isArmed);
    if (!state.isArmed) g_rampedX = g_rampedY = g_rampedRotation = 0.0f;
    if (state.isArmed && imu::isGyroOk()) g_headingTargetDeg = imu::yawDeg();   // ao armar, a direção atual é a referência
  }

  const bool hasFreshCommand = (state.command.receivedMs != 0) && ((uint32_t)(nowMs - state.command.receivedMs) < COMMAND_TIMEOUT_MS);
  const bool hasFreshTest = (state.motorTest.receivedMs != 0) && ((uint32_t)(nowMs - state.motorTest.receivedMs) < COMMAND_TIMEOUT_MS);
  float moveX = 0.0f, moveY = 0.0f, rotation = 0.0f;
  if (hasFreshCommand) { moveX = state.command.x; moveY = state.command.y; rotation = state.command.rotation; }
  if (state.isCalibratingCompass) {
    moveX = 0.0f; moveY = 0.0f; rotation = COMPASS_CALIBRATION_SPIN;
    if ((int32_t)(nowMs - g_compassCalibrationEndMs) >= 0) { stopCompassCalibration(true); rotation = 0.0f; }
  }
  if (!state.isArmed) { moveX = 0.0f; moveY = 0.0f; rotation = 0.0f; }

  ramp(g_rampedX, moveX, dt);
  ramp(g_rampedY, moveY, dt);
  ramp(g_rampedRotation, rotation, dt);

  float vx = g_rampedX, vy = g_rampedY;
  const bool hasGyro = imu::isGyroOk();
  const float yawDeg = hasGyro ? imu::yawDeg() : 0.0f;

  if (state.fieldOrientedEnabled && hasGyro) {
    const float cosYaw = cosf(yawDeg * DEG_TO_RAD), sinYaw = sinf(yawDeg * DEG_TO_RAD);
    const float rotatedX = cosYaw * vx + sinYaw * vy;
    const float rotatedY = -sinYaw * vx + cosYaw * vy;
    vx = rotatedX; vy = rotatedY;
  }

  float omega = g_rampedRotation;
  const float moveMagnitude = sqrtf(vx * vx + vy * vy);
  const float yawRateDps = hasGyro ? imu::yawRateDps() : 0.0f;
  if (fabsf(g_rampedRotation) > 0.01f) g_lastRotationCommandMs = nowMs;
  const bool isSettling = ((uint32_t)(nowMs - g_lastRotationCommandMs) < HEADING_SETTLE_MS) && fabsf(yawRateDps) > HEADING_LATCH_RATE_DPS;
  if (state.headingHoldEnabled && hasGyro && imu::isYawTrusted() && !state.isCalibratingCompass) {
    if (fabsf(g_rampedRotation) > 0.01f || isSettling) {
      g_headingTargetDeg = yawDeg;
    } else if (moveMagnitude < 0.01f) {
      if (fabsf(remainderf(g_headingTargetDeg - yawDeg, 360.0f)) > HEADING_IDLE_RELATCH_DEG) g_headingTargetDeg = yawDeg;
    } else {
      float headingErrorDeg = remainderf(g_headingTargetDeg - yawDeg, 360.0f);
      if (fabsf(headingErrorDeg) > HEADING_GIVEUP_DEG) {
        g_headingTargetDeg = yawDeg;
        headingErrorDeg = 0.0f;
      }
      if (fabsf(headingErrorDeg) > HEADING_DEADBAND_DEG) {
        omega = constrain(HEADING_KP * headingErrorDeg - HEADING_KD * yawRateDps, -HEADING_MAX_CORRECTION, HEADING_MAX_CORRECTION);
      }
    }
  } else {
    g_headingTargetDeg = yawDeg;
  }

  const bool isTestingMotor = hasFreshTest && state.isArmed && !state.isCalibratingCompass;
  if (!isTestingMotor && state.isArmed) imu::updateGyroSign(omega);

  float wheels[3];
  if (isTestingMotor) {
    wheels[0] = wheels[1] = wheels[2] = 0.0f;
    wheels[state.motorTest.motorNumber - 1] = state.motorTest.command;
  } else {
    kinematics::wheelCommands(vx, vy, omega, wheels);
  }

  bool wheelsIdle = true;
  for (int wheelIndex = 0; wheelIndex < 3; wheelIndex++) {
    if (fabsf(wheels[wheelIndex]) < OUTPUT_DEADBAND) wheels[wheelIndex] = 0.0f;
    else wheelsIdle = false;
    state.wheelCommands[wheelIndex] = wheels[wheelIndex];
  }
  motors::write(wheels);
  if (isTestingMotor && state.motorTest.isRawDuty) {
    motors::writeRawDuty(state.motorTest.motorNumber - 1, state.motorTest.command);
  }

  if (!wheelsIdle) g_lastWheelActivityMs = nowMs;
  imu::setMotorsIdle(wheelsIdle && (uint32_t)(nowMs - g_lastWheelActivityMs) > 2000);
}

void onShortPress() {
  if (g_state.isCalibratingCompass) { stopCompassCalibration(false); return; }
  g_state.stopRequested = true;
  g_state.isArmed = !g_state.isArmed;
}

void onLongPress() {
  g_state.requestedControlMode = (g_state.controlMode == MODE_BLUETOOTH) ? MODE_WIFI : MODE_BLUETOOTH;
}

void updateButton() {
  static bool debouncedReleased = true;
  static bool lastRawReleased = true;
  static uint32_t lastEdgeMs = 0;
  static uint32_t pressStartMs = 0;
  static bool longPressFired = false;

  const bool rawReleased = digitalRead(PIN_BUTTON) == HIGH;
  const uint32_t nowMs = millis();
  if (rawReleased != lastRawReleased) { lastRawReleased = rawReleased; lastEdgeMs = nowMs; }
  if ((uint32_t)(nowMs - lastEdgeMs) >= BUTTON_DEBOUNCE_MS && rawReleased != debouncedReleased) {
    debouncedReleased = rawReleased;
    g_isButtonHeld = !debouncedReleased;
    if (!debouncedReleased) {
      pressStartMs = nowMs;
      longPressFired = false;
    } else if (!longPressFired) {
      onShortPress();
    }
  }
  if (!debouncedReleased && !longPressFired && (uint32_t)(nowMs - pressStartMs) >= BUTTON_LONG_PRESS_MS) {
    longPressFired = true;
    onLongPress();
  }
  g_ledOffWhileButtonHeld = !debouncedReleased && !longPressFired;
}

constexpr uint32_t LED_BLINK_MS = 150;
uint32_t g_ledBlinkStartMs = 0;
int g_ledBlinkCount = 0;

void blinkLed(int count) {
  g_ledBlinkCount = count;
  g_ledBlinkStartMs = millis();
}

void updateLed() {
  const uint32_t nowMs = millis();
  const uint32_t phaseMs = nowMs % 1000;
  const bool isConnected = (g_state.controlMode == MODE_BLUETOOTH) ? xbox::isConnected() : (net::clientCount() > 0);

  static bool lastHeadingHold = g_state.headingHoldEnabled;
  static bool lastFieldOriented = g_state.fieldOrientedEnabled;
  if (g_state.headingHoldEnabled != lastHeadingHold) { lastHeadingHold = g_state.headingHoldEnabled; blinkLed(lastHeadingHold ? 1 : 2); }
  if (g_state.fieldOrientedEnabled != lastFieldOriented) { lastFieldOriented = g_state.fieldOrientedEnabled; blinkLed(lastFieldOriented ? 1 : 2); }

  bool ledOn;
  if (g_ledOffWhileButtonHeld)        ledOn = false;
  else if (g_state.isCalibratingCompass) ledOn = (nowMs % 100) < 50;
  else if (!g_state.isArmed)          ledOn = (nowMs % 300) < 150;
  else if (isConnected)               ledOn = true;
  else if (g_state.controlMode == MODE_BLUETOOTH)
                                      ledOn = (phaseMs < 80) || (phaseMs >= 200 && phaseMs < 280);
  else                                ledOn = phaseMs < 100;

  if (g_ledBlinkCount > 0 && !g_ledOffWhileButtonHeld) {
    const uint32_t elapsedMs = nowMs - g_ledBlinkStartMs;
    const uint32_t totalMs = (uint32_t)g_ledBlinkCount * 2 * LED_BLINK_MS;
    if (elapsedMs < totalMs) ledOn = ((elapsedMs / LED_BLINK_MS) % 2) == 1;
    else g_ledBlinkCount = 0;
  }
  digitalWrite(PIN_LED, ledOn ? LOW : HIGH);
}

void readSerialCommands() {
  static char line[96];
  static size_t length = 0;
  while (Serial.available() > 0) {
    const char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (length > 0) { line[length] = '\0'; net::handleCommand(line); length = 0; }
    } else if (length < sizeof(line) - 1) {
      line[length++] = ch;
    }
  }
}

void sendTelemetry() {
  const RobotState& state = g_state;
  char line[160];
  snprintf(line, sizeof(line), "T %.1f %.1f %.2f %.2f %.2f %d %d %d %d %d %d %d %d %.2f %.2f %.2f %d %d %d %d %d %.2f %d %d %d",
           imu::yawDeg(), imu::compassHeadingDeg(), state.wheelCommands[0], state.wheelCommands[1], state.wheelCommands[2],
           state.isArmed, imu::isGyroOk(), imu::isCompassOk(), state.headingHoldEnabled, state.fieldOrientedEnabled,
           state.isCalibratingCompass, net::clientCount(), g_isButtonHeld,
           imu::yawRateDps(), imu::gyroBiasDps(), imu::gyroNoiseDps(), imu::isStill(), imu::isYawTrusted(),
           imu::gyroRestartCount(), imu::gyroSign(), imu::isGyroSignConfirmed(),
           motors::minDuty(), motors::isInverted(0), motors::isInverted(1), motors::isInverted(2));
  net::broadcast(line);
}

}

void setup() {
  motors::init();
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  Serial.begin(115200);

  imu::init();

  g_state.controlMode = loadControlMode();
  if (g_state.controlMode == MODE_BLUETOOTH) {
    net::stop();
    xbox::init();
  } else {
    net::start();
  }

  g_state.isArmed = true;
  motors::enable(true);
  g_lastControlTickUs = micros();

  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

void loop() {
  if (g_state.controlMode == MODE_BLUETOOTH) xbox::loop();
  else net::loop();
  readSerialCommands();
  imu::update();
  updateButton();
  if (g_state.requestedControlMode >= 0) switchControlMode();

  const uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - g_lastControlTickUs) >= CONTROL_PERIOD_US) {
    const float dt = fminf((uint32_t)(nowUs - g_lastControlTickUs) * 1e-6f, 0.05f);
    g_lastControlTickUs = nowUs;
    controlTick(dt);
  }

  updateLed();

  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - g_lastTelemetryMs) >= TELEMETRY_PERIOD_MS) {
    g_lastTelemetryMs = nowMs;
    sendTelemetry();
  }
}
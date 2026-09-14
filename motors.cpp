#include "motors.h"
#include "config.h"
#include <Preferences.h>
#include <math.h>

namespace motors {
namespace {

struct MotorPins { uint8_t in1, in2; };
const MotorPins MOTOR_PINS[3] = {
  { PIN_M1_IN1, PIN_M1_IN2 },
  { PIN_M2_IN1, PIN_M2_IN2 },
  { PIN_M3_IN1, PIN_M3_IN2 },
};
constexpr uint32_t MAX_DUTY = (1u << PWM_RESOLUTION_BITS) - 1;
bool g_driversEnabled = false;
float g_minDuty = MOTOR_MIN_DUTY;
bool g_inverted[3] = { MOTOR_INVERTED[0], MOTOR_INVERTED[1], MOTOR_INVERTED[2] };

int invertedMask(const bool inverted[3]) {
  return (inverted[0] ? 1 : 0) | (inverted[1] ? 2 : 0) | (inverted[2] ? 4 : 0);
}

void loadSettings() {
  Preferences prefs;
  if (!prefs.begin("omnibot", true)) return;
  if (prefs.isKey("mduty") && prefs.getFloat("mdutydef", -1.0f) == MOTOR_MIN_DUTY) {
    g_minDuty = prefs.getFloat("mduty", MOTOR_MIN_DUTY);
  }
  if (prefs.isKey("minv") && prefs.getInt("minvdef", -1) == invertedMask(MOTOR_INVERTED)) {
    const int mask = prefs.getInt("minv", 0);
    for (int motorIndex = 0; motorIndex < 3; motorIndex++) g_inverted[motorIndex] = ((mask >> motorIndex) & 1) != 0;
  }
  prefs.end();
}

void saveSettings() {
  Preferences prefs;
  if (!prefs.begin("omnibot", false)) return;
  prefs.putFloat("mduty", g_minDuty);
  prefs.putFloat("mdutydef", MOTOR_MIN_DUTY);
  prefs.putInt("minv", invertedMask(g_inverted));
  prefs.putInt("minvdef", invertedMask(MOTOR_INVERTED));
  prefs.end();
}

void writeBridge(int motorIndex, bool isForward, uint32_t duty) {
  uint32_t in1Duty = MAX_DUTY, in2Duty = MAX_DUTY;
  if (duty != 0) {
    if (isForward) in2Duty = MAX_DUTY - duty;
    else           in1Duty = MAX_DUTY - duty;
  }
  ledcWrite(2 * motorIndex, in1Duty);
  ledcWrite(2 * motorIndex + 1, in2Duty);
}

void writeMotor(int motorIndex, float command, float minDuty, float maxDuty) {
  if (g_inverted[motorIndex]) command = -command;
  const float magnitude = fminf(fabsf(command), 1.0f);
  uint32_t duty = 0;
  if (magnitude > 0.001f) {
    const float dutyFraction = fminf(minDuty + magnitude * (maxDuty - minDuty), 1.0f);
    duty = (uint32_t)lroundf(dutyFraction * MAX_DUTY);
  }
  writeBridge(motorIndex, command > 0, duty);
}

void coastAll() {
  for (int channel = 0; channel < 6; channel++) ledcWrite(channel, 0);
}

}

void init() {
  loadSettings();
  pinMode(PIN_DRIVER_SLEEP, OUTPUT);
  digitalWrite(PIN_DRIVER_SLEEP, LOW);
  for (int motorIndex = 0; motorIndex < 3; motorIndex++) {
    ledcSetup(2 * motorIndex, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcAttachPin(MOTOR_PINS[motorIndex].in1, 2 * motorIndex);
    ledcSetup(2 * motorIndex + 1, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcAttachPin(MOTOR_PINS[motorIndex].in2, 2 * motorIndex + 1);
  }
  coastAll();
}

void enable(bool enabled) {
  g_driversEnabled = enabled;
  digitalWrite(PIN_DRIVER_SLEEP, enabled ? HIGH : LOW);
  if (!enabled) coastAll();
}

bool isEnabled() { return g_driversEnabled; }

void write(const float commands[3]) {
  if (!g_driversEnabled) { coastAll(); return; }
  for (int motorIndex = 0; motorIndex < 3; motorIndex++) {
    writeMotor(motorIndex, commands[motorIndex], g_minDuty, MOTOR_MAX_DUTY);
  }
}

void writeRawDuty(int motorIndex, float dutySigned) {
  if (g_driversEnabled && motorIndex >= 0 && motorIndex < 3) writeMotor(motorIndex, dutySigned, 0.0f, 1.0f);
}

void stop() {
  const float zeros[3] = { 0.0f, 0.0f, 0.0f };
  write(zeros);
}

float minDuty() { return g_minDuty; }

void setMinDuty(float duty) {
  g_minDuty = constrain(roundf(duty * 100.0f) / 100.0f, 0.0f, MOTOR_MAX_DUTY);
  saveSettings();
}

bool isInverted(int motorIndex) {
  return motorIndex >= 0 && motorIndex < 3 && g_inverted[motorIndex];
}

void setInverted(int motorIndex, bool inverted) {
  if (motorIndex < 0 || motorIndex >= 3) return;
  g_inverted[motorIndex] = inverted;
  saveSettings();
}

}

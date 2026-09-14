#include "imu.h"
#include "config.h"
#include <Wire.h>
#include <Preferences.h>
#include <MPU6050.h>
#include <Adafruit_QMC5883P.h>
#include <math.h>

namespace imu {
namespace {

constexpr uint8_t COMPASS_I2C_ADDRESS = 0x2C;
constexpr float GYRO_LSB_PER_DPS = 65.5f;
constexpr int MPU_FROZEN_READ_LIMIT = 100;
constexpr int GYRO_SIGN_VOTES_NEEDED = 60;
constexpr float GYRO_SIGN_MIN_COMMAND = 0.25f;
constexpr float GYRO_SIGN_MIN_RATE_DPS = 15.0f;

MPU6050 g_mpu(MPU_I2C_ADDRESS);
Adafruit_QMC5883P g_compass;

bool g_mpuOk = false;
uint32_t g_mpuLastRetryMs = 0;
int g_mpuFailCount = 0;
int g_mpuRestartCount = 0;
int16_t g_lastRawSample[6] = { 0, 0, 0, 0, 0, 0 };
int g_frozenReadCount = 0;

float g_gyroBiasDps = 0.0f;
float g_gyroRateDps = 0.0f;
float g_yawDeg = 0.0f;
uint32_t g_lastGyroReadUs = 0;
bool g_motorsIdle = false;
bool g_isBiasTrusted = false;

double g_windowSum = 0.0, g_windowSumSquares = 0.0;
int g_windowCount = 0;
float g_gyroNoiseDps = 0.0f;
bool g_isStill = false;

float g_gyroSign = 1.0f;
int g_gyroSignVotes = 0;
bool g_isGyroSignConfirmed = false;

bool g_compassOk = false;
int g_compassFailCount = 0;
uint32_t g_lastCompassReadMs = 0;
float g_compassOffset[2] = { 0, 0 };
float g_compassScale[2] = { 1, 1 };
float g_compassHeadingDeg = 0.0f;
bool g_isSamplingCompass = false;
float g_sampleMin[2], g_sampleMax[2];

bool isI2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}


bool initMpu() {
  if (!isI2cDevicePresent(MPU_I2C_ADDRESS)) return false;
  g_mpu.initialize();
  g_mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_500);
  g_mpu.setDLPFMode(MPU6050_DLPF_BW_42);
  g_mpu.setRate(4);
  return true;
}

bool readGyroZ(float& rateDps) {
  int16_t raw[6];
  g_mpu.getMotion6(&raw[0], &raw[1], &raw[2], &raw[3], &raw[4], &raw[5]);
  bool allZero = true, allOnes = true, sameAsLast = true;
  for (int axis = 0; axis < 6; axis++) {
    allZero = allZero && raw[axis] == 0;
    allOnes = allOnes && raw[axis] == -1;
    sameAsLast = sameAsLast && raw[axis] == g_lastRawSample[axis];
    g_lastRawSample[axis] = raw[axis];
  }
  if (allZero || allOnes) return false;
  g_frozenReadCount = sameAsLast ? g_frozenReadCount + 1 : 0;
  if (g_frozenReadCount >= MPU_FROZEN_READ_LIMIT) return false;
  rateDps = raw[5] / GYRO_LSB_PER_DPS;
  return true;
}

void calibrateGyro() {
  delay(150);

  float bestMean = 0.0f, bestStd = 1e9f;
  for (int attempt = 1; attempt <= GYRO_CALIBRATION_ATTEMPTS; attempt++) {
    double sum = 0.0, sumSquares = 0.0;
    int sampleCount = 0;
    for (int sample = 0; sample < GYRO_CALIBRATION_SAMPLES; sample++) {
      float rateDps;
      if (readGyroZ(rateDps)) { sum += rateDps; sumSquares += (double)rateDps * rateDps; sampleCount++; }
      delay(5);
    }
    if (sampleCount < GYRO_CALIBRATION_SAMPLES / 2) continue;
    const float mean = (float)(sum / sampleCount);
    const float variance = (float)(sumSquares / sampleCount) - mean * mean;
    const float stdDev = sqrtf(variance > 0 ? variance : 0);
    if (stdDev < bestStd) { bestStd = stdDev; bestMean = mean; }
    if (stdDev < GYRO_STILL_MAX_STD_DPS) break;
  }
  g_gyroBiasDps = bestMean;
  g_isBiasTrusted = bestStd < GYRO_STILL_MAX_STD_DPS;
}

bool initCompass() {
  if (!isI2cDevicePresent(COMPASS_I2C_ADDRESS) || !g_compass.begin(COMPASS_I2C_ADDRESS, &Wire)) return false;
  g_compass.setMode(QMC5883P_MODE_CONTINUOUS);
  g_compass.setODR(QMC5883P_ODR_200HZ);
  g_compass.setOSR(QMC5883P_OSR_8);
  g_compass.setDSR(QMC5883P_DSR_4);
  g_compass.setRange(QMC5883P_RANGE_8G);
  g_compass.setSetResetMode(QMC5883P_SETRESET_ON);
  return true;
}

bool readCompass() {
  if (!g_compass.isDataReady()) return true;
  int16_t rawX, rawY, rawZ;
  if (!g_compass.getRawMagnetic(&rawX, &rawY, &rawZ)) return false;
  const float reading[2] = { (float)rawX, (float)rawY };
  if (g_isSamplingCompass) {
    for (int axis = 0; axis < 2; axis++) {
      g_sampleMin[axis] = fminf(g_sampleMin[axis], reading[axis]);
      g_sampleMax[axis] = fmaxf(g_sampleMax[axis], reading[axis]);
    }
  }
  const float fieldX = (reading[0] - g_compassOffset[0]) * g_compassScale[0];
  const float fieldY = (reading[1] - g_compassOffset[1]) * g_compassScale[1];
  const float heading = fmodf(atan2f(fieldY, fieldX) * RAD_TO_DEG + COMPASS_HEADING_OFFSET_DEG, 360.0f);
  g_compassHeadingDeg = heading < 0 ? heading + 360.0f : heading;
  return true;
}

void loadCalibration() {
  Preferences prefs;
  if (!prefs.begin("omnibot", true)) return;
  if (prefs.isKey("mox")) {
    g_compassOffset[0] = prefs.getFloat("mox", 0);
    g_compassOffset[1] = prefs.getFloat("moy", 0);
    g_compassScale[0] = prefs.getFloat("msx", 1);
    g_compassScale[1] = prefs.getFloat("msy", 1);
  }
  if (prefs.isKey("gsign")) {
    g_gyroSign = prefs.getInt("gsign", 1) < 0 ? -1.0f : 1.0f;
    g_isGyroSignConfirmed = true;
  }
  prefs.end();
}

void saveGyroSign() {
  Preferences prefs;
  if (!prefs.begin("omnibot", false)) return;
  prefs.putInt("gsign", g_gyroSign < 0 ? -1 : 1);
  prefs.end();
}

void saveCompassCalibration() {
  Preferences prefs;
  if (!prefs.begin("omnibot", false)) return;
  prefs.putFloat("mox", g_compassOffset[0]);
  prefs.putFloat("moy", g_compassOffset[1]);
  prefs.putFloat("msx", g_compassScale[0]);
  prefs.putFloat("msy", g_compassScale[1]);
  prefs.end();
}

} 

void init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  Wire.setTimeOut(50);

  while (millis() < 200) delay(5);
  for (int attempt = 0; attempt < 10 && !g_mpuOk; attempt++) {
    g_mpuOk = initMpu();
    if (!g_mpuOk) delay(100);
  }
  if (g_mpuOk) calibrateGyro();

  g_compassOk = initCompass();
  loadCalibration();
  g_lastGyroReadUs = micros();
  g_lastCompassReadMs = g_mpuLastRetryMs = millis();
}

void update() {
  const uint32_t nowMs = millis();

  if (!g_mpuOk && (uint32_t)(nowMs - g_mpuLastRetryMs) >= 2000) {
    g_mpuLastRetryMs = nowMs;
    if (initMpu()) {
      g_mpuOk = true;
      g_mpuFailCount = 0;
      g_frozenReadCount = 0;
      g_lastGyroReadUs = micros();
      g_mpuRestartCount++;
    }
  }

  if (g_mpuOk) {
    const uint32_t nowUs = micros();
    if ((uint32_t)(nowUs - g_lastGyroReadUs) >= GYRO_READ_PERIOD_US) {
      float rawRateDps;
      if (readGyroZ(rawRateDps)) {
        const float dt = fminf((uint32_t)(nowUs - g_lastGyroReadUs) * 1e-6f, 0.1f);
        g_lastGyroReadUs = nowUs;
        g_gyroRateDps = rawRateDps - g_gyroBiasDps;
        g_yawDeg = remainderf(g_yawDeg + g_gyroSign * g_gyroRateDps * dt, 360.0f);

        g_windowSum += rawRateDps;
        g_windowSumSquares += (double)rawRateDps * rawRateDps;
        if (++g_windowCount >= GYRO_BIAS_WINDOW_SAMPLES) {
          const float mean = (float)(g_windowSum / g_windowCount);
          const float variance = (float)(g_windowSumSquares / g_windowCount) - mean * mean;
          g_gyroNoiseDps = sqrtf(variance > 0 ? variance : 0);
          g_isStill = g_gyroNoiseDps < GYRO_STILL_MAX_STD_DPS;
          if (g_motorsIdle && g_isStill) {
            g_gyroBiasDps += (mean - g_gyroBiasDps) * GYRO_BIAS_ALPHA;
            g_isBiasTrusted = true;
          }
          g_windowSum = g_windowSumSquares = 0.0;
          g_windowCount = 0;
        }
        g_mpuFailCount = 0;
      } else {
        g_lastGyroReadUs = nowUs;
        if (++g_mpuFailCount > 50) {
          g_mpuOk = false;
          g_mpuLastRetryMs = nowMs;
        }
      }
    }
  }

  if (g_compassOk && (uint32_t)(nowMs - g_lastCompassReadMs) >= COMPASS_READ_PERIOD_MS) {
    g_lastCompassReadMs = nowMs;
    if (readCompass()) {
      g_compassFailCount = 0;
    } else if (++g_compassFailCount > 25) {
      g_compassOk = false;
    }
  }
}

bool isGyroOk() { return g_mpuOk; }
int gyroRestartCount() { return g_mpuRestartCount; }
float yawDeg() { return g_yawDeg; }
float yawRateDps() { return g_gyroSign * g_gyroRateDps; }
float gyroBiasDps() { return g_gyroBiasDps; }
float gyroNoiseDps() { return g_gyroNoiseDps; }
bool isStill() { return g_isStill; }
bool isYawTrusted() { return g_mpuOk && g_isBiasTrusted; }
void zeroYaw() { g_yawDeg = 0.0f; }
void setMotorsIdle(bool idle) { g_motorsIdle = idle; }
int gyroSign() { return g_gyroSign < 0 ? -1 : 1; }
bool isGyroSignConfirmed() { return g_isGyroSignConfirmed; }

void updateGyroSign(float commandedRotation) {
  if (!g_mpuOk) return;
  const float measuredRate = yawRateDps();
  if (fabsf(commandedRotation) < GYRO_SIGN_MIN_COMMAND || fabsf(measuredRate) < GYRO_SIGN_MIN_RATE_DPS) return;
  g_gyroSignVotes += ((commandedRotation > 0) == (measuredRate > 0)) ? 1 : -1;
  if (g_gyroSignVotes >= GYRO_SIGN_VOTES_NEEDED) {
    g_gyroSignVotes = GYRO_SIGN_VOTES_NEEDED;
    if (!g_isGyroSignConfirmed) {
      g_isGyroSignConfirmed = true;
      saveGyroSign();
    }
  } else if (g_gyroSignVotes <= -GYRO_SIGN_VOTES_NEEDED) {
    g_gyroSign = -g_gyroSign;
    g_gyroSignVotes = 0;
    g_isGyroSignConfirmed = true;
    g_yawDeg = -g_yawDeg;
    saveGyroSign();
  }
}

bool isCompassOk() { return g_compassOk; }
float compassHeadingDeg() { return g_compassHeadingDeg; }

void beginCompassSampling() {
  for (int axis = 0; axis < 2; axis++) {
    g_sampleMin[axis] = 32767.0f;
    g_sampleMax[axis] = -32768.0f;
  }
  g_isSamplingCompass = true;
}

void endCompassSampling() {
  if (!g_isSamplingCompass) return;
  g_isSamplingCompass = false;
  float radius[2];
  for (int axis = 0; axis < 2; axis++) {
    g_compassOffset[axis] = 0.5f * (g_sampleMax[axis] + g_sampleMin[axis]);
    radius[axis] = fmaxf(0.5f * (g_sampleMax[axis] - g_sampleMin[axis]), 1.0f);
  }
  const float meanRadius = 0.5f * (radius[0] + radius[1]);
  g_compassScale[0] = meanRadius / radius[0];
  g_compassScale[1] = meanRadius / radius[1];
  saveCompassCalibration();
}

}

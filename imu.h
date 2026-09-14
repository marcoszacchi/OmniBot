#pragma once
#include <Arduino.h>

namespace imu {

void init();
void update();

bool isGyroOk();
int gyroRestartCount();
float yawDeg();
float yawRateDps();
float gyroBiasDps();
float gyroNoiseDps();
bool isStill();
bool isYawTrusted();
void zeroYaw();
void setMotorsIdle(bool idle);

void updateGyroSign(float commandedRotation);
int gyroSign();
bool isGyroSignConfirmed();

bool isCompassOk();
float compassHeadingDeg();
void beginCompassSampling();
void endCompassSampling();

}
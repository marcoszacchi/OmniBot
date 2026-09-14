#include "kinematics.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

namespace kinematics {

void wheelCommands(float vx, float vy, float omega, float wheels[3]) {
  float peakMagnitude = 1.0f;
  for (int wheelIndex = 0; wheelIndex < 3; wheelIndex++) {
    const float wheelAngle = WHEEL_ANGLE_DEG[wheelIndex] * DEG_TO_RAD;
    wheels[wheelIndex] = -sinf(wheelAngle) * vx + cosf(wheelAngle) * vy + omega;
    peakMagnitude = fmaxf(peakMagnitude, fabsf(wheels[wheelIndex]));
  }
  for (int wheelIndex = 0; wheelIndex < 3; wheelIndex++) wheels[wheelIndex] /= peakMagnitude;
}

}

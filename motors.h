#pragma once
#include <Arduino.h>

namespace motors {

void init();
void enable(bool enabled);
bool isEnabled();
void write(const float commands[3]);
void writeRawDuty(int motorIndex, float dutySigned);
void stop();

float minDuty();
void setMinDuty(float duty);
bool isInverted(int motorIndex);
void setInverted(int motorIndex, bool inverted);

}

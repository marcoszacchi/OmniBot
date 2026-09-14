#pragma once
#include <Arduino.h>

namespace xbox {

bool init();
void stop();
void loop();
bool isConnected();
void forgetController();

}
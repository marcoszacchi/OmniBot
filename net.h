#pragma once
#include <Arduino.h>

namespace net {

void start();
void stop();
void loop();
int clientCount();
void broadcast(const char* message);
void handleCommand(const char* line);

}
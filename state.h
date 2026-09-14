#pragma once
#include <Arduino.h>

struct MoveCommand {
  float x = 0.0f;
  float y = 0.0f;
  float rotation = 0.0f;
  uint32_t receivedMs = 0;
};

struct MotorTest {
  int motorNumber = 0;
  float command = 0.0f;
  bool isRawDuty = false;
  uint32_t receivedMs = 0;
};

struct RobotState {
  MoveCommand command;
  MotorTest motorTest;

  bool isArmed = true;
  bool headingHoldEnabled = true;
  bool fieldOrientedEnabled = false;
  bool isCalibratingCompass = false;

  bool stopRequested = false;
  bool zeroYawRequested = false;
  bool compassCalibrationRequested = false;

  int controlMode = 0;
  int requestedControlMode = -1;

  float wheelCommands[3] = { 0.0f, 0.0f, 0.0f };
};

extern RobotState g_state;
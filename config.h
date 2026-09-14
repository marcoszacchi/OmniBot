#pragma once
#include <Arduino.h>

//  GPIO's
constexpr uint8_t PIN_M1_IN1 = 0;
constexpr uint8_t PIN_M1_IN2 = 1;
constexpr uint8_t PIN_M2_IN1 = 3;
constexpr uint8_t PIN_M2_IN2 = 4;
constexpr uint8_t PIN_M3_IN1 = 5;
constexpr uint8_t PIN_M3_IN2 = 6;
constexpr uint8_t PIN_DRIVER_SLEEP = 7;
constexpr uint8_t PIN_I2C_SDA = 8;
constexpr uint8_t PIN_I2C_SCL = 9;
constexpr uint8_t PIN_LED = 10;
constexpr uint8_t PIN_BUTTON = 20;

//  PWM
constexpr uint32_t PWM_FREQ_HZ = 20000;
constexpr uint8_t PWM_RESOLUTION_BITS = 10;
constexpr float MOTOR_MAX_DUTY = 0.80f;
constexpr float MOTOR_MIN_DUTY = 0.05f;
constexpr bool MOTOR_INVERTED[3] = { false, false, false };
constexpr float WHEEL_ANGLE_DEG[3] = { 150.0f, 270.0f, 30.0f };

//  CONTROLE
constexpr uint32_t CONTROL_PERIOD_US = 10000;
constexpr uint32_t COMMAND_TIMEOUT_MS = 400;
constexpr float INPUT_SLEW_PER_S = 5.0f;
constexpr float OUTPUT_DEADBAND = 0.003f;
constexpr float HEADING_KP = 0.02f;
constexpr float HEADING_KD = 0.0015f;
constexpr float HEADING_MAX_CORRECTION = 0.50f;
constexpr float HEADING_DEADBAND_DEG = 1.5f;
constexpr float HEADING_LATCH_RATE_DPS = 20.0f;
constexpr uint32_t HEADING_SETTLE_MS = 500;
constexpr float HEADING_IDLE_RELATCH_DEG = 10.0f;
constexpr float HEADING_GIVEUP_DEG = 30.0f;

//  IMU
constexpr uint32_t I2C_FREQ_HZ = 100000;
constexpr uint8_t MPU_I2C_ADDRESS = 0x68;
constexpr uint32_t GYRO_READ_PERIOD_US = 10000;
constexpr int GYRO_CALIBRATION_SAMPLES = 120;
constexpr int GYRO_CALIBRATION_ATTEMPTS = 4;
constexpr float GYRO_STILL_MAX_STD_DPS = 0.8f;
constexpr int GYRO_BIAS_WINDOW_SAMPLES = 100;
constexpr float GYRO_BIAS_ALPHA = 0.5f;
constexpr float COMPASS_HEADING_OFFSET_DEG = 0.0f;
constexpr uint32_t COMPASS_READ_PERIOD_MS = 40;
constexpr uint32_t COMPASS_CALIBRATION_MS = 12000;
constexpr float COMPASS_CALIBRATION_SPIN = 0.35f;

//  WIFI e MODO DE CONTROLE
constexpr char AP_SSID[] = "OmniBot";
constexpr char AP_PASSWORD[] = "omnibot_";
constexpr uint8_t AP_CHANNEL = 1;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr uint32_t TELEMETRY_PERIOD_MS = 100;
constexpr int MODE_WIFI = 0;
constexpr int MODE_BLUETOOTH = 1;

//  CONTROLE XBOX
constexpr float XBOX_STICK_DEADZONE = 0.22f;
constexpr float XBOX_AXIS_DEADZONE = 0.12f;
constexpr float XBOX_BRAKE_FACTOR = 0.80f;
constexpr float XBOX_SPEED_STEP = 0.10f;
constexpr float XBOX_MIN_SPEED = 0.10f;
constexpr uint32_t XBOX_COMBO_HOLD_MS = 2000;
constexpr int XBOX_CONNECT_RETRIES = 3;
constexpr uint32_t XBOX_CONNECT_TIMEOUT_MS = 2500;

//  BOTÃO GPIO 20
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 1500;
#include "xbox.h"
#include "config.h"
#include "state.h"
#include "motors.h"
#include <NimBLEDevice.h>
#include <XboxControllerNotificationParser.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>
#include <string>
#include <vector>

namespace xbox {
namespace {

enum class LinkState { Idle, Scanning, Found, Connected };

constexpr uint16_t HID_SERVICE_UUID = 0x1812;
constexpr uint16_t HID_REPORT_UUID = 0x2A4D;
constexpr uint16_t REPORT_REFERENCE_UUID = 0x2908;
constexpr uint16_t DEVICE_INFO_SERVICE_UUID = 0x180A;
constexpr uint16_t GAMEPAD_APPEARANCE = 0x03C4;

XboxControllerNotificationParser g_report;
volatile uint32_t g_reportCount = 0;
volatile bool g_disconnectPending = false;
volatile bool g_pairingFailed = false;

bool g_isActive = false;
LinkState g_linkState = LinkState::Idle;
NimBLEClient* g_bleClient = nullptr;
NimBLERemoteCharacteristic* g_outputReport = nullptr;
NimBLEAddress g_targetAddress;
NimBLEAddress g_bondedAddress;
bool g_hasBondedAddress = false;
bool g_hasReceivedReport = false;
uint32_t g_lastReportCount = 0;
uint32_t g_scanStartMs = 0;
XboxControllerNotificationParser g_previousReport;
uint32_t g_calibrationComboStartMs = 0, g_wifiComboStartMs = 0;
bool g_calibrationComboFired = false, g_wifiComboFired = false;
float g_maxSpeed = 0.7f;
float g_rotationSpeed = 0.5f;

void loadSettings() {
  Preferences prefs;
  if (!prefs.begin("omnibot", true)) return;
  g_maxSpeed = prefs.getFloat("vmax", 0.7f);
  g_rotationSpeed = prefs.getFloat("vrot", 0.5f);
  const String address = prefs.getString("xbaddr", "");
  const int addressType = prefs.getInt("xbtype", 0);
  prefs.end();
  if (address.length() == 17) {
    g_bondedAddress = NimBLEAddress(std::string(address.c_str()), (uint8_t)addressType);
    g_hasBondedAddress = true;
  }
}

void saveSpeeds() {
  Preferences prefs;
  if (!prefs.begin("omnibot", false)) return;
  prefs.putFloat("vmax", g_maxSpeed);
  prefs.putFloat("vrot", g_rotationSpeed);
  prefs.end();
}

void saveBondedAddress() {
  Preferences prefs;
  if (!prefs.begin("omnibot", false)) return;
  prefs.putString("xbaddr", g_bondedAddress.toString().c_str());
  prefs.putInt("xbtype", (int)g_bondedAddress.getType());
  prefs.end();
}

float normalizeAxis(uint16_t raw) {
  const float center = XboxControllerNotificationParser::maxJoy / 2.0f;
  return constrain(((float)raw - center) / center, -1.0f, 1.0f);
}

float normalizeTrigger(uint16_t raw) {
  return (float)raw / XboxControllerNotificationParser::maxTrig;
}

void applyRadialDeadzone(float& x, float& y, float deadzone) {
  const float magnitude = sqrtf(x * x + y * y);
  if (magnitude < deadzone) { x = 0.0f; y = 0.0f; return; }
  const float scale = fminf((magnitude - deadzone) / (1.0f - deadzone), 1.0f) / magnitude;
  x *= scale;
  y *= scale;
}

float applyAxisDeadzone(float value, float deadzone) {
  const float magnitude = fabsf(value);
  if (magnitude < deadzone) return 0.0f;
  const float rescaled = fminf((magnitude - deadzone) / (1.0f - deadzone), 1.0f);
  return value < 0 ? -rescaled : rescaled;
}

bool justPressed(bool isDown, bool wasDown) { return isDown && !wasDown; }

void clearCommand() {
  g_state.command.x = g_state.command.y = g_state.command.rotation = 0.0f;
  g_state.command.receivedMs = 0;
}

bool isSameAddress(const NimBLEAddress& a, const NimBLEAddress& b) {
  return memcmp(a.getVal(), b.getVal(), 6) == 0;
}

bool isDiscoverable(uint8_t advertisingFlags) {
  return (advertisingFlags & (BLE_HS_ADV_F_DISC_LTD | BLE_HS_ADV_F_DISC_GEN)) != 0;
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    if (g_linkState != LinkState::Scanning) return;
    const uint8_t advertisingType = device->getAdvType();
    const bool isDirected = advertisingType == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD ||
                            advertisingType == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_LD;
    const bool isKnown = g_hasBondedAddress && (isDirected || isSameAddress(device->getAddress(), g_bondedAddress));
    const bool isXbox = isKnown ||
                        (device->haveAppearance() && device->getAppearance() == GAMEPAD_APPEARANCE) ||
                        (device->haveName() && device->getName().rfind("Xbox", 0) == 0);
    if (!isXbox) return;

    const uint8_t advertisingFlags = device->getAdvFlags();
    const bool shouldConnect = isKnown || advertisingFlags == 0 || isDiscoverable(advertisingFlags);
    if (!shouldConnect || !device->isConnectable()) return;

    g_targetAddress = device->getAddress();
    g_linkState = LinkState::Found;
    NimBLEDevice::getScan()->stop();
  }
};

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int) override {
    g_disconnectPending = true;
  }
  void onAuthenticationComplete(NimBLEConnInfo& connection) override {
    if (!connection.isEncrypted()) g_pairingFailed = true;
  }
};

ScanCallbacks g_scanCallbacks;
ClientCallbacks g_clientCallbacks;

bool decodeReport(uint8_t* data, size_t length) {
  if (g_report.update(data, length) != 0) return false;
  g_reportCount = g_reportCount + 1;
  return true;
}

void onReportNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  decodeReport(data, length);
}

void rumble(uint8_t strongMotor, uint8_t weakMotor, uint8_t durationX10ms, uint8_t gapX10ms, uint8_t repeatCount) {
  if (g_outputReport == nullptr || g_linkState != LinkState::Connected) return;
  uint8_t report[8] = { 0x03, 0, 0, strongMotor, weakMotor, durationX10ms, gapX10ms, repeatCount };
  g_outputReport->writeValue(report, sizeof(report), !g_outputReport->canWriteNoResponse());
}

void rumbleSwitchedOn()  { rumble(60, 60, 12, 0, 0); }
void rumbleSwitchedOff() { rumble(60, 60, 7, 8, 1); }
void rumbleShort()       { rumble(30, 30, 5, 0, 0); }
void rumbleAtLimit()     { rumble(40, 40, 5, 6, 1); }

void readHidCharacteristic(NimBLERemoteService* service, uint16_t uuid) {
  NimBLERemoteCharacteristic* characteristic = service->getCharacteristic(NimBLEUUID(uuid));
  if (characteristic != nullptr && characteristic->canRead()) characteristic->readValue();
}

void startScan() {
  g_linkState = LinkState::Scanning;
  g_scanStartMs = millis();
  if (!NimBLEDevice::getScan()->start(0, false, true)) {
    g_linkState = LinkState::Idle;
    delay(500);
  }
}

void disconnect() {
  if (g_bleClient->isConnected()) g_bleClient->disconnect();
  g_linkState = LinkState::Idle;
}

void connectToController() {
  motors::stop();
  g_pairingFailed = false;
  g_disconnectPending = false;

  bool isLinked = false;
  for (int attempt = 1; attempt <= XBOX_CONNECT_RETRIES && !isLinked; attempt++) {
    isLinked = g_bleClient->connect(g_targetAddress, true, false, true);
    if (!isLinked) delay(100);
  }
  if (!isLinked) {
    g_linkState = LinkState::Idle;
    return;
  }
  if (!g_bleClient->secureConnection() || g_pairingFailed) {
    disconnect();
    return;
  }
  NimBLERemoteService* hidService = g_bleClient->getService(NimBLEUUID(HID_SERVICE_UUID));
  if (hidService == nullptr) {
    disconnect();
    return;
  }

  NimBLERemoteService* deviceInfoService = g_bleClient->getService(NimBLEUUID(DEVICE_INFO_SERVICE_UUID));
  if (deviceInfoService != nullptr) readHidCharacteristic(deviceInfoService, 0x2A50);
  readHidCharacteristic(hidService, 0x2A4A);
  readHidCharacteristic(hidService, 0x2A4E);
  readHidCharacteristic(hidService, 0x2A4B);

  int subscribedCount = 0;
  NimBLERemoteCharacteristic* inputReport = nullptr;
  g_outputReport = nullptr;
  for (NimBLERemoteCharacteristic* characteristic : hidService->getCharacteristics(true)) {
    if (characteristic->getUUID() != NimBLEUUID(HID_REPORT_UUID)) continue;
    uint8_t reportType = 0;   // 1 = entrada, 2 = saída, 3 = feature
    NimBLERemoteDescriptor* reportReference = characteristic->getDescriptor(NimBLEUUID(REPORT_REFERENCE_UUID));
    if (reportReference != nullptr) {
      const NimBLEAttValue value = reportReference->readValue();
      if (value.size() >= 2) reportType = value.data()[1];
    }
    const bool isWritable = characteristic->canWrite() || characteristic->canWriteNoResponse();
    if (reportType == 2 && isWritable && g_outputReport == nullptr) g_outputReport = characteristic;
    if (characteristic->canNotify() && characteristic->subscribe(true, onReportNotify)) {
      subscribedCount++;
      if (inputReport == nullptr) inputReport = characteristic;
    }
  }
  if (subscribedCount == 0) {
    disconnect();
    return;
  }

  bool hasInitialReport = false;
  if (inputReport != nullptr && inputReport->canRead()) {
    const NimBLEAttValue value = inputReport->readValue();
    uint8_t reportBytes[32];
    const size_t byteCount = value.size() < sizeof(reportBytes) ? value.size() : sizeof(reportBytes);
    memcpy(reportBytes, value.data(), byteCount);
    hasInitialReport = decodeReport(reportBytes, byteCount);
  }

  g_bondedAddress = g_targetAddress;
  g_hasBondedAddress = true;
  saveBondedAddress();

  g_hasReceivedReport = hasInitialReport;
  g_lastReportCount = g_reportCount;
  g_previousReport = g_report;
  g_calibrationComboStartMs = g_wifiComboStartMs = 0;
  g_calibrationComboFired = g_wifiComboFired = false;
  g_linkState = LinkState::Connected;
  rumbleSwitchedOn();
}

void adjustSpeed(float& speed, float delta) {
  speed += delta;
  bool atLimit = false;
  if (speed >= 1.0f - 0.001f)           { speed = 1.0f;           atLimit = true; }
  if (speed <= XBOX_MIN_SPEED + 0.001f) { speed = XBOX_MIN_SPEED; atLimit = true; }
  saveSpeeds();
  if (atLimit) rumbleAtLimit();
  else rumbleShort();
}

void applyControllerInput(uint32_t nowMs) {
  const uint32_t reportCount = g_reportCount;
  const XboxControllerNotificationParser report = g_report;

  if (reportCount != g_lastReportCount) {
    g_lastReportCount = reportCount;
    g_hasReceivedReport = true;
  }
  if (!g_hasReceivedReport) return;

  float moveX = normalizeAxis(report.joyLHori);
  float moveY = -normalizeAxis(report.joyLVert);
  float rotateX = normalizeAxis(report.joyRHori);
  applyRadialDeadzone(moveX, moveY, XBOX_STICK_DEADZONE);
  moveX = applyAxisDeadzone(moveX, XBOX_AXIS_DEADZONE);
  moveY = applyAxisDeadzone(moveY, XBOX_AXIS_DEADZONE);
  rotateX = applyAxisDeadzone(rotateX, XBOX_STICK_DEADZONE);

  const float rightTrigger = normalizeTrigger(report.trigRT);
  const float brake = 1.0f - XBOX_BRAKE_FACTOR * normalizeTrigger(report.trigLT);
  const float moveScale = (g_maxSpeed + rightTrigger * (1.0f - g_maxSpeed)) * brake;
  const float rotationScale = (g_rotationSpeed + rightTrigger * (1.0f - g_rotationSpeed)) * brake;
  float rotation = -rotateX * rotationScale;

  if (report.btnB) moveX = moveY = rotation = 0.0f;

  g_state.command.x = constrain(moveX * moveScale, -1.0f, 1.0f);
  g_state.command.y = constrain(moveY * moveScale, -1.0f, 1.0f);
  g_state.command.rotation = constrain(rotation, -1.0f, 1.0f);
  g_state.command.receivedMs = nowMs ? nowMs : 1;

  if (justPressed(report.btnB, g_previousReport.btnB)) g_state.stopRequested = true;
  if (justPressed(report.btnStart, g_previousReport.btnStart)) {
    g_state.isArmed = !g_state.isArmed;
    if (!g_state.isArmed) g_state.stopRequested = true;
    if (g_state.isArmed) rumbleSwitchedOn(); else rumbleSwitchedOff();
  }
  if (justPressed(report.btnSelect, g_previousReport.btnSelect)) {
    g_state.zeroYawRequested = true;
    rumbleShort();
  }
  if (justPressed(report.btnY, g_previousReport.btnY)) {
    g_state.headingHoldEnabled = !g_state.headingHoldEnabled;
    if (g_state.headingHoldEnabled) rumbleSwitchedOn(); else rumbleSwitchedOff();
  }
  if (justPressed(report.btnX, g_previousReport.btnX)) {
    g_state.fieldOrientedEnabled = !g_state.fieldOrientedEnabled;
    if (g_state.fieldOrientedEnabled) rumbleSwitchedOn(); else rumbleSwitchedOff();
  }

  if (report.btnLB && report.btnRB) {
    if (g_calibrationComboStartMs == 0) g_calibrationComboStartMs = nowMs;
    else if (!g_calibrationComboFired && (uint32_t)(nowMs - g_calibrationComboStartMs) >= XBOX_COMBO_HOLD_MS) {
      g_calibrationComboFired = true;
      g_state.compassCalibrationRequested = true;
    }
  } else {
    g_calibrationComboStartMs = 0;
    g_calibrationComboFired = false;
  }
  if (report.btnStart && report.btnSelect) {
    if (g_wifiComboStartMs == 0) g_wifiComboStartMs = nowMs;
    else if (!g_wifiComboFired && (uint32_t)(nowMs - g_wifiComboStartMs) >= XBOX_COMBO_HOLD_MS) {
      g_wifiComboFired = true;
      g_state.requestedControlMode = MODE_WIFI;
    }
  } else {
    g_wifiComboStartMs = 0;
    g_wifiComboFired = false;
  }

  if (justPressed(report.btnDirUp, g_previousReport.btnDirUp)) adjustSpeed(g_maxSpeed, +XBOX_SPEED_STEP);
  if (justPressed(report.btnDirDown, g_previousReport.btnDirDown)) adjustSpeed(g_maxSpeed, -XBOX_SPEED_STEP);
  if (justPressed(report.btnDirRight, g_previousReport.btnDirRight)) adjustSpeed(g_rotationSpeed, +XBOX_SPEED_STEP);
  if (justPressed(report.btnDirLeft, g_previousReport.btnDirLeft)) adjustSpeed(g_rotationSpeed, -XBOX_SPEED_STEP);

  g_previousReport = report;
}

}

bool init() {
  if (g_isActive) return true;
  loadSettings();
  if (!NimBLEDevice::init("OmniBot")) return false;
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  g_bleClient = NimBLEDevice::createClient();
  g_bleClient->setClientCallbacks(&g_clientCallbacks, false);
  g_bleClient->setConnectionParams(6, 12, 0, 400);
  g_bleClient->setConnectTimeout(XBOX_CONNECT_TIMEOUT_MS);

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&g_scanCallbacks, true);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(100);
  scan->setMaxResults(0);
  scan->setDuplicateFilter(false);

  g_isActive = true;
  g_linkState = LinkState::Idle;
  g_hasReceivedReport = false;
  g_disconnectPending = false;
  return true;
}

void stop() {
  if (!g_isActive) return;
  g_isActive = false;
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning()) scan->stop();
  if (g_bleClient != nullptr && g_bleClient->isConnected()) {
    g_bleClient->disconnect();
    for (int wait = 0; wait < 50 && g_bleClient->isConnected(); wait++) delay(10);
  }
  NimBLEDevice::deinit(true);
  g_bleClient = nullptr;
  g_outputReport = nullptr;
  g_linkState = LinkState::Idle;
  g_hasReceivedReport = false;
  g_disconnectPending = false;
  clearCommand();
}

void loop() {
  if (!g_isActive) return;
  const uint32_t nowMs = millis();

  if (g_disconnectPending) {
    g_disconnectPending = false;
    g_linkState = LinkState::Idle;
    g_hasReceivedReport = false;
    g_outputReport = nullptr;
    clearCommand();
    motors::stop();
  }

  switch (g_linkState) {
    case LinkState::Idle:
      startScan();
      break;
    case LinkState::Scanning:
      if ((uint32_t)(nowMs - g_scanStartMs) > 1500 && !NimBLEDevice::getScan()->isScanning()) g_linkState = LinkState::Idle;
      break;
    case LinkState::Found:
      connectToController();
      break;
    case LinkState::Connected:
      if (!g_bleClient->isConnected()) { g_disconnectPending = true; break; }
      applyControllerInput(nowMs);
      break;
  }
}

void forgetController() {
  Preferences prefs;
  if (prefs.begin("omnibot", false)) {
    prefs.remove("xbaddr");
    prefs.remove("xbtype");
    prefs.end();
  }
  g_hasBondedAddress = false;
  if (g_isActive) {
    if (g_bleClient != nullptr && g_bleClient->isConnected()) g_bleClient->disconnect();
    NimBLEDevice::deleteAllBonds();
  }
}

bool isConnected() { return g_isActive && g_linkState == LinkState::Connected && g_hasReceivedReport; }

}

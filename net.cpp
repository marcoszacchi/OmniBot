#include "net.h"
#include "config.h"
#include "state.h"
#include "web_page.h"
#include "xbox.h"
#include "motors.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <stdlib.h>
#include <string.h>

namespace net {
namespace {

WebServer g_httpServer(80);
DNSServer g_dnsServer;
WebSocketsServer g_webSocket(WEBSOCKET_PORT);
bool g_isRunning = false;

float clampUnit(float value) {
  if (isnan(value)) return 0.0f;
  return constrain(value, -1.0f, 1.0f);
}

float parseFloat(const char*& cursor) {
  char* end = nullptr;
  const float value = strtof(cursor, &end);
  if (end == cursor) return 0.0f;
  cursor = end;
  return value;
}

void sendPage() {
  g_httpServer.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void redirectToPage() {
  g_httpServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  g_httpServer.send(302, "text/plain", "");
}

void onWebSocketEvent(uint8_t, WStype_t type, uint8_t* payload, size_t length) {
  if (type != WStype_TEXT) return;
  char line[96];
  const size_t lineLength = length < sizeof(line) - 1 ? length : sizeof(line) - 1;
  memcpy(line, payload, lineLength);
  line[lineLength] = '\0';
  handleCommand(line);
}

}

void handleCommand(const char* line) {
  RobotState& state = g_state;
  const uint32_t nowMs = millis();
  const char* cursor = line + 1;

  switch (line[0]) {
    case 'C': {
      state.command.x = clampUnit(parseFloat(cursor));
      state.command.y = clampUnit(parseFloat(cursor));
      state.command.rotation = clampUnit(parseFloat(cursor));
      state.command.receivedMs = nowMs ? nowMs : 1;
      break;
    }
    case 'S':
      state.stopRequested = true;
      state.command.x = state.command.y = state.command.rotation = 0.0f;
      state.command.receivedMs = 0;
      state.motorTest.motorNumber = 0;
      state.motorTest.receivedMs = 0;
      break;
    case 'E': {
      const bool shouldArm = parseFloat(cursor) > 0.5f;
      if (!shouldArm) state.stopRequested = true;
      state.isArmed = shouldArm;
      break;
    }
    case 'H':
      state.headingHoldEnabled = parseFloat(cursor) > 0.5f;
      break;
    case 'F':
      state.fieldOrientedEnabled = parseFloat(cursor) > 0.5f;
      break;
    case 'Z':
      state.zeroYawRequested = true;
      break;
    case 'K':
      state.compassCalibrationRequested = true;
      break;
    case 'B':
      state.requestedControlMode = (parseFloat(cursor) > 0.5f) ? MODE_BLUETOOTH : MODE_WIFI;
      break;
    case 'X':
      while (*cursor == ' ') cursor++;
      if (*cursor == '0') xbox::forgetController();
      break;
    case 'D': {
      const float duty = parseFloat(cursor);
      if (!isnan(duty)) motors::setMinDuty(duty);
      break;
    }
    case 'I': {
      const int motorNumber = (int)parseFloat(cursor);
      const bool inverted = parseFloat(cursor) > 0.5f;
      if (motorNumber >= 1 && motorNumber <= 3) motors::setInverted(motorNumber - 1, inverted);
      break;
    }
    case 'T':
    case 'R': {
      const int motorNumber = (int)parseFloat(cursor);
      const float command = clampUnit(parseFloat(cursor));
      if (motorNumber >= 1 && motorNumber <= 3) {
        state.motorTest.motorNumber = motorNumber;
        state.motorTest.command = command;
        state.motorTest.isRawDuty = (line[0] == 'R');
        state.motorTest.receivedMs = (command != 0.0f) ? (nowMs ? nowMs : 1) : 0;
      }
      break;
    }
  }
}

void start() {
  static bool handlersRegistered = false;
  if (g_isRunning) return;

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, 4);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  delay(100);
  const IPAddress apIp = WiFi.softAPIP();

  g_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  g_dnsServer.start(53, "*", apIp);

  if (!handlersRegistered) {
    g_httpServer.on("/", HTTP_GET, sendPage);
    g_httpServer.onNotFound(redirectToPage);
    g_webSocket.onEvent(onWebSocketEvent);
    handlersRegistered = true;
  }
  g_httpServer.begin();
  g_webSocket.begin();
  g_isRunning = true;
}

void stop() {
  if (g_isRunning) {
    g_isRunning = false;
    g_webSocket.close();
    g_httpServer.stop();
    g_dnsServer.stop();
    WiFi.softAPdisconnect(true);
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(50);
}

void loop() {
  if (!g_isRunning) return;
  g_dnsServer.processNextRequest();
  g_httpServer.handleClient();
  g_webSocket.loop();
}

int clientCount() {
  return g_isRunning ? (int)g_webSocket.connectedClients() : 0;
}

void broadcast(const char* message) {
  if (g_isRunning) g_webSocket.broadcastTXT(message);
}

}
#pragma once
#include <Arduino.h>
#include "button.h"

// Both buttons: OTA (release after 5-10 s) -> WiFi reboot; factory reset at 10 s hold
#define OTA_HOLD_MS        5000
#define OTA_WARNING_MS     3000
#define RESET_HOLD_MS      10000
#define RESET_WARNING_MS   7000

#define BLINK_FAST_MS      150
#define OTA_WIFI_SSID      "ZbButtonAP"
#define OTA_WIFI_PASSWORD  "ZbButton"

class ButtonMonitor {
public:
  void begin(uint8_t pin1, uint8_t pin2);
  void update();

  // true -> skip Zigbee init; WiFi AP already running
  bool bootIntoOtaIfRequested();
  void clearOtaBootFlag();

  bool isOtaActive();
  bool isResetting();

private:
  uint8_t  _pin1;
  uint8_t  _pin2;

  bool     _bothHeld       = false;
  uint32_t _bothHoldStart  = 0;
  bool     _otaWarnShown   = false;
  bool     _rstWarnShown   = false;
  bool     _bothBlinkState = false;
  uint32_t _bothLastBlink  = 0;

  bool     _otaActive     = false;
  uint32_t _otaStartTime  = 0;

  void _checkBothHold();
  void _startOtaAp();
  void _stopOta();
  void _requestOtaReboot();
  void _doReset();
  void _fastBlink(bool& state, uint32_t& lastBlink);
};

extern ButtonMonitor buttonMonitor;

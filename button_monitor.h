#pragma once
#include <Arduino.h>
#include "button.h"

// Удержание одной кнопки
#define OTA_HOLD_MS      10000
#define OTA_WARNING_MS   5000

// Удержание двух кнопок
#define RESET_HOLD_MS    5000
#define RESET_WARNING_MS 2000

#define BLINK_FAST_MS    150

class ButtonMonitor {
public:
  void begin(uint8_t pin1, uint8_t pin2);
  void update();

  bool isOtaActive();
  bool isResetting();

private:
  uint8_t  _pin1;
  uint8_t  _pin2;

  // OTA — удержание кнопки 1
  bool     _otaHeld       = false;
  uint32_t _otaHoldStart  = 0;
  bool     _otaWarning    = false;
  bool     _otaActive     = false;
  uint32_t _otaStartTime  = 0;
  bool     _otaBlinkState = false;
  uint32_t _otaLastBlink  = 0;

  // Reset — удержание обеих кнопок
  bool     _rstHeld       = false;
  uint32_t _rstHoldStart  = 0;
  bool     _rstWarning    = false;
  bool     _rstBlinkState = false;
  uint32_t _rstLastBlink  = 0;

  void _checkOta();
  void _checkReset();
  void _startOta();
  void _stopOta();
  void _doReset();
  void _fastBlink(bool& state, uint32_t& lastBlink);
};

extern ButtonMonitor buttonMonitor;

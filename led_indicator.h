#pragma once
#include <Arduino.h>

#define LED_PIN LED_BUILTIN

enum LedMode {
    LED_SEARCHING,
    LED_CONNECTED,
    LED_ERROR
  };

class LedIndicator {
public:
    void begin();
    void setMode(LedMode mode);
    void forceRefresh();
    void update();
    void setRaw(bool state);  // прямое управление LED без изменения режима

private:
    LedMode  _mode        = LED_SEARCHING;
    uint32_t _lastBlink   = 0;
    bool     _ledState    = false;
    uint8_t  _blinkCount  = 0;
    bool     _blinkState  = false;

    void _setLed(bool state);
    void _updateSearching();
    void _updateConnected();
    void _updateError();
};

extern LedIndicator ledIndicator;

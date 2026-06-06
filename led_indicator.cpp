#include "led_indicator.h"

#define SEARCH_BLINK_MS    500
#define CONNECTED_BLINK_MS 5000
#define CONNECTED_ON_MS    100
#define ERROR_BLINK_MS     200
#define ERROR_PAUSE_MS     1000

LedIndicator ledIndicator;

void LedIndicator::begin() {
    pinMode(LED_PIN, OUTPUT);
    _setLed(false);
}

void LedIndicator::setMode(LedMode mode) {
    if (_mode == mode) return;
    _mode       = mode;
    _blinkCount = 0;
    _blinkState = false;
    _setLed(false);
    _lastBlink  = millis();
}

void LedIndicator::forceRefresh() {
    _lastBlink  = 0;
    _blinkCount = 0;
    _setLed(false);
}

void LedIndicator::setRaw(bool state) {
    _setLed(state);
}

void LedIndicator::_setLed(bool state) {
    // На XIAO светодиод инвертированный
    digitalWrite(LED_PIN, state ? LOW : HIGH);
    _ledState = state;
}

void LedIndicator::_updateSearching() {
    uint32_t now = millis();
    if ((now - _lastBlink) >= SEARCH_BLINK_MS) {
        _lastBlink = now;
        _setLed(!_ledState);
    }
}

void LedIndicator::_updateConnected() {
    uint32_t now = millis();
    if (!_ledState) {
        if ((now - _lastBlink) >= CONNECTED_BLINK_MS) {
            _lastBlink = now;
            _setLed(true);
        }
    } else {
        if ((now - _lastBlink) >= CONNECTED_ON_MS) {
            _setLed(false);
        }
    }
}

void LedIndicator::_updateError() {
    uint32_t now = millis();

    if (_blinkCount >= 6) {
        if ((now - _lastBlink) >= ERROR_PAUSE_MS) {
            _blinkCount = 0;
            _lastBlink  = now;
        }
        return;
    }

    if ((now - _lastBlink) >= ERROR_BLINK_MS) {
        _lastBlink  = now;
        _blinkCount++;
        _blinkState = !_blinkState;
        _setLed(_blinkState);
    }
}

void LedIndicator::update() {
    switch (_mode) {
        case LED_SEARCHING: _updateSearching(); break;
        case LED_CONNECTED: _updateConnected(); break;
        case LED_ERROR:     _updateError();     break;
    }
}

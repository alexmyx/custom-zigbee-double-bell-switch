#include "settings.h"

Settings settings;

static uint16_t clampMs(uint16_t value, uint16_t minMs, uint16_t maxMs) {
    if (value < minMs) return minMs;
    if (value > maxMs) return maxMs;
    return value;
}

void Settings::setDoubleClickMs(uint16_t ms) {
    doubleClickMs = clampMs(ms, SETTINGS_DOUBLE_MIN_MS, SETTINGS_DOUBLE_MAX_MS);
}

void Settings::setTripleClickMs(uint16_t ms) {
    tripleClickMs = clampMs(ms, SETTINGS_TRIPLE_MIN_MS, SETTINGS_TRIPLE_MAX_MS);
}

void Settings::setLongPressMs(uint16_t ms) {
    longPressMs = clampMs(ms, SETTINGS_LONG_MIN_MS, SETTINGS_LONG_MAX_MS);
}

void Settings::begin() {
    _prefs.begin("settings", false);

    doubleClickMs = _prefs.getUShort(
      "doubleClickMs",
      DEFAULT_DOUBLE_CLICK_MS
    );

    tripleClickMs = _prefs.getUShort(
      "tripleClickMs",
      DEFAULT_TRIPLE_CLICK_MS
    );

    longPressMs = _prefs.getUShort(
      "longPressMs",
      DEFAULT_LONG_PRESS_MS
    );

}

void Settings::save() {
    _prefs.putUShort("doubleClickMs", doubleClickMs);
    _prefs.putUShort("tripleClickMs", tripleClickMs);
    _prefs.putUShort("longPressMs",   longPressMs);
}

void Settings::reset() {
    _prefs.clear();
    doubleClickMs = DEFAULT_DOUBLE_CLICK_MS;
    tripleClickMs = DEFAULT_TRIPLE_CLICK_MS;
    longPressMs   = DEFAULT_LONG_PRESS_MS;
}

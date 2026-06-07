#include "settings.h"

Settings settings;

uint16_t Settings::clampDoubleClick(uint16_t v) {
    if (v < MIN_DOUBLE_CLICK_MS) return MIN_DOUBLE_CLICK_MS;
    if (v > MAX_DOUBLE_CLICK_MS) return MAX_DOUBLE_CLICK_MS;
    return v;
}

uint16_t Settings::clampTripleClick(uint16_t v) {
    if (v < MIN_TRIPLE_CLICK_MS) return MIN_TRIPLE_CLICK_MS;
    if (v > MAX_TRIPLE_CLICK_MS) return MAX_TRIPLE_CLICK_MS;
    return v;
}

uint16_t Settings::clampLongPress(uint16_t v) {
    if (v < MIN_LONG_PRESS_MS) return MIN_LONG_PRESS_MS;
    if (v > MAX_LONG_PRESS_MS) return MAX_LONG_PRESS_MS;
    return v;
}

void Settings::applyTiming(uint16_t dbl, uint16_t tpl, uint16_t lng) {
    doubleClickMs = clampDoubleClick(dbl);
    tripleClickMs = clampTripleClick(tpl);
    longPressMs   = clampLongPress(lng);
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

    applyTiming(doubleClickMs, tripleClickMs, longPressMs);
}

void Settings::save() {
    _prefs.putUShort("doubleClickMs", doubleClickMs);
    _prefs.putUShort("tripleClickMs", tripleClickMs);
    _prefs.putUShort("longPressMs",   longPressMs);
}

void Settings::reset() {
    _prefs.clear();
    applyTiming(
      DEFAULT_DOUBLE_CLICK_MS,
      DEFAULT_TRIPLE_CLICK_MS,
      DEFAULT_LONG_PRESS_MS
    );
}

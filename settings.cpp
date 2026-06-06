#include "settings.h"

Settings settings;

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

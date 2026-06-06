#include "settings.h"

Settings settings;

void Settings::begin() {
    _prefs.begin("settings", false);

    strlcpy(
      deviceName,
      _prefs.getString(
        "deviceName",
        DEFAULT_DEVICE_NAME
      ).c_str(),
      sizeof(deviceName)
    );

    strlcpy(
      otaPassword,
      _prefs.getString(
        "otaPassword",
        DEFAULT_OTA_PASSWORD
      ).c_str(),
      sizeof(otaPassword)
    );

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
    _prefs.putString("deviceName",    deviceName);
    _prefs.putString("otaPassword",   otaPassword);
    _prefs.putUShort("doubleClickMs", doubleClickMs);
    _prefs.putUShort("tripleClickMs", tripleClickMs);
    _prefs.putUShort("longPressMs",   longPressMs);
}

void Settings::reset() {
    _prefs.clear();
    strlcpy(deviceName,  DEFAULT_DEVICE_NAME,  sizeof(deviceName));
    strlcpy(otaPassword, DEFAULT_OTA_PASSWORD, sizeof(otaPassword));
    doubleClickMs = DEFAULT_DOUBLE_CLICK_MS;
    tripleClickMs = DEFAULT_TRIPLE_CLICK_MS;
    longPressMs   = DEFAULT_LONG_PRESS_MS;
}

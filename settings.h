#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define DEFAULT_DEVICE_NAME      "zigbee-button"
#define DEFAULT_OTA_PASSWORD     "your_password"
#define DEFAULT_DOUBLE_CLICK_MS  400
#define DEFAULT_TRIPLE_CLICK_MS  400
#define DEFAULT_LONG_PRESS_MS    800

class Settings {
public:
    char     deviceName[32];
    char     otaPassword[32];
    uint16_t doubleClickMs;
    uint16_t tripleClickMs;
    uint16_t longPressMs;

    void begin();
    void save();
    void reset();

private:
    Preferences _prefs;
};

extern Settings settings;

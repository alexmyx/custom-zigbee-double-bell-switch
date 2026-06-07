#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define DEFAULT_DOUBLE_CLICK_MS  400
#define DEFAULT_TRIPLE_CLICK_MS  400
#define DEFAULT_LONG_PRESS_MS    800

#define SETTINGS_DOUBLE_MIN_MS   200
#define SETTINGS_DOUBLE_MAX_MS   1000
#define SETTINGS_TRIPLE_MIN_MS   200
#define SETTINGS_TRIPLE_MAX_MS   1500
#define SETTINGS_LONG_MIN_MS     500
#define SETTINGS_LONG_MAX_MS     3000

class Settings {
public:
    uint16_t doubleClickMs;
    uint16_t tripleClickMs;
    uint16_t longPressMs;

    void begin();
    void save();
    void reset();

    void setDoubleClickMs(uint16_t ms);
    void setTripleClickMs(uint16_t ms);
    void setLongPressMs(uint16_t ms);

private:
    Preferences _prefs;
};

extern Settings settings;

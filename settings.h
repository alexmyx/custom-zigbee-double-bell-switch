#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define DEFAULT_DOUBLE_CLICK_MS  400
#define DEFAULT_TRIPLE_CLICK_MS  400
#define DEFAULT_LONG_PRESS_MS    800

class Settings {
public:
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

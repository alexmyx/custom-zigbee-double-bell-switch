#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define DEFAULT_DOUBLE_CLICK_MS  400
#define DEFAULT_TRIPLE_CLICK_MS  400
#define DEFAULT_LONG_PRESS_MS    800

#define MIN_DOUBLE_CLICK_MS      200
#define MAX_DOUBLE_CLICK_MS      1000
#define MIN_TRIPLE_CLICK_MS      200
#define MAX_TRIPLE_CLICK_MS      1500
#define MIN_LONG_PRESS_MS        500
#define MAX_LONG_PRESS_MS        3000

class Settings {
public:
    uint16_t doubleClickMs;
    uint16_t tripleClickMs;
    uint16_t longPressMs;

    void begin();
    void save();
    void reset();

    static uint16_t clampDoubleClick(uint16_t v);
    static uint16_t clampTripleClick(uint16_t v);
    static uint16_t clampLongPress(uint16_t v);
    void applyTiming(uint16_t dbl, uint16_t tpl, uint16_t lng);

private:
    Preferences _prefs;
};

extern Settings settings;

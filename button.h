#pragma once
#include <Arduino.h>

#define DEBOUNCE_MS  50

enum ButtonEvent {
    BTN_NONE,
    BTN_SINGLE,
    BTN_DOUBLE,
    BTN_TRIPLE,
    BTN_LONG
  };

struct Button {
    uint8_t  pin;
    uint8_t  clickCount;
    uint32_t lastPressTime;
    uint32_t pressStartTime;
    bool     isPressed;
    bool     longPressFired;
    bool     waitingSecond;

    Button(uint8_t _pin) :
      pin(_pin),
      clickCount(0),
      lastPressTime(0),
      pressStartTime(0),
      isPressed(false),
      longPressFired(false),
      waitingSecond(false) {}

    void begin() {
        pinMode(pin, INPUT_PULLUP);
    }

    ButtonEvent read();
};

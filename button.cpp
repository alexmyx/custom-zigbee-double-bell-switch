#include "button.h"
#include "settings.h"
#include "version.h"

ButtonEvent Button::read() {
    bool     currentState  = (digitalRead(pin) == LOW);
    uint32_t now           = millis();
    ButtonEvent event      = BTN_NONE;

    uint16_t doubleClickMs = settings.doubleClickMs;
    uint16_t tripleClickMs = settings.tripleClickMs;
    uint16_t longPressMs   = settings.longPressMs;

    // Нажатие
    if (currentState && !isPressed) {
        if ((now - lastPressTime) > DEBOUNCE_MS) {
            isPressed      = true;
            pressStartTime = now;
            longPressFired = false;
        }
    }

    // Удержание
    if (currentState && isPressed && !longPressFired) {
        if ((now - pressStartTime) >= longPressMs) {
            longPressFired = true;
            waitingSecond  = false;
            clickCount     = 0;
            event          = BTN_LONG;
        }
    }

    // Отпускание
    if (!currentState && isPressed) {
        isPressed     = false;
        lastPressTime = now;
        if (!longPressFired) {
            if (clickCount < 3) clickCount++;
            waitingSecond = true;
        }
    }

    // Таймаут: после 1 нажатия ждём doubleClickMs,
    //          после 2+ — tripleClickMs
    if (waitingSecond && !isPressed) {
        uint16_t timeout = (clickCount >= 2) ? tripleClickMs : doubleClickMs;
        if ((now - lastPressTime) > timeout) {
            waitingSecond = false;
            if      (clickCount == 1) event = BTN_SINGLE;
            else if (clickCount == 2) event = BTN_DOUBLE;
            else                      event = BTN_TRIPLE;
            clickCount = 0;
        }
    }

    return event;
}

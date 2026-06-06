#include "version.h"
#include "settings.h"
#include "button.h"
#include "led_indicator.h"
#include "button_monitor.h"
#include "zigbee_handler.h"
#include "Zigbee.h"
#include <esp_task_wdt.h>

#define PIN_BUTTON_1  D0  // GPIO 0
#define PIN_BUTTON_2  D1  // GPIO 1
#define WDT_TIMEOUT   10

Button btn1(PIN_BUTTON_1);
Button btn2(PIN_BUTTON_2);

void handleEvent(uint8_t idx, ButtonEvent event) {
    if (event == BTN_NONE) return;
    zigbeeSendAction(idx, event);
}

void setup() {
#ifdef APP_DEBUG
    Serial.begin(115200);
    delay(1000);
#endif

    const esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms     = WDT_TIMEOUT * 1000,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    esp_task_wdt_reconfigure(&wdt_cfg);

    settings.begin();
    ledIndicator.begin();
    ledIndicator.setMode(LED_SEARCHING);

    btn1.begin();
    btn2.begin();

    buttonMonitor.begin(PIN_BUTTON_1, PIN_BUTTON_2);

    zigbeeInit();

    // WDT после Zigbee.begin() — begin() может ждать стек до 30 с
    esp_task_wdt_add(NULL);
}

void loop() {
    esp_task_wdt_reset();

#ifdef APP_DEBUG
    static uint32_t _lastAlive = 0;
    if ((millis() - _lastAlive) >= 3000) {
        _lastAlive = millis();
        Serial.printf("[ALIVE] uptime=%lus joined=%d lqi=%d\n",
            millis() / 1000, (int)zigbeeIsConnected(), (int)zigbeeGetLinkQuality());
    }

#endif

    buttonMonitor.update();
    zigbeeLoop();
    ledIndicator.update();

    if (!buttonMonitor.isOtaActive()) {
        handleEvent(1, btn1.read());
        handleEvent(2, btn2.read());
    }

    delay(10);
}

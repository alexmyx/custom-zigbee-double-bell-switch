# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for a dual-button Zigbee smart device based on **Seeed Studio XIAO ESP32C6**. Written in C++ (Arduino sketch). Code comments and UI strings are in Russian.

## Build & Flash

Сборка через **PlatformIO** (`platformio.ini` в корне проекта):

```bash
pio run                        # сборка
pio run --target upload        # прошивка
pio device monitor             # серийный монитор (115200 бод)
pio project init --ide clion   # пересоздать CMakeLists.txt для CLion
```

Arduino IDE: открыть `zigbee_button.ino`, выбрать плату **XIAO ESP32C6**.

Отладочный вывод: раскомментировать `#define DEBUG` в `version.h`.

Необходимые библиотеки (устанавливаются автоматически через PlatformIO):
- `ArduinoJson` — указана в `lib_deps`
- ESP32 Arduino core (включает `WiFi.h`, `WebServer.h`, `Update.h`, `Preferences.h`, `Zigbee.h`, `esp_task_wdt.h`)

## Zigbee — ключевые решения

Устройство **маскируется под Aqara WXKG07LM** (`lumi.remote.b286acn02`):
- `manufacturer` = `LUMI`
- `zigbeeModel` = `lumi.remote.b286acn02`
- z2m определяет его как **"Aqara WXKG07LM Wireless remote switch D1 (double rocker)"** без дополнительных конвертеров

Кластер: **genMultistateInput (0x0012)**, атрибут `present_value` (0x0055):

| present_value | Действие | z2m action (EP1) | z2m action (EP2) |
|---|---|---|---|
| 1 | single | `single_left` | `single_right` |
| 2 | double | `double_left` | `double_right` |
| 3 | triple | `triple_left` | `triple_right` |
| 0 | hold | `hold_left` | `hold_right` |

Эндпоинт 1 → кнопка 1 (left), эндпоинт 2 → кнопка 2 (right).

Сборка требует специальной таблицы разделов и sdkconfig для Zigbee router:
```ini
board_build.partitions = zigbee_zczr.csv
board_build.sdkconfig_options =
    CONFIG_ZB_ENABLED=y
    CONFIG_ZB_ZCZR=y
```

## Architecture

### Module responsibilities

| File | Responsibility |
|------|----------------|
| `zigbee_button.ino` | Entry point — initializes all subsystems, runs main loop (10 ms tick) |
| `button.h/cpp` | Single-button event detection: single click, double click, triple click, long press |
| `button_monitor.h/cpp` | Multi-button coordination: OTA mode (hold btn1 10 s), factory reset (hold both 5 s), web server |
| `led_indicator.h/cpp` | Status LED state machine: SEARCHING / CONNECTED / ERROR |
| `settings.h/cpp` | Persistent config via ESP32 `Preferences` (device name, OTA password, timing) |
| `zigbee_handler.h/cpp` | Zigbee stack init (router mode), two `ZigbeeMultistate` endpoints, sends action reports |
| `version.h` | `FW_VERSION`, build date/time, `LOG()` macro |

### Data flow

```
GPIO 2/3 (buttons)
  → Button::read()          — debounce + classify event (50 ms debounce)
  → ButtonMonitor::update() — intercepts events for OTA/reset; passes rest through
  → zigbeeSendAction()      — reports present_value on the appropriate ZigbeeMultistate endpoint

Zigbee connection state → LedIndicator (SEARCHING blink 500 ms / CONNECTED pulse 5 s / ERROR 3×200 ms)
```

### Button events

| Event | Timing |
|-------|--------|
| Single click | timeout `doubleClickMs` (default 400 ms) after release |
| Double click | 2 clicks within `doubleClickMs` window |
| Triple click | 3 clicks, timeout `tripleClickMs` (default 400 ms) after 2nd release |
| Long press | held ≥ `longPressMs` (default 800 ms) |

### OTA / web interface

Activated by holding button 1 for 10 s. Device starts a WiFi AP (`zigbee-button`, password from settings) and serves HTTP on `192.168.0.100`:

- `GET /` — HTML configuration UI (stored in PROGMEM)
- `POST /update` — firmware upload (password protected)
- `POST /settings` — save JSON config (`name`, `pass`, `dbl`, `tpl`, `lng`)
- `POST /reset-settings` — factory reset Zigbee bindings

### Key constants (defaults)

| Constant | Value | Location |
|----------|-------|----------|
| `PIN_BUTTON_1` / `PIN_BUTTON_2` | GPIO 2 / 3 | `zigbee_button.ino` |
| `DEBOUNCE_MS` | 50 ms | `button.h` |
| `DEFAULT_DOUBLE_CLICK_MS` | 400 ms | `settings.h` |
| `DEFAULT_TRIPLE_CLICK_MS` | 400 ms | `settings.h` |
| `DEFAULT_LONG_PRESS_MS` | 800 ms | `settings.h` |
| `OTA_HOLD_MS` | 10 000 ms | `button_monitor.h` |
| `RESET_HOLD_MS` | 5 000 ms | `button_monitor.h` |
| `WDT_TIMEOUT` | 10 s | `zigbee_button.ino` |

## Important notes

- **All timing is non-blocking** — use `millis()` comparisons, never `delay()` inside event handlers.
- **Buttons are active-low** with internal pull-ups enabled.
- **LED logic is inverted** — `ledIndicator.setRaw(true)` = LED on (calls `digitalWrite(LED_PIN, LOW)`).
- **Watchdog timer is 10 s** — long synchronous operations must call `esp_task_wdt_reset()`.
- **`LedIndicator::setRaw(bool)`** — use this (not `digitalWrite` directly) to keep internal LED state in sync.
- There are no automated tests; verification is done via serial debug output and manual device testing.
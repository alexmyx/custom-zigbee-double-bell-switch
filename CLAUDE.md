# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for a dual-button Zigbee smart device based on **Seeed Studio XIAO ESP32C6**. Written in C++ (Arduino sketch). All comments, logs, and UI strings are in English.

**Working baseline — do not break without explicit request:**
- Buttons report via `_reportToCoordinator(0x0000)` on `genMultistateInput` / `present_value`
- Battery/voltage in MQTT via Lumi **`genBasic 0xFF01`** (manufacturer attr, code `0x115F`) — `3000 mV` -> `battery: 100` in stock z2m
- `device_temperature` via Lumi TLV tag 3 in the same `0xFF01` blob (chip internal sensor)
- `setMultistateInputStates(4)` on both endpoints (single/double/triple/hold)
- Direct report path in `zigbee_handler.cpp`; finding & binding on join
- `esp_zb_secur_network_min_join_lqi_set(0)` and `zigbeeSteeringKick` after 60 s for factory-new devices

## Build & Flash

Build with **PlatformIO** (`platformio.ini` in project root):

```bash
pio run                        # build
pio run --target upload        # flash
pio device monitor             # serial monitor (115200 baud)
pio project init --ide clion   # regenerate CMakeLists.txt for CLion
```

Arduino IDE: open `zigbee_button.ino`, select board **XIAO ESP32C6**.

Platform: **pioarduino** fork (`platform-espressif32` stable zip).

Application debug output: `#define APP_DEBUG` in `version.h` (comment out for release builds). ESP-IDF verbose: `CORE_DEBUG_LEVEL=0` in `platformio.ini`.

Dependencies (installed automatically via PlatformIO):
- `ArduinoJson` — listed in `lib_deps`
- ESP32 Arduino core (includes `WiFi.h`, `WebServer.h`, `Update.h`, `Preferences.h`, `Zigbee.h`, `esp_task_wdt.h`)

## Zigbee — key decisions

**Role:** Zigbee **End Device** (`ZIGBEE_MODE_ED`, `CONFIG_ZB_ZED=y`), not router.

Device **masquerades as Aqara WXKG07LM** (`lumi.remote.b286acn02`):
- `manufacturer` = `LUMI`
- `zigbeeModel` = `lumi.remote.b286acn02`
- z2m identifies it as **"Aqara WXKG07LM Wireless remote switch D1 (double rocker)"** without external converters

Cluster: **genMultistateInput (0x0012)**, attribute `present_value` (0x0055):

| present_value | Action | z2m action (EP1) | z2m action (EP2) |
|---|---|---|---|
| 1 | single | `single_left` | `single_right` |
| 2 | double | `double_left` | `double_right` |
| 3 | triple | `triple_left` | `triple_right` |
| 0 | hold | `hold_left` | `hold_right` |

Endpoint 1 -> button 1 (left), endpoint 2 -> button 2 (right).

Button reports: `setMultistateInput()` + `_reportPresentValue()` -> `_reportToCoordinator()` to coordinator `0x0000`.

**Battery / voltage / temperature in z2m:**
- EP1: `genPowerCfg` (battery 100%, 3.0 V)
- EP1: manufacturer attr `genBasic 0xFF01` — TLV blob `[1,0x21,3000 mV LE]` + `[3,0x28,temp°C]` (code `0x115F`)
- Reports: 10 s after join + at most once per 60 s on button press

Build requires custom partition table and sdkconfig:
```ini
board_build.partitions = zigbee_zczr_4mb.csv
board_build.sdkconfig_options =
    CONFIG_ZB_ENABLED=y
    CONFIG_ZB_ZED=y
build_flags =
    -DZIGBEE_MODE_ED
```

## Architecture

### Module responsibilities

| File | Responsibility |
|------|----------------|
| `zigbee_button.ino` | Entry point — init, OTA-boot check, main loop (10 ms tick) |
| `button.h/cpp` | Single-button event detection: single, double, triple, long press |
| `button_monitor.h/cpp` | Both-button OTA trigger, factory reset, WiFi AP + web server |
| `led_indicator.h/cpp` | Status LED: SEARCHING / CONNECTED / ERROR |
| `settings.h/cpp` | Persistent config via `Preferences` (click timing only) |
| `zigbee_handler.h/cpp` | Zigbee ED init, two `ZigbeeMultistate` endpoints, action reports, voltage/temp |
| `version.h` | `FW_VERSION`, build date/time, `LOG()` / `APP_DEBUG` |

### Data flow

```
D0/D1 (GPIO 0/1, buttons, active-low)
  -> Button::read()          — debounce + classify event (50 ms debounce)
  -> ButtonMonitor::update() — OTA/reset gestures; blocks button events in OTA mode
  -> zigbeeSendAction()      — present_value report to coordinator on correct EP

Zigbee connection state -> LedIndicator (SEARCHING blink 500 ms / CONNECTED pulse 5 s / ERROR 3x200 ms)
```

### Button events

| Event | Timing |
|-------|--------|
| Single click | timeout `doubleClickMs` (default 400 ms) after release |
| Double click | 2 clicks within `doubleClickMs` window |
| Triple click | 3 clicks, timeout `tripleClickMs` (default 400 ms) after 2nd release |
| Long press | held >= `longPressMs` (default 800 ms) |

### OTA / web interface

**Why reboot:** ESP32-C6 cannot run WiFi AP reliably after Zigbee init. OTA uses a separate boot without Zigbee.

**Enter OTA:** both buttons **5-10 s -> release** -> NVS flag `ota_boot` -> `ESP.restart()` -> `bootIntoOtaIfRequested()` starts WiFi AP only.

**Exit OTA:** `POST /reboot-zigbee` (web button after saving settings), successful firmware upload, or 5 min session timeout.

**Factory reset:** both buttons **>= 10 s without release** -> `settings.reset()` + `Zigbee.factoryReset()` -> reboot.

**Gestures (fast LED blink):**
- 3-5 s hold — warning "2 seconds left to OTA"
- 7-10 s hold — warning "3 seconds left to factory reset"

WiFi AP: IP **`192.168.0.100`**, SSID **`ZbButtonAP`**, password **`ZbButton`** (hardcoded). OTA session: **5 min**, then reboot.

Web UI (PROGMEM):
- `GET /` — single page: firmware upload + button timing
- `POST /update` — upload `.bin` (no password)
- `POST /settings` — JSON: `dbl`, `tpl`, `lng`
- `POST /reboot-zigbee` — clear `ota_boot` and reboot to Zigbee mode

Serial on OTA start: `[OTA] WiFi SSID="ZbButtonAP" pass="ZbButton"`.

### Key constants (defaults)

| Constant | Value | Location |
|----------|-------|----------|
| `PIN_BUTTON_1` / `PIN_BUTTON_2` | D0 / D1 (GPIO 0 / 1) | `zigbee_button.ino` |
| `OTA_WIFI_SSID` | `"ZbButtonAP"` | `button_monitor.h` |
| `OTA_WIFI_PASSWORD` | `"ZbButton"` | `button_monitor.h` |
| `DEBOUNCE_MS` | 50 ms | `button.h` |
| `DEFAULT_DOUBLE_CLICK_MS` | 400 ms | `settings.h` |
| `DEFAULT_TRIPLE_CLICK_MS` | 400 ms | `settings.h` |
| `DEFAULT_LONG_PRESS_MS` | 800 ms | `settings.h` |
| `OTA_HOLD_MS` | 5 000 ms | `button_monitor.h` |
| `RESET_HOLD_MS` | 10 000 ms | `button_monitor.h` |
| `OTA session timeout` | 300 000 ms (5 min) | `button_monitor.cpp` |
| `WDT_TIMEOUT` | 10 s | `zigbee_button.ino` |

## Important notes

- **All timing is non-blocking** — use `millis()` comparisons; `delay()` only in setup/reboot paths.
- **Buttons are active-low** with internal pull-ups enabled.
- **LED logic is inverted** — `ledIndicator.setRaw(true)` = LED on (`digitalWrite(LED_PIN, LOW)`).
- **Watchdog timer is 10 s** — `esp_task_wdt_add()` after `zigbeeInit()` on normal boot; before `_startOtaAp()` on OTA boot. Call `esp_task_wdt_reset()` during OTA upload.
- **`LedIndicator::setRaw(bool)`** — use this (not `digitalWrite` directly) to keep internal LED state in sync.
- **Settings in NVS** — only `doubleClickMs`, `tripleClickMs`, `longPressMs`.
- **No framework patches** — do not add `extra_script.py` / Arduino-Zigbee patches without explicit request.
- There are no automated tests; verification is done via serial debug output and manual device testing.

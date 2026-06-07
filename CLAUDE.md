# CLAUDE.md

**Session handoff:** see [`CONTINUATION.md`](CONTINUATION.md) for branch state, decisions, and TODO after chat reset.

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for a dual-button Zigbee smart device based on **Seeed Studio XIAO ESP32C6**. Written in C++ (Arduino sketch). All comments, logs, and UI strings are in English.

**Device identity (custom device branch):**
- Vendor: `alexmyx`
- Model: `Zigbee double button`
- Manufacturer code: `0x1378` (provisional — see `zigbee_constants.h`)
- z2m: external converter in `z2m/zb_button.js`

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

Application debug output: `#define APP_DEBUG` in `version.h` (comment out for release builds).

Dependencies (installed automatically via PlatformIO):
- ESP32 Arduino core (includes `WiFi.h`, `WebServer.h`, `Update.h`, `Preferences.h`, `Zigbee.h`, `esp_task_wdt.h`)

## Zigbee — key decisions

**Role:** Zigbee **Router** (`ZIGBEE_MODE_ZCZR`, `CONFIG_ZB_ZCZR=y`).

**Endpoints:**
- EP1 — button 1 + config cluster + device temperature
- EP2 — button 2

**Button actions:** cluster **genMultistateInput (0x0012)**, attribute `present_value`:

| present_value | Action | z2m action (EP1) | z2m action (EP2) |
|---|---|---|---|
| 1 | single | `single_button_1` | `single_button_2` |
| 2 | double | `double_button_1` | `double_button_2` |
| 3 | triple | `triple_button_1` | `triple_button_2` |
| 0 | hold | `hold_button_1` | `hold_button_2` |
| 255 | release (after hold) | `release_button_1` | `release_button_2` |

Reports: `setMultistateInput()` + direct `_reportToCoordinator()` to coordinator `0x0000`.

**Timing configuration:** manufacturer cluster `0xFC01` on EP1 (RW UINT16 attrs):
- `0x0000` — `double_click_ms` (200–1000, default 400)
- `0x0001` — `triple_click_ms` (200–1500, default 400)
- `0x0002` — `long_press_ms` (500–3000, default 800)

Writable only via z2m (Zigbee Write Attribute). Values persist in NVS.

**Temperature:** standard **genDeviceTempConfig** on EP1 (`device_temperature` in z2m).

**Power:** mains (`ZB_POWER_SOURCE_MAINS`), no battery clusters.

Build requires custom partition table and sdkconfig:
```ini
board_build.partitions = zigbee_zczr_4mb.csv
board_build.sdkconfig_options =
    CONFIG_ZB_ENABLED=y
    CONFIG_ZB_ZCZR=y
build_flags =
    -DZIGBEE_MODE_ZCZR
```

## Architecture

### Module responsibilities

| File | Responsibility |
|------|----------------|
| `zigbee_button.ino` | Entry point — init, OTA-boot check, main loop (10 ms tick) |
| `button.h/cpp` | Single-button event detection |
| `button_monitor.h/cpp` | Both-button OTA trigger, factory reset, WiFi AP + web server |
| `led_indicator.h/cpp` | Status LED: SEARCHING / CONNECTED / ERROR |
| `settings.h/cpp` | Persistent timing via `Preferences` + clamp helpers |
| `zigbee_constants.h` | Vendor/model/manufacturer code/cluster IDs |
| `zigbee_handler.h/cpp` | Zigbee router init, endpoints, action reports, config/temp |
| `z2m/zb_button.js` | External converter for Zigbee2MQTT |
| `version.h` | `FW_VERSION`, build date/time, `LOG()` / `APP_DEBUG` |

### OTA / web interface

**Enter OTA:** both buttons **5-10 s -> release** -> reboot to WiFi AP mode.

**Exit OTA:** successful firmware upload or 5 min session timeout (auto-reboot to Zigbee).

**Factory reset:** both buttons **>= 10 s** -> `settings.reset()` + `Zigbee.factoryReset()`.

WiFi AP: IP `192.168.0.100`, SSID `ZbButtonAP`, password `ZbButton`.

Web UI: firmware upload only (`POST /update`).

## Important notes

- **All timing is non-blocking** — use `millis()` comparisons.
- **Buttons are active-low** with internal pull-ups.
- **LED logic is inverted** — `ledIndicator.setRaw(true)` = LED on.
- **Watchdog timer is 10 s** — reset during OTA upload.
- **Re-pair required** after identity change.
- No automated tests; verify via serial output and manual testing.

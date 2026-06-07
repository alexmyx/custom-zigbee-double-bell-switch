# Continuation context (session handoff)

Last updated: 2026-06-05. Read this file after chat/session reset to continue work on `zigbee_button`.

## Branch & git state

- **Active branch:** `custom_device` (diverged from `main` at `c572f31 third commit`)
- **Not committed yet** (as of last session):
  - Modified: `CLAUDE.md`, `button_monitor.cpp/h`, `platformio.ini`, `settings.cpp/h`, `zigbee_handler.cpp/h`
  - New: `zigbee_constants.h`, `z2m/zb_button.js`, `z2m/README.md`
- User commits manually when ready; do not commit unless asked.

## Project goal

Dual-button Zigbee router on **Seeed XIAO ESP32C6**. Migrated from **Aqara WXKG07LM masquerade** to **own device** (`alexmyx`). Full button logic kept (single/double/triple/hold, OTA, factory reset, LED). z2m support via external converter; upstream PR to `zigbee-herdsman-converters` planned by user later.

## Key decisions (confirmed by user)

| Topic | Decision |
|-------|----------|
| Vendor / model | `alexmyx` / `Zigbee double button` |
| Manufacturer code | Provisional **`0x1378`** (CSA official code needed before z2m PR) |
| Zigbee role | **Router** (`ZIGBEE_MODE_ZCZR`, `CONFIG_ZB_ZCZR=y`) |
| Endpoints | EP1 = button 1 + config + temp; EP2 = button 2; **no EP3 / no `*_both`** |
| Button cluster | `genMultistateInput` (0x0012), `present_value`: 0=hold, 1=single, 2=double, 3=triple |
| z2m action names | `single_button_1`, `double_button_2`, `hold_button_1`, etc. (not Aqara `*_left`) |
| Timing config | **Zigbee only** (cluster `0xFC01` on EP1); **removed from OTA web** |
| OTA web | Firmware upload only; exit = successful flash or 5 min timeout |
| Temperature | **Keep** — standard `genDeviceTempConfig` on EP1 |
| Battery / Aqara | **Removed** — mains power, no Lumi 0xFF01, no genPowerCfg fake |
| Yandex direct | **Not needed** — no Aqara compat profile |
| Button reports | Direct `_reportToCoordinator(0x0000)` + fallback `reportMultistateInput()` |
| Finding & binding | **Removed** (was for Aqara) |

## ZCL map (firmware ↔ z2m)

Constants in `zigbee_constants.h`; mirror in `z2m/zb_button.js`.

| Item | Value |
|------|-------|
| Config cluster | `0xFC01`, manuf `0x1378` |
| `double_click_ms` | attr `0x0000`, 200–1000 ms, default 400 |
| `triple_click_ms` | attr `0x0001`, 200–1500 ms, default 400 |
| `long_press_ms` | attr `0x0002`, 500–3000 ms, default 800 |

Config ZCL attrs point directly at `settings.doubleClickMs` etc. (no shadow copies). Write via z2m → `zbAttributeSet` → clamp → NVS.

## File map

| File | Role |
|------|------|
| `zigbee_button.ino` | Entry, 10 ms loop, GPIO D0/D1 |
| `button.cpp/h` | Click state machine, reads `settings.*` |
| `button_monitor.cpp/h` | OTA AP, factory reset, both-button gestures |
| `settings.cpp/h` | NVS timing + clamp helpers |
| `zigbee_constants.h` | Vendor, cluster IDs, manuf code |
| `zigbee_handler.cpp/h` | Zigbee init, EP1 class with config/temp, actions |
| `z2m/zb_button.js` | External converter for Zigbee2MQTT |
| `z2m/README.md` | Install converter + ZCL reference |
| `CLAUDE.md` | Build/architecture guide for AI |

## Build

```bash
pio run
pio run --target upload
pio device monitor   # 115200
```

Debug: `#define APP_DEBUG` in `version.h`. LQI logs and steering kick only with `APP_DEBUG`.

## z2m setup

1. Copy `z2m/zb_button.js` → `<z2m>/external_converters/`
2. Restart Zigbee2MQTT
3. **Remove old device** (was Aqara fingerprint), re-pair
4. Fingerprint: `zigbeeModel: ['Zigbee double button']`, vendor `alexmyx`

## Testing checklist (NOT done on hardware yet)

- [ ] Re-pair, device recognized with external converter
- [ ] Actions on EP1/EP2 (`single_button_1`, …)
- [ ] Write `double_click_ms` / `triple_click_ms` / `long_press_ms` from z2m
- [ ] `device_temperature` visible
- [ ] OTA: both buttons 5–10 s → upload → reboot to Zigbee
- [ ] Factory reset: both ≥10 s → defaults 400/400/800, re-join
- [ ] **Risk:** config cluster Write Attribute on ESP stack — verify on device

## Remaining work

1. **Hardware test** (above checklist)
2. **Git commit** on `custom_device` when user wants
3. **PR to z2m** — user will do; needs CSA manufacturer code
4. Optional: `APP_DEBUG` off for release; `.gitignore` for `.idea/`

## Code simplifications already applied

- OTA: no timing UI, no ArduinoJson, no `/settings` / `/reboot-zigbee`
- Config attrs → `&settings.*`, table `_configAttrs[]`, no `syncConfigAttributes`
- Single `_readChipTempC()`; temp reporting kept (10 s after join, then every 5 min)

## Do NOT revert without explicit request

- Router mode (not ED)
- Own identity (not LUMI / `lumi.remote.b286acn02`)
- Direct report to coordinator for button events

## Conversation reference

Prior chat transcript: `.cursor/projects/.../agent-transcripts/1839cad0-ca18-4806-aed5-74b2d78dd73d.jsonl`

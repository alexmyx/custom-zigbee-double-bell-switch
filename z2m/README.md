# Zigbee2MQTT external converter

## Install (Zigbee2MQTT 2.11+)

Per [External converters](https://www.zigbee2mqtt.io/advanced/more/external_converters.html):

1. Copy **`zb_button.mjs`** into **`external_converters/`** next to `configuration.yaml`.
2. Delete `zb_button.js`, `zb_button.js.invalid` if present — deploy **only** the `.mjs` file.
3. In `configuration.yaml`:

```yaml
advanced:
  enable_external_js: true
```

4. Remove legacy `external_converters:` list from `configuration.yaml` (z2m 2.x auto-loads the folder).
5. Restart Zigbee2MQTT — no `Failed to load external converter` in the log.
6. Reconfigure or re-pair the device.

Converter layout:

- **Buttons** — classic `fromZigbee` on `genMultistateInput`
- **Timing** — manufacturer UINT16 on `genBasic` (`deviceAddCustomCluster` + `0x1378` / `0x0000–0x0002`)
- **Temperature** — `deviceTemperature()` modern extend only

### If z2m renames the file to `.invalid`

Load failed — use `.mjs` with `export default`, enable `enable_external_js`, check z2m log for the exact error.

## Device identity

| Field | Value |
|-------|-------|
| Vendor | `alexmyx` |
| Model | `Zigbee double button` |
| Manufacturer code | `0x1378` (provisional) |

## Manufacturer code

Zigbee manufacturer codes are assigned by the Connectivity Standards Alliance (CSA).  
`0x1378` is a **provisional development code** used in firmware and in this converter.  
Before opening a PR to [zigbee-herdsman-converters](https://github.com/Koenkk/zigbee-herdsman-converters), request an official code from CSA and update:

- `zigbee_constants.h` → `ZB_MANUF_CODE_ALEXMYX`
- `z2m/zb_button.js` → `ALEXMYX_MANUF_CODE`

## Clusters

| Cluster | Endpoint | Purpose |
|---------|----------|---------|
| `genMultistateInput` | 1, 2 | Button actions (`present_value`) |
| `genBasic` (manuf `0x1378`, attrs `0x0000–0x0002`) | 1 | Timing configuration |
| `genDeviceTempConfig` | 1 | Chip temperature |

## Actions (MQTT)

- `single_button_1`, `double_button_1`, `triple_button_1`, `hold_button_1`, `release_button_1`
- `single_button_2`, `double_button_2`, `triple_button_2`, `hold_button_2`, `release_button_2`

## Configuration (Zigbee Write Attribute)

| Expose | Attribute | Range (ms) | Default |
|--------|-----------|------------|---------|
| `double_click_ms` | `0x0000` | 200–1000 | 400 |
| `triple_click_ms` | `0x0001` | 200–1500 | 400 |
| `long_press_ms` | `0x0002` | 500–3000 | 800 |

Writable only via z2m (Zigbee Write Attribute). OTA web UI is firmware-only.

## Re-pairing

After flashing firmware with the new identity, remove the old device from Zigbee2MQTT and pair again.

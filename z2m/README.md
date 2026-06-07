# Zigbee2MQTT external converter

Copy `zb_button.js` to the Zigbee2MQTT `external_converters` folder (next to `configuration.yaml`), then restart Zigbee2MQTT.

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
| `manuSpecificAlexmyxBtnConfig` (`0xFC01`) | 1 | Timing configuration |
| `genDeviceTempConfig` | 1 | Chip temperature |

## Actions (MQTT)

- `single_button_1`, `double_button_1`, `triple_button_1`, `hold_button_1`
- `single_button_2`, `double_button_2`, `triple_button_2`, `hold_button_2`

## Configuration (Zigbee Write Attribute)

| Expose | Attribute | Range (ms) | Default |
|--------|-----------|------------|---------|
| `double_click_ms` | `0x0000` | 200–1000 | 400 |
| `triple_click_ms` | `0x0001` | 200–1500 | 400 |
| `long_press_ms` | `0x0002` | 500–3000 | 800 |

Writable only via z2m (Zigbee Write Attribute). OTA web UI is firmware-only.

## Re-pairing

After flashing firmware with the new identity, remove the old device from Zigbee2MQTT and pair again.

#pragma once

// Provisional manufacturer code for development. Request an official code from CSA
// (https://csa-iot.org/) before merging into zigbee-herdsman-converters.
#define ZB_MANUF_CODE_ALEXMYX    0x1378

// Timing attrs: manufacturer-specific UINT16 on genBasic (manuf ZB_MANUF_CODE_ALEXMYX)
#define ZB_ATTR_DOUBLE_CLICK_MS  0x0000
#define ZB_ATTR_TRIPLE_CLICK_MS  0x0001
#define ZB_ATTR_LONG_PRESS_MS    0x0002

#define ZB_VENDOR_NAME           "alexmyx"
#define ZB_MODEL_NAME            "Zigbee double button"

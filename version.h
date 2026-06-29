#pragma once

#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0
#define FW_VERSION_PATCH  0
#define FW_VERSION_STR    "1.0.0"
#define FW_BUILD_DATE     __DATE__
#define FW_BUILD_TIME     __TIME__

// XIAO ESP32C6: route RF to u.fl (GPIO3/14). Connect antenna first — do not
// enable with an empty u.fl port. Comment out to use the built-in PCB antenna.
// #define USE_EXTERNAL_ANTENNA

// Debug output: connection, heartbeat, button presses.
// Comment out APP_DEBUG before release builds.
#define APP_DEBUG

#ifdef APP_DEBUG
  #define LOG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define LOG(fmt, ...) do {} while(0)
#endif

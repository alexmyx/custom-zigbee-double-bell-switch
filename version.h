#pragma once

#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0
#define FW_VERSION_PATCH  0
#define FW_VERSION_STR    "1.0.0"
#define FW_BUILD_DATE     __DATE__
#define FW_BUILD_TIME     __TIME__

// Отладочный вывод: подключение, heartbeat, нажатия кнопок.
// Закомментировать APP_DEBUG перед финальной сборкой.
#define APP_DEBUG

#ifdef APP_DEBUG
  #define LOG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define LOG(fmt, ...) do {} while(0)
#endif

# Zigbee Button

Прошивка для двухкнопочного Zigbee-устройства на **Seeed Studio XIAO ESP32C6**. Устройство работает как **Zigbee Router** (питание от USB), отправляет нажатия в Zigbee2MQTT и поддерживает OTA-обновление по Wi‑Fi.

## Железо

| Компонент | Подключение |
|-----------|-------------|
| Плата | Seeed XIAO ESP32C6 |
| Кнопка 1 | D0 (GPIO 0), active-low, внутренний pull-up |
| Кнопка 2 | D1 (GPIO 1), active-low, внутренний pull-up |
| LED | Встроенный (инвертированная логика) |

## Сборка и прошивка

Требуется [PlatformIO](https://platformio.org/):

```bash
pio run                 # сборка
pio run --target upload # прошивка
pio device monitor      # serial monitor, 115200 baud
```

Arduino IDE: открыть `zigbee_button.ino`, выбрать плату **XIAO ESP32C6**.

Отладочные логи: раскомментировать/включить `#define APP_DEBUG` в `version.h` (для release лучше выключить).

## Zigbee и Home Assistant

Устройство **совместимо со stock Zigbee2MQTT** как **Aqara WXKG07LM** (`lumi.remote.b286acn02`). Это software-профиль для работы без внешнего converter — не клон оригинального пульта Aqara.

| Endpoint | Кнопка | Примеры action в z2m |
|----------|--------|----------------------|
| 1 | Левая (button 1) | `single_left`, `double_left`, `triple_left`, `hold_left`, `release_left` |
| 2 | Правая (button 2) | `single_right`, `double_right`, `triple_right`, `hold_right`, `release_right` |

Кластер: `genMultistateInput`, атрибут `present_value`:

| Значение | Жест |
|----------|------|
| 1 | одиночное нажатие |
| 2 | двойное |
| 3 | тройное |
| 0 | удержание (long press) |
| 255 | отпускание после hold (release) |

Дополнительно в MQTT: `battery`, `voltage`, `device_temperature` (через Lumi TLV на `genBasic 0xFF01`).

## Жесты кнопок

| Жест | Поведение |
|------|-----------|
| Короткое нажатие | single / double / triple (тайминги настраиваются) |
| Удержание одной кнопки | hold при удержании ≥ long press, **release при отпускании** |
| Обе кнопки 5–10 s → отпустить | перезагрузка в режим OTA (Wi‑Fi) |
| Обе кнопки ≥ 10 s | factory reset (Zigbee + настройки) |

Тайминги по умолчанию: double 400 ms, triple 400 ms, long press 800 ms.

## OTA (обновление прошивки)

ESP32-C6 не держит Wi‑Fi AP одновременно с Zigbee — OTA идёт через **отдельную перезагрузку** без Zigbee.

1. Зажать **обе** кнопки на **5–10 секунд**, отпустить.
2. Подключиться к Wi‑Fi:
   - SSID: `ZbButtonAP`
   - Пароль: `ZbButton`
3. Открыть в браузере: **http://192.168.0.100**
4. Загрузить `.bin` или изменить тайминги кнопок.
5. После сохранения настроек — **Reboot in Zigbee mode**, либо автоматический reboot после успешной прошивки.

Сессия OTA: **5 минут**, затем устройство перезагружается обратно.

## LED

| Режим | Индикация |
|-------|-----------|
| SEARCHING | мигание каждые 500 ms (ищет сеть) |
| CONNECTED | короткий импульс раз в 5 s |
| ERROR | 3× быстрое мигание |

При удержании обеих кнопок LED быстро мигает (предупреждение перед OTA или reset).

## Структура проекта

```
zigbee_button.ino   — точка входа, main loop
button.*            — распознавание нажатий
button_monitor.*    — OTA, factory reset, web UI
led_indicator.*     — индикация состояния
settings.*          — NVS (тайминги кнопок)
zigbee_handler.*    — Zigbee stack, endpoints, reports
version.h           — версия прошивки, APP_DEBUG
```

Подробности для разработки — в `CLAUDE.md`.

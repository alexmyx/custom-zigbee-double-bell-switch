#include "button_monitor.h"
#include "led_indicator.h"
#include "settings.h"
#include "version.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "Zigbee.h"

ButtonMonitor buttonMonitor;

static WebServer server(80);

// ── HTML ─────────────────────────────────────────────────────────

// Статическая часть — в PROGMEM
static const char PAGE_HEAD[] PROGMEM =
"<!DOCTYPE html><html><head>"
"<meta charset=UTF-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Zigbee Button</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:Arial,sans-serif;background:#f0f0f0;padding:16px}"
".wrap{max-width:400px;margin:0 auto}"
"h2{color:#333;margin-bottom:16px;font-size:18px}"
".info{background:#fff;border-radius:8px;padding:12px;"
"margin-bottom:12px;box-shadow:0 2px 4px rgba(0,0,0,.1);"
"font-size:13px;color:#555}"
".info b{color:#333}"
".tabs{display:flex;background:#fff;border-radius:8px 8px 0 0;"
"overflow:hidden;box-shadow:0 2px 4px rgba(0,0,0,.1)}"
".tab{flex:1;padding:11px;text-align:center;cursor:pointer;"
"background:#f8f8f8;border-bottom:2px solid #ddd;"
"font-size:13px;color:#666}"
".tab.on{background:#fff;border-bottom:2px solid #4CAF50;"
"color:#333;font-weight:700}"
".panel{display:none;background:#fff;"
"border-radius:0 0 8px 8px;padding:16px;"
"box-shadow:0 2px 4px rgba(0,0,0,.1)}"
".panel.on{display:block}"
"label{display:block;font-size:12px;color:#666;margin:10px 0 3px}"
"input[type=text],input[type=password],"
"input[type=number],input[type=file]{"
"width:100%;padding:8px;border:1px solid #ddd;"
"border-radius:4px;font-size:13px}"
"input:focus{outline:none;border-color:#4CAF50}"
".btn{width:100%;padding:10px;background:#4CAF50;color:#fff;"
"border:none;border-radius:4px;cursor:pointer;"
"font-size:14px;margin-top:12px}"
".btn:hover{background:#45a049}"
".btn:disabled{background:#aaa;cursor:default}"
".btn.red{background:#e53935;margin-top:6px}"
".btn.red:hover{background:#c62828}"
".bar-bg{background:#eee;border-radius:4px;margin-top:10px}"
"#bar{width:0;height:16px;background:#4CAF50;"
"border-radius:4px;transition:width .3s}"
"#status{font-size:12px;color:#666;margin-top:6px}"
"#progress{display:none}"
".warn{background:#fff8e1;border-left:3px solid #ffc107;"
"padding:8px 10px;border-radius:4px;"
"font-size:12px;color:#555;margin-bottom:10px}"
".ok{display:none;background:#e8f5e9;"
"border-left:3px solid #4CAF50;padding:8px 10px;"
"border-radius:4px;font-size:12px;color:#2e7d32;margin-top:10px}"
".hint{font-size:11px;color:#999;margin-top:2px}"
"hr{border:none;border-top:1px solid #eee;margin:12px 0}"
".timer{font-size:11px;color:#999;margin-top:12px;text-align:center}"
"</style></head><body><div class=wrap>"
"<h2>&#9881;&#65039; Zigbee Button</h2>"
"<div class=info>";

static const char PAGE_TABS[] PROGMEM =
"</div>"
"<div class=tabs>"
"<div class='tab on' onclick=\"sw('ota')\">&#128260; OTA</div>"
"<div class=tab onclick=\"sw('cfg')\">&#128295; Settings</div>"
"</div>"
"<div class='panel on' id=tab-ota>"
"<div class=warn>&#9888;&#65039; Не закрывайте страницу при загрузке</div>"
"<label>Файл прошивки (.bin):</label>"
"<input type=file id=fw accept=.bin>"
"<label>Пароль OTA:</label>"
"<input type=password id=op placeholder='Введите пароль'>"
"<button class=btn onclick=upd()>Загрузить прошивку</button>"
"<div id=progress><div class=bar-bg><div id=bar></div></div>"
"<div id=status></div></div>"
"<div class=timer>&#8987; Активна ещё <span id=tmr>5:00</span></div>"
"</div>"
"<div class=panel id=tab-cfg>"
"<label>Имя устройства:</label>"
"<input type=text id=cn maxlength=31 value='";

static const char PAGE_CFG[] PROGMEM =
"'><hr>"
"<label>Новый пароль OTA (мин. 8 символов):</label>"
"<input type=password id=cp maxlength=31 placeholder='Без изменений'>"
"<hr><label>Двойное нажатие (мс):</label>"
"<input type=number id=cd min=200 max=1000 step=50 value='";

static const char PAGE_TRIPLE[] PROGMEM =
"'><div class=hint>Рекомендуется: 300–500</div>"
"<label>Тройное нажатие (мс):</label>"
"<input type=number id=ct min=200 max=1500 step=50 value='";

static const char PAGE_LONG[] PROGMEM =
"'><div class=hint>Рекомендуется: 300–600</div>"
"<label>Удержание (мс):</label>"
"<input type=number id=cl min=500 max=3000 step=100 value='";

static const char PAGE_END[] PROGMEM =
"'><div class=hint>Рекомендуется: 600–1200</div>"
"<button class=btn onclick=save()>Сохранить</button>"
"<div class=ok id=ok>&#10003; Сохранено</div>"
"<hr>"
"<button class='btn red' onclick=rst()>Сбросить к заводским</button>"
"</div></div>"
"<script>"
"function sw(t){"
"document.querySelectorAll('.tab').forEach((e,i)=>"
"e.classList.toggle('on',(i==0&&t=='ota')||(i==1&&t=='cfg')));"
"document.querySelectorAll('.panel').forEach(e=>"
"e.classList.remove('on'));"
"document.getElementById('tab-'+t).classList.add('on')}"
"let tl=300;"
"setInterval(()=>{"
"tl--;let m=Math.floor(tl/60),s=tl%60;"
"document.getElementById('tmr').textContent="
"m+':'+(s<10?'0':'')+s;"
"},1000);"
"function upd(){"
"const f=document.getElementById('fw').files[0];"
"const p=document.getElementById('op').value;"
"if(!f){alert('Выберите файл');return;}"
"const fd=new FormData();"
"fd.append('firmware',f);fd.append('password',p);"
"document.getElementById('progress').style.display='block';"
"document.querySelector('#tab-ota .btn').disabled=true;"
"const x=new XMLHttpRequest();"
"x.upload.onprogress=e=>{"
"if(e.lengthComputable){"
"const p=Math.round(e.loaded/e.total*100);"
"document.getElementById('bar').style.width=p+'%';"
"document.getElementById('status').textContent='Загрузка: '+p+'%'}}"
"x.onload=()=>{"
"document.getElementById('status').textContent="
"x.status==200?'&#10003; Готово! Перезагрузка...':'&#10007; '+x.responseText;"
"if(x.status==200)document.getElementById('bar').style.width='100%';"
"else document.querySelector('#tab-ota .btn').disabled=false;}"
"x.onerror=()=>{"
"document.getElementById('status').textContent='&#10007; Потеряно соединение';"
"document.querySelector('#tab-ota .btn').disabled=false;}"
"x.open('POST','/update');x.send(fd)}"
"function save(){"
"const d={"
"name:document.getElementById('cn').value.trim(),"
"pass:document.getElementById('cp').value,"
"dbl:parseInt(document.getElementById('cd').value),"
"tpl:parseInt(document.getElementById('ct').value),"
"lng:parseInt(document.getElementById('cl').value)};"
"if(!d.name){alert('Имя не может быть пустым');return;}"
"fetch('/settings',{method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(d)})"
".then(r=>r.text()).then(t=>{"
"if(t==='OK'){const o=document.getElementById('ok');"
"o.style.display='block';"
"setTimeout(()=>o.style.display='none',3000)}"
"else alert('Ошибка: '+t)})}"
"function rst(){"
"if(!confirm('Сбросить все настройки?'))return;"
"fetch('/reset-settings',{method:'POST'})"
".then(()=>setTimeout(()=>location.reload(),1500))}"
"</script></body></html>";

static String buildPage() {
  String page;
  page.reserve(5120);
  page += FPSTR(PAGE_HEAD);

  // Динамическая часть — info блок
  page += "Устройство: <b>";
  page += settings.deviceName;
  page += "</b><br>Версия: <b>";
  page += FW_VERSION_STR;
  page += "</b><br>Собрано: <b>";
  page += FW_BUILD_DATE;
  page += " ";
  page += FW_BUILD_TIME;
  page += "</b>";

  page += FPSTR(PAGE_TABS);

  // Значение поля имени устройства
  page += settings.deviceName;

  page += FPSTR(PAGE_CFG);

  // Значение doubleClickMs
  page += settings.doubleClickMs;

  page += FPSTR(PAGE_TRIPLE);

  // Значение tripleClickMs
  page += settings.tripleClickMs;

  page += FPSTR(PAGE_LONG);

  // Значение longPressMs
  page += settings.longPressMs;

  page += FPSTR(PAGE_END);
  return page;
}

// ── Обработчики сервера ──────────────────────────────────────────

static void handleRoot() {
  server.send(200, "text/html", buildPage());
}

static void handleSettings() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Нет данных");
    return;
  }

  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Ошибка JSON");
    return;
  }

  if (doc.containsKey("name")) {
    strlcpy(settings.deviceName,
      doc["name"].as<const char*>(),
      sizeof(settings.deviceName));
  }

  if (doc.containsKey("pass")) {
    const char* p = doc["pass"];
    if (strlen(p) >= 8) {
      strlcpy(settings.otaPassword, p,
        sizeof(settings.otaPassword));
    }
  }

  if (doc.containsKey("dbl")) {
    uint16_t v = doc["dbl"];
    if (v >= 200 && v <= 1000) settings.doubleClickMs = v;
  }

  if (doc.containsKey("tpl")) {
    uint16_t v = doc["tpl"];
    if (v >= 200 && v <= 1500) settings.tripleClickMs = v;
  }

  if (doc.containsKey("lng")) {
    uint16_t v = doc["lng"];
    if (v >= 500 && v <= 3000) settings.longPressMs = v;
  }

  settings.save();
  server.send(200, "text/plain", "OK");
}

static void handleResetSettings() {
  settings.reset();
  settings.save();
  server.send(200, "text/plain", "OK");
}

// Флаг: загрузка отклонена из-за неверного пароля
static bool g_uploadRejected = false;

static void handleUpdateDone() {
  if (g_uploadRejected) {
    server.send(403, "text/plain", "Неверный пароль");
    return;
  }
  bool ok = !Update.hasError();
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.errorString());
  if (ok) {
    delay(300);
    ESP.restart();
  }
}

static void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_uploadRejected = false;
    if (server.hasArg("password") &&
        server.arg("password") != settings.otaPassword) {
      LOG("[OTA] Неверный пароль\n");
      g_uploadRejected = true;
      return;
    }
    LOG("[OTA] Начало: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  }

  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_uploadRejected) return;
    esp_task_wdt_reset();
    if (Update.write(upload.buf, upload.currentSize)
        != upload.currentSize) {
      Update.printError(Serial);
    }
    LOG("[OTA] %d%%\n",
      (int)(Update.progress() * 100 / Update.size()));
  }

  else if (upload.status == UPLOAD_FILE_END) {
    if (g_uploadRejected) return;
    if (Update.end(true)) {
      LOG("[OTA] Успех: %u байт\n", upload.totalSize);
    } else {
      Update.printError(Serial);
      ledIndicator.setMode(LED_ERROR);
    }
  }
}

// ── Быстрое моргание ─────────────────────────────────────────────
void ButtonMonitor::_fastBlink(
    bool& state, uint32_t& lastBlink) {
  uint32_t now = millis();
  if ((now - lastBlink) >= BLINK_FAST_MS) {
    lastBlink = now;
    state     = !state;
    ledIndicator.setRaw(state);
  }
}

// ── OTA ──────────────────────────────────────────────────────────
void ButtonMonitor::_startOta() {
  LOG("[OTA] Запуск точки доступа...\n");

  IPAddress ip(192, 168, 0, 100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(settings.deviceName, settings.otaPassword);
  delay(300);

  LOG("[OTA] SSID: %s\n", settings.deviceName);
  LOG("[OTA] http://192.168.0.100\n");

  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/settings",      HTTP_POST, handleSettings);
  server.on("/reset-settings",HTTP_POST, handleResetSettings);
  server.on("/update",        HTTP_POST,
    handleUpdateDone, handleUpdateUpload);

  server.begin();

  _otaActive    = true;
  _otaStartTime = millis();
  LOG("[OTA] Веб-сервер запущен\n");
}

void ButtonMonitor::_stopOta() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  _otaActive = false;
  ledIndicator.forceRefresh();
  LOG("[OTA] Выключено\n");
}

void ButtonMonitor::_checkOta() {
  if (_otaActive) {
    server.handleClient();
    // Таймаут 5 минут
    if ((millis() - _otaStartTime) >= 300000UL) {
      LOG("[OTA] Таймаут\n");
      _stopOta();
    }
    return;
  }

  // Следим только за кнопкой 1
  // Если обе нажаты — это сброс, не OTA
  bool p1 = (digitalRead(_pin1) == LOW);
  bool p2 = (digitalRead(_pin2) == LOW);
  if (p2) {
    // Обе нажаты — сбрасываем состояние OTA
    if (_otaHeld) {
      _otaHeld    = false;
      _otaWarning = false;
      ledIndicator.forceRefresh();
    }
    return;
  }

  uint32_t now = millis();

  if (p1 && !_otaHeld) {
    _otaHeld      = true;
    _otaHoldStart = now;
    _otaWarning   = false;
    LOG("[OTA] Удержание...\n");
  }

  if (!p1 && _otaHeld) {
    if ((now - _otaHoldStart) < OTA_HOLD_MS) {
      _otaHeld    = false;
      _otaWarning = false;
      ledIndicator.forceRefresh();
    }
  }

  if (p1 && _otaHeld) {
    uint32_t held = now - _otaHoldStart;

    if (held >= OTA_WARNING_MS) {
      if (!_otaWarning) {
        _otaWarning = true;
        LOG("[OTA] Ещё 5 сек...\n");
      }
      _fastBlink(_otaBlinkState, _otaLastBlink);
    }

    if (held >= OTA_HOLD_MS) {
      _otaHeld = false;
      _startOta();
    }
  }
}

// ── Reset ─────────────────────────────────────────────────────────
void ButtonMonitor::_doReset() {
  LOG("[RESET] Сброс настроек Zigbee\n");

  // Отключаем watchdog — будут намеренные задержки
  esp_task_wdt_delete(NULL);

  ledIndicator.setRaw(true);
  delay(1000);
  ledIndicator.setRaw(false);
  delay(300);

  for (int i = 0; i < 3; i++) {
    ledIndicator.setRaw(true);  delay(100);
    ledIndicator.setRaw(false); delay(100);
  }

  delay(300);
  Zigbee.factoryReset();
  delay(300);
  ESP.restart();
}

void ButtonMonitor::_checkReset() {
  if (_otaActive) return;

  bool p1 = (digitalRead(_pin1) == LOW);
  bool p2 = (digitalRead(_pin2) == LOW);

  uint32_t now = millis();

  if (p1 && p2 && !_rstHeld) {
    _rstHeld      = true;
    _rstHoldStart = now;
    _rstWarning   = false;
    LOG("[RESET] Обе кнопки нажаты...\n");
  }

  if ((!p1 || !p2) && _rstHeld) {
    if ((now - _rstHoldStart) < RESET_HOLD_MS) {
      LOG("[RESET] Отменён\n");
      _rstHeld    = false;
      _rstWarning = false;
      ledIndicator.forceRefresh();
    }
  }

  if (p1 && p2 && _rstHeld) {
    uint32_t held = now - _rstHoldStart;

    if (held >= RESET_WARNING_MS) {
      if (!_rstWarning) {
        _rstWarning = true;
        LOG("[RESET] Предупреждение!\n");
      }
      _fastBlink(_rstBlinkState, _rstLastBlink);
    }

    if (held >= RESET_HOLD_MS) {
      _doReset();
    }
  }
}

// ── Публичные методы ─────────────────────────────────────────────
void ButtonMonitor::begin(uint8_t pin1, uint8_t pin2) {
  _pin1 = pin1;
  _pin2 = pin2;
}

void ButtonMonitor::update() {
  _checkReset();  // сброс — высший приоритет
  _checkOta();
}

bool ButtonMonitor::isOtaActive() {
  return _otaActive;
}

bool ButtonMonitor::isResetting() {
  return _rstHeld;
}

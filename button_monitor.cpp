#include "button_monitor.h"
#include "led_indicator.h"
#include "settings.h"
#include "version.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "Zigbee.h"

ButtonMonitor buttonMonitor;

static WebServer server(80);
static Preferences _otaPrefs;
static const char* OTA_BOOT_KEY = "ota_boot";

// ── HTML ─────────────────────────────────────────────────────────

// Static HTML in PROGMEM
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
".main{background:#fff;border-radius:8px;padding:16px;"
"box-shadow:0 2px 4px rgba(0,0,0,.1)}"
".main h3{font-size:14px;color:#333;margin:12px 0 4px}"
".main h3:first-child{margin-top:0}"
"label{display:block;font-size:12px;color:#666;margin:10px 0 3px}"
"input[type=number],input[type=file]{"
"width:100%;padding:8px;border:1px solid #ddd;"
"border-radius:4px;font-size:13px}"
"input:focus{outline:none;border-color:#4CAF50}"
".btn{width:100%;padding:10px;background:#4CAF50;color:#fff;"
"border:none;border-radius:4px;cursor:pointer;"
"font-size:14px;margin-top:12px}"
".btn:hover{background:#45a049}"
".btn:disabled{background:#aaa;cursor:default}"
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
"<h2>Zigbee Button</h2>"
"<div class=info>";

static const char PAGE_BODY[] PROGMEM =
"</div>"
"<div class=main>"
"<div class=warn>Do not close this page while uploading</div>"
"<h3>Firmware update</h3>"
"<label>Firmware file (.bin):</label>"
"<input type=file id=fw accept=.bin>"
"<button class=btn id=btn-upd onclick='upd()'>Upload firmware</button>"
"<div id=progress><div class=bar-bg><div id=bar></div></div>"
"<div id=status></div></div>"
"<hr>"
"<h3>Button timing</h3>"
"<label>Double click (ms):</label>"
"<input type=number id=cd min=200 max=1000 step=50 value='";

static const char PAGE_TRIPLE[] PROGMEM =
"'><div class=hint>Suggested: 300-500</div>"
"<label>Triple click (ms):</label>"
"<input type=number id=ct min=200 max=1500 step=50 value='";

static const char PAGE_LONG[] PROGMEM =
"'><div class=hint>Suggested: 300-600</div>"
"<label>Long press (ms):</label>"
"<input type=number id=cl min=500 max=3000 step=100 value='";

static const char PAGE_END[] PROGMEM =
"'><div class=hint>Suggested: 600-1200</div>"
"<button class=btn onclick='save()'>Save timing</button>"
"<div class=ok id=ok>Saved</div>"
"<button class=btn id=btn-zb style='display:none' onclick='rebootZb()'>"
"Reboot in Zigbee mode</button>"
"<div class=timer>Session expires in <span id=tmr>5:00</span></div>"
"</div>"
"<script>"
"var tl=300;"
"setInterval(function(){"
"tl--;if(tl<0){tl=0;}"
"var m=Math.floor(tl/60),s=tl%60,el=document.getElementById('tmr');"
"if(el){el.textContent=m+':'+(s<10?'0':'')+s;}"
"},1000);"
"function upd(){"
"var f=document.getElementById('fw').files[0];"
"if(!f){alert('Select a file');return;}"
"var fd=new FormData();"
"fd.append('firmware',f);"
"document.getElementById('progress').style.display='block';"
"document.getElementById('btn-upd').disabled=true;"
"var x=new XMLHttpRequest();"
"x.upload.onprogress=function(e){"
"if(e.lengthComputable){"
"var pct=Math.round(e.loaded/e.total*100);"
"document.getElementById('bar').style.width=pct+'%';"
"document.getElementById('status').textContent='Upload: '+pct+'%';}};"
"x.onload=function(){"
"if(x.status===200){"
"document.getElementById('status').textContent='Done! Rebooting...';"
"document.getElementById('bar').style.width='100%';"
"}else{"
"document.getElementById('status').textContent='Error: '+x.responseText;"
"document.getElementById('btn-upd').disabled=false;}};"
"x.onerror=function(){"
"document.getElementById('status').textContent='Connection lost';"
"document.getElementById('btn-upd').disabled=false;};"
"x.open('POST','/update');x.send(fd);"
"}"
"function save(){"
"var d={"
"dbl:parseInt(document.getElementById('cd').value,10),"
"tpl:parseInt(document.getElementById('ct').value,10),"
"lng:parseInt(document.getElementById('cl').value,10)};"
"fetch('/settings',{method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(d)})"
".then(function(r){return r.text();})"
".then(function(t){"
"if(t==='OK'){var o=document.getElementById('ok');"
"o.style.display='block';"
"document.getElementById('btn-zb').style.display='block';"
"}else{alert('Error: '+t);}});"
"}"
"function rebootZb(){"
"document.getElementById('btn-zb').disabled=true;"
"fetch('/reboot-zigbee',{method:'POST'});"
"}"
"</script></body></html>";

static String buildPage() {
    String page;
    page.reserve(4096);
    page += FPSTR(PAGE_HEAD);

    page += "Device: <b>";
    page += OTA_WIFI_SSID;
    page += "</b><br>Firmware: <b>";
    page += FW_VERSION_STR;
    page += "</b><br>Built: <b>";
    page += FW_BUILD_DATE;
    page += " ";
    page += FW_BUILD_TIME;
    page += "</b>";

    page += FPSTR(PAGE_BODY);
    page += settings.doubleClickMs;
    page += FPSTR(PAGE_TRIPLE);
    page += settings.tripleClickMs;
    page += FPSTR(PAGE_LONG);
    page += settings.longPressMs;
    page += FPSTR(PAGE_END);
    return page;
}

// ── HTTP handlers ────────────────────────────────────────────────

static void handleRoot() {
    server.send(200, "text/html; charset=utf-8", buildPage());
}

static void handleSettings() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No data");
        return;
    }

    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    if (doc.containsKey("dbl")) {
        settings.setDoubleClickMs(doc["dbl"]);
    }

    if (doc.containsKey("tpl")) {
        settings.setTripleClickMs(doc["tpl"]);
    }

    if (doc.containsKey("lng")) {
        settings.setLongPressMs(doc["lng"]);
    }

    settings.save();
    server.send(200, "text/plain", "OK");
}

static void handleRebootZigbee() {
    server.send(200, "text/plain", "OK");
    delay(100);
    buttonMonitor.rebootToZigbeeMode();
}

static void handleUpdateDone() {
    bool ok = !Update.hasError();
    server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.errorString());
    if (ok) {
        buttonMonitor.clearOtaBootFlag();
        delay(300);
        ESP.restart();
    }
}

static void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        LOG("[OTA] Upload start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }

    else if (upload.status == UPLOAD_FILE_WRITE) {
        esp_task_wdt_reset();
        if (Update.write(upload.buf, upload.currentSize)
                != upload.currentSize) {
            Update.printError(Serial);
        }
        LOG("[OTA] %d%%\n",
            (int)(Update.progress() * 100 / Update.size()));
    }

    else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            LOG("[OTA] Success: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
            ledIndicator.setMode(LED_ERROR);
        }
    }
}

// ── Fast blink ───────────────────────────────────────────────────
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
void ButtonMonitor::clearOtaBootFlag() {
    _otaPrefs.begin("zigbee_btn", false);
    _otaPrefs.putBool(OTA_BOOT_KEY, false);
}

void ButtonMonitor::rebootToZigbeeMode() {
    LOG("[OTA] Reboot to Zigbee mode\n");
    clearOtaBootFlag();
    delay(100);
    ESP.restart();
}

bool ButtonMonitor::bootIntoOtaIfRequested() {
    _otaPrefs.begin("zigbee_btn", false);
    if (!_otaPrefs.getBool(OTA_BOOT_KEY, false)) {
        return false;
    }
    LOG("[OTA] Firmware mode (Zigbee off)\n");
    // WDT before _startOtaAp: setup() has not called add() yet for OTA path
    esp_task_wdt_add(NULL);
    _startOtaAp();
    return true;
}

void ButtonMonitor::_requestOtaReboot() {
    LOG("[OTA] Rebooting to WiFi mode...\n");
    _otaPrefs.begin("zigbee_btn", false);
    _otaPrefs.putBool(OTA_BOOT_KEY, true);
    delay(100);
    ESP.restart();
}

void ButtonMonitor::_startOtaAp() {
    esp_task_wdt_reset();

    IPAddress ip(192, 168, 0, 100);
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));

    if (!WiFi.softAP(OTA_WIFI_SSID, OTA_WIFI_PASSWORD, 6, 0, 4)) {
        LOG("[OTA] softAP fail\n");
        clearOtaBootFlag();
        delay(100);
        ESP.restart();
        return;
    }
    delay(300);

    Serial.printf("[OTA] WiFi SSID=\"%s\" pass=\"%s\"\n",
                                OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
    LOG("[OTA] http://192.168.0.100\n");

    server.on("/",             HTTP_GET,  handleRoot);
    server.on("/settings",     HTTP_POST, handleSettings);
    server.on("/reboot-zigbee", HTTP_POST, handleRebootZigbee);
    server.on("/update",         HTTP_POST,
        handleUpdateDone, handleUpdateUpload);

    server.begin();

    _otaActive    = true;
    _otaStartTime = millis();
    LOG("[OTA] Web server started\n");
}

void ButtonMonitor::_stopOta() {
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    clearOtaBootFlag();
    _otaActive = false;
    LOG("[OTA] Exit, rebooting...\n");
    delay(100);
    ESP.restart();
}

void ButtonMonitor::_checkBothHold() {
    bool p1 = (digitalRead(_pin1) == LOW);
    bool p2 = (digitalRead(_pin2) == LOW);
    bool both = p1 && p2;
    uint32_t now = millis();

    if (_otaActive) {
        server.handleClient();
        if ((now - _otaStartTime) >= 300000UL) {
            LOG("[OTA] Session timeout\n");
            _stopOta();
        }
    }

    if (both && !_bothHeld) {
        _bothHeld      = true;
        _bothHoldStart = now;
        _otaWarnShown  = false;
        _rstWarnShown  = false;
        LOG("[OTA/RESET] Both buttons held\n");
    }

    if (!both && _bothHeld) {
        uint32_t held = now - _bothHoldStart;
        _bothHeld     = false;
        _otaWarnShown = false;
        _rstWarnShown = false;
        ledIndicator.forceRefresh();
        if (held >= OTA_HOLD_MS && held < RESET_HOLD_MS) {
            _requestOtaReboot();
        }
        return;
    }

    if (!both) {
        return;
    }

    uint32_t held = now - _bothHoldStart;

    if (held >= RESET_HOLD_MS) {
        _bothHeld = false;
        _doReset();
        return;
    }

    if (held >= RESET_WARNING_MS) {
        if (!_rstWarnShown) {
            _rstWarnShown = true;
            LOG("[RESET] 3 seconds left...\n");
        }
        _fastBlink(_bothBlinkState, _bothLastBlink);
        return;
    }

    if (held >= OTA_WARNING_MS) {
        if (!_otaWarnShown) {
            _otaWarnShown = true;
            LOG("[OTA] 2 seconds left...\n");
        }
        _fastBlink(_bothBlinkState, _bothLastBlink);
    }
}

// ── Reset ─────────────────────────────────────────────────────────
void ButtonMonitor::_doReset() {
    LOG("[RESET] Factory reset (Zigbee + settings)\n");

    // Disable watchdog for intentional blocking delays
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
    settings.reset();
    settings.save();
    Zigbee.factoryReset();
    delay(300);
    ESP.restart();
}

// ── Public API ───────────────────────────────────────────────────
void ButtonMonitor::begin(uint8_t pin1, uint8_t pin2) {
    _pin1 = pin1;
    _pin2 = pin2;
}

void ButtonMonitor::update() {
    _checkBothHold();
}

bool ButtonMonitor::isOtaActive() {
    return _otaActive;
}

bool ButtonMonitor::isResetting() {
    return _bothHeld;
}

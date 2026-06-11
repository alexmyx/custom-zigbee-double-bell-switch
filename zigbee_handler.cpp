#include "zigbee_handler.h"
#include "zigbee_constants.h"
#include "led_indicator.h"
#include "settings.h"
#include "version.h"

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee router required: set ZIGBEE_MODE_ZCZR and CONFIG_ZB_ZCZR=y in platformio.ini"
#endif

#include "Zigbee.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_secur.h"
#include "bdb/esp_zigbee_bdb_commissioning.h"
#include "nwk/esp_zigbee_nwk.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_cluster.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_multistate_input.h"
#include "zcl/esp_zigbee_zcl_device_temp_config.h"

#define POST_JOIN_REPORT_MS       12000
#define CONFIG_REPORT_RETRY_MS    25000
#define TEMP_REPORT_INTERVAL      (5UL * 60UL * 1000UL)

static int16_t _deviceTempC = 25;

static bool _connected = false;
static uint32_t _connectedAt = 0;
static uint32_t _lastTempReport = 0;
static uint32_t _lastConfigOnBtn = 0;
static bool _tempSentOnJoin = false;
static bool _configReportedRetry = false;

static int16_t _cachedLqi = -1;

static int16_t _readChipTempC() {
    float t = temperatureRead();
    if (isnan(t)) {
        return 25;
    }
    if (t < -40.0f) {
        t = -40.0f;
    } else if (t > 125.0f) {
        t = 125.0f;
    }
    return (int16_t)lroundf(t);
}

struct ConfigAttrDef {
    uint16_t attr_id;
    uint16_t Settings::*field;
    uint16_t (*clamp)(uint16_t);
};

static const ConfigAttrDef _configAttrs[] = {
    {ZB_ATTR_DOUBLE_CLICK_MS, &Settings::doubleClickMs, Settings::clampDoubleClick},
    {ZB_ATTR_TRIPLE_CLICK_MS, &Settings::tripleClickMs, Settings::clampTripleClick},
    {ZB_ATTR_LONG_PRESS_MS,   &Settings::longPressMs,   Settings::clampLongPress},
};

static void _reportBtnConfig();

class ZbButtonEP1 : public ZigbeeMultistate {
public:
    explicit ZbButtonEP1(uint8_t endpoint) : ZigbeeMultistate(endpoint) {}

    bool addButtonConfigAttrs() {
        esp_zb_attribute_list_t *basic = esp_zb_cluster_list_get_cluster(
            _cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        if (!basic) {
            return false;
        }

        const uint8_t access = ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE
                             | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;

        for (const ConfigAttrDef& def : _configAttrs) {
            uint16_t *value = &(settings.*(def.field));
            esp_err_t ret = esp_zb_cluster_add_manufacturer_attr(
                basic, ESP_ZB_ZCL_CLUSTER_ID_BASIC, def.attr_id,
                ZB_MANUF_CODE_ALEXMYX, ESP_ZB_ZCL_ATTR_TYPE_U16, access,
                value);
            if (ret != ESP_OK) {
                return false;
            }
        }
        return true;
    }

    bool addDeviceTempCluster() {
        _deviceTempC = _readChipTempC();
        esp_zb_device_temp_config_cluster_cfg_t cfg = {
            .current_temperature = _deviceTempC,
        };
        esp_zb_attribute_list_t *cluster =
            esp_zb_device_temp_config_cluster_create(&cfg);
        if (!cluster) {
            return false;
        }
        return esp_zb_cluster_list_add_device_temp_config_cluster(
                   _cluster_list, cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE)
               == ESP_OK;
    }

    void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override {
        if (message->info.cluster != ESP_ZB_ZCL_CLUSTER_ID_BASIC
            || message->attribute.data.type != ESP_ZB_ZCL_ATTR_TYPE_U16
            || !message->attribute.data.value) {
            return;
        }

        for (const ConfigAttrDef& def : _configAttrs) {
            if (message->attribute.id != def.attr_id) {
                continue;
            }
            uint16_t raw = *(const uint16_t *)message->attribute.data.value;
            settings.*(def.field) = def.clamp(raw);
            settings.save();
            LOG("[Zigbee] Timing via ZCL attr 0x%04x = %u\n",
                def.attr_id, settings.*(def.field));
            return;
        }
    }
};

static ZbButtonEP1 zbButton1(EP_BUTTON_1);
static ZigbeeMultistate zbButton2(EP_BUTTON_2);

static bool _reportToCoordinator(uint8_t endpoint, uint16_t cluster_id,
                                 uint16_t attr_id,
                                 uint16_t manuf_code = 0) {
    esp_zb_zcl_report_attr_cmd_t cmd = {};
    cmd.zcl_basic_cmd.src_endpoint = endpoint;
    cmd.zcl_basic_cmd.dst_endpoint = 1;
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd.clusterID = cluster_id;
    cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.dis_default_resp = 1;
    cmd.attributeID = attr_id;
    if (manuf_code != 0) {
        cmd.manuf_specific = 1;
        cmd.manuf_code = manuf_code;
    }

    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return false;
    }
    esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&cmd);
    esp_zb_lock_release();
    return ret == ESP_OK;
}

static void _syncBtnConfigAttrs() {
    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return;
    }
    for (const ConfigAttrDef& def : _configAttrs) {
        uint16_t val = settings.*(def.field);
        esp_zb_zcl_set_manufacturer_attribute_val(
            EP_BUTTON_1, ESP_ZB_ZCL_CLUSTER_ID_BASIC,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_MANUF_CODE_ALEXMYX,
            def.attr_id, &val, false);
    }
    esp_zb_lock_release();
}

static void _reportBtnConfig() {
    _syncBtnConfigAttrs();
    for (const ConfigAttrDef& def : _configAttrs) {
        bool ok = _reportToCoordinator(
            EP_BUTTON_1, ESP_ZB_ZCL_CLUSTER_ID_BASIC, def.attr_id,
            ZB_MANUF_CODE_ALEXMYX);
#ifdef APP_DEBUG
        LOG("[Zigbee] config report attr 0x%04x %s\n", def.attr_id,
            ok ? "ok" : "fail");
#endif
    }
}

static bool _reportPresentValue(uint8_t endpoint, ZigbeeMultistate& btn) {
    if (_reportToCoordinator(
            endpoint, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT,
            ESP_ZB_ZCL_ATTR_MULTI_INPUT_PRESENT_VALUE_ID)) {
        return true;
    }
    return btn.reportMultistateInput();
}

static void _updateDeviceTempAttr() {
    _deviceTempC = _readChipTempC();
    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return;
    }
    esp_zb_zcl_set_attribute_val(
        EP_BUTTON_1, ESP_ZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_CURRENT_TEMP_ID,
        &_deviceTempC, false);
    esp_zb_lock_release();
}

static void _reportDeviceTemp() {
    _updateDeviceTempAttr();
    _reportToCoordinator(
        EP_BUTTON_1, ESP_ZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG,
        ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_CURRENT_TEMP_ID);
}

static void _maybeReportDeviceTemp() {
    uint32_t now = millis();
    if (_lastTempReport != 0
        && ((now - _lastTempReport) < TEMP_REPORT_INTERVAL)) {
        return;
    }
    _lastTempReport = now;
    _reportDeviceTemp();
}

#ifdef APP_DEBUG
bool zigbeeDbgFactoryNew() {
    return esp_zb_bdb_is_factory_new();
}
#endif

int16_t zigbeeGetLinkQuality() {
    if (!Zigbee.connected()) return _cachedLqi;

    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return _cachedLqi;
    }

    esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t nbr = {};
    int16_t best = -1;
    int16_t coord = -1;
    int16_t parent = -1;

    while (esp_zb_nwk_get_next_neighbor(&it, &nbr) == ESP_OK) {
        int16_t lqi = (int16_t)(uint8_t)nbr.lqi;
        if (nbr.short_addr == 0x0000) {
            coord = lqi;
        }
        if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_PARENT) {
            parent = lqi;
        }
        if (lqi > best) {
            best = lqi;
        }
    }

    esp_zb_lock_release();

    int16_t result = parent;
    if (coord >= 0) {
        result = coord;  // router: coordinator is rarely marked PARENT
    } else if (result < 0) {
        result = best;
    }
    if (result >= 0) {
        _cachedLqi = result;
    }
    return _cachedLqi;
}

void zigbeeInit() {
    zbButton1.addMultistateInput();
    zbButton2.addMultistateInput();
    zbButton1.setMultistateInputStates(4);
    zbButton2.setMultistateInputStates(4);
    zbButton1.setPowerSource(ZB_POWER_SOURCE_MAINS);
    zbButton2.setPowerSource(ZB_POWER_SOURCE_MAINS);
    zbButton1.setManufacturerAndModel(ZB_VENDOR_NAME, ZB_MODEL_NAME);
    zbButton2.setManufacturerAndModel(ZB_VENDOR_NAME, ZB_MODEL_NAME);

    if (!zbButton1.addButtonConfigAttrs()) {
        LOG("[Zigbee] WARN: timing attrs init failed\n");
    }
    if (!zbButton1.addDeviceTempCluster()) {
        LOG("[Zigbee] WARN: device temp cluster init failed\n");
    }

    Zigbee.addEndpoint(&zbButton1);
    Zigbee.addEndpoint(&zbButton2);

    Zigbee.setRxOnWhenIdle(true);
    Zigbee.setScanDuration(4);
    esp_zb_secur_network_min_join_lqi_set(0);

    esp_zb_cfg_t zb_cfg = ZIGBEE_DEFAULT_ROUTER_CONFIG();
    Zigbee.setTimeout(2000);
    if (!Zigbee.begin(&zb_cfg)) {
        LOG("[Zigbee] begin failed, rebooting\n");
        delay(1000);
        ESP.restart();
    }
    LOG("[Zigbee] Init %s / %s (factory_new=%d)\n",
        ZB_VENDOR_NAME, ZB_MODEL_NAME, (int)esp_zb_bdb_is_factory_new());
}

void zigbeeLoop() {
    bool now_connected = Zigbee.connected();

    if (!now_connected && esp_zb_bdb_is_factory_new()) {
        static uint32_t _lastJoinHint = 0;
        uint32_t now = millis();
        if (_lastJoinHint == 0 || (now - _lastJoinHint) >= 30000) {
            _lastJoinHint = now;
            LOG("[Zigbee] Not joined — permit join ON in z2m, remove old entries, then erase flash\n");
        }
    } else if (!now_connected && !esp_zb_bdb_is_factory_new()) {
        static uint32_t _lastOrphanHint = 0;
        uint32_t now = millis();
        if (_lastOrphanHint == 0 || (now - _lastOrphanHint) >= 15000) {
            _lastOrphanHint = now;
            LOG("[Zigbee] Partial join — z2m may send Leave/reset; delete device in z2m, erase flash, re-pair\n");
        }
    }

    if (now_connected && !_connected) {
        _connected = true;
        _connectedAt = millis();
        _tempSentOnJoin = false;
        _configReportedRetry = false;
        LOG("[Zigbee] Connected\n");
        ledIndicator.setMode(LED_CONNECTED);
    } else if (!now_connected && _connected) {
        _connected = false;
        _tempSentOnJoin = false;
        _configReportedRetry = false;
        LOG("[Zigbee] Disconnected\n");
        ledIndicator.setMode(LED_SEARCHING);
    }

    if (now_connected && !_configReportedRetry
        && ((millis() - _connectedAt) >= CONFIG_REPORT_RETRY_MS)) {
        _configReportedRetry = true;
        _reportBtnConfig();
    }

    if (now_connected && !_tempSentOnJoin
        && ((millis() - _connectedAt) >= POST_JOIN_REPORT_MS)) {
        _tempSentOnJoin = true;
        _lastTempReport = millis();
        _syncBtnConfigAttrs();
        _reportDeviceTemp();
        _reportBtnConfig();
    }

    if (now_connected && _tempSentOnJoin) {
        _maybeReportDeviceTemp();
    }

    if (now_connected) {
        zigbeeGetLinkQuality();
    }
}

void zigbeeSendAction(uint8_t buttonIndex, ButtonEvent event) {
    if (!Zigbee.connected()) {
        return;
    }

    uint16_t value;
    switch (event) {
        case BTN_SINGLE: value = 1; break;
        case BTN_DOUBLE: value = 2; break;
        case BTN_TRIPLE: value = 3; break;
        case BTN_LONG:    value = 0;   break;
        case BTN_RELEASE: value = 255; break;
        default: return;
    }

    uint8_t ep = (buttonIndex == 1) ? EP_BUTTON_1 : EP_BUTTON_2;
#ifdef APP_DEBUG
    const char* name = "";
    switch (event) {
        case BTN_SINGLE:  name = "single";  break;
        case BTN_DOUBLE:  name = "double";  break;
        case BTN_TRIPLE:  name = "triple";  break;
        case BTN_LONG:    name = "hold";    break;
        case BTN_RELEASE: name = "release"; break;
        default: break;
    }
    LOG("[Zigbee] EP%d -> %s (%u), lqi=%d\n", ep, name, value,
        (int)zigbeeGetLinkQuality());
#endif

    ZigbeeMultistate& btn = (buttonIndex == 1)
        ? static_cast<ZigbeeMultistate&>(zbButton1)
        : static_cast<ZigbeeMultistate&>(zbButton2);
    if (!btn.setMultistateInput(value)) {
        return;
    }
    _reportPresentValue(ep, btn);

    uint32_t now = millis();
    if (_lastConfigOnBtn == 0
        || ((now - _lastConfigOnBtn) >= TEMP_REPORT_INTERVAL)) {
        _lastConfigOnBtn = now;
        _reportBtnConfig();
    }
}

bool zigbeeIsConnected() {
    return _connected;
}

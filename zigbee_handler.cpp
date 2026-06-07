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
#include "bdb/esp_zigbee_bdb_commissioning.h"
#include "esp_zigbee_secur.h"
#include "nwk/esp_zigbee_nwk.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_cluster.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_multistate_input.h"
#include "zcl/esp_zigbee_zcl_device_temp_config.h"

#define POST_JOIN_REPORT_MS    10000
#define TEMP_REPORT_INTERVAL   (5UL * 60UL * 1000UL)

static int16_t _deviceTempC = 25;

static bool _connected = false;
static uint32_t _connectedAt = 0;
static uint32_t _lastTempReport = 0;
static bool _tempSentOnJoin = false;

#ifdef APP_DEBUG
static int16_t _cachedLqi = -1;
static uint32_t _lastSteeringKick = 0;
#endif

static bool _isJoined();

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

class ZbButtonEP1 : public ZigbeeMultistate {
public:
    explicit ZbButtonEP1(uint8_t endpoint) : ZigbeeMultistate(endpoint) {}

    bool addButtonConfigCluster() {
        esp_zb_attribute_list_t *attr_list =
            esp_zb_zcl_attr_list_create(ZB_CLUSTER_BTN_CONFIG);
        if (!attr_list) {
            return false;
        }

        const uint8_t access = ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE
                             | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;

        for (const ConfigAttrDef& def : _configAttrs) {
            uint16_t *value = &(settings.*(def.field));
            esp_err_t ret = esp_zb_cluster_add_manufacturer_attr(
                attr_list, ZB_CLUSTER_BTN_CONFIG, def.attr_id,
                ZB_MANUF_CODE_ALEXMYX, ESP_ZB_ZCL_ATTR_TYPE_U16, access,
                value);
            if (ret != ESP_OK) {
                return false;
            }
        }

        return esp_zb_cluster_list_add_custom_cluster(
                   _cluster_list, attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE)
               == ESP_OK;
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
        if (message->info.cluster != ZB_CLUSTER_BTN_CONFIG
            || message->attribute.data.type != ESP_ZB_ZCL_ATTR_TYPE_U16
            || !message->attribute.data.value) {
            return;
        }

        for (const ConfigAttrDef& def : _configAttrs) {
            if (message->attribute.id != def.attr_id) {
                continue;
            }
            settings.*(def.field) = def.clamp(settings.*(def.field));
            settings.save();
            LOG("[Zigbee] Timing via ZCL: dbl=%u tpl=%u lng=%u\n",
                settings.doubleClickMs, settings.tripleClickMs,
                settings.longPressMs);
            return;
        }
    }
};

static ZbButtonEP1 zbButton1(EP_BUTTON_1);
static ZigbeeMultistate zbButton2(EP_BUTTON_2);

static bool _isJoined() {
    return esp_zb_bdb_dev_joined() && Zigbee.connected();
}

#ifdef APP_DEBUG
extern "C" void zigbeeSteeringKick(uint8_t param) {
    (void)param;
    if (esp_zb_bdb_is_factory_new() && !_isJoined()) {
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    }
}
#endif

static bool _reportToCoordinator(uint8_t endpoint, uint16_t cluster_id,
                                 uint16_t attr_id) {
    esp_zb_zcl_report_attr_cmd_t cmd = {};
    cmd.zcl_basic_cmd.src_endpoint = endpoint;
    cmd.zcl_basic_cmd.dst_endpoint = 1;
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd.clusterID = cluster_id;
    cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.dis_default_resp = 1;
    cmd.attributeID = attr_id;

    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return false;
    }
    esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&cmd);
    esp_zb_lock_release();
    return ret == ESP_OK;
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

int16_t zigbeeGetLinkQuality() {
#ifdef APP_DEBUG
    if (!_isJoined()) return _cachedLqi;

    esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t nbr = {};
    while (esp_zb_nwk_get_next_neighbor(&it, &nbr) == ESP_OK) {
        if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_PARENT) {
            _cachedLqi = (int16_t)(uint8_t)nbr.lqi;
            return _cachedLqi;
        }
    }
    return _cachedLqi;
#else
    return -1;
#endif
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

    if (!zbButton1.addButtonConfigCluster()) {
        LOG("[Zigbee] WARN: config cluster init failed\n");
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
    Zigbee.begin(&zb_cfg);
}

void zigbeeLoop() {
    bool now_connected = _isJoined();

#ifdef APP_DEBUG
    if (esp_zb_bdb_is_factory_new() && !now_connected && Zigbee.started()
        && (_lastSteeringKick == 0) && (millis() >= 60000)) {
        _lastSteeringKick = millis();
        esp_zb_scheduler_alarm((esp_zb_callback_t)zigbeeSteeringKick, 0, 0);
    }
#endif

    if (now_connected && !_connected) {
        _connected = true;
        _connectedAt = millis();
        _tempSentOnJoin = false;
        LOG("[Zigbee] Connected\n");
        ledIndicator.setMode(LED_CONNECTED);
    } else if (!now_connected && _connected) {
        _connected = false;
        _tempSentOnJoin = false;
        LOG("[Zigbee] Disconnected\n");
        ledIndicator.setMode(LED_SEARCHING);
    }

    if (now_connected && !_tempSentOnJoin
        && ((millis() - _connectedAt) >= POST_JOIN_REPORT_MS)) {
        _tempSentOnJoin = true;
        _lastTempReport = millis();
        _reportDeviceTemp();
    }

    if (now_connected && _tempSentOnJoin) {
#ifdef APP_DEBUG
        int16_t lqi = zigbeeGetLinkQuality();
        if (lqi >= 0) {
            _cachedLqi = lqi;
        }
#endif
        _maybeReportDeviceTemp();
    }
}

void zigbeeSendAction(uint8_t buttonIndex, ButtonEvent event) {
    if (!_isJoined()) {
        return;
    }

    uint16_t value;
    switch (event) {
        case BTN_SINGLE: value = 1; break;
        case BTN_DOUBLE: value = 2; break;
        case BTN_TRIPLE: value = 3; break;
        case BTN_LONG:   value = 0; break;
        default: return;
    }

    uint8_t ep = (buttonIndex == 1) ? EP_BUTTON_1 : EP_BUTTON_2;
#ifdef APP_DEBUG
    const char* name = "";
    switch (event) {
        case BTN_SINGLE: name = "single"; break;
        case BTN_DOUBLE: name = "double"; break;
        case BTN_TRIPLE: name = "triple"; break;
        case BTN_LONG:   name = "hold";   break;
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
}

bool zigbeeIsConnected() {
    return _connected;
}

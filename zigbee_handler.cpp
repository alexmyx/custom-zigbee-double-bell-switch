#include "zigbee_handler.h"
#include "led_indicator.h"
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
#include "zcl/esp_zigbee_zcl_command.h"
#include "zcl/esp_zigbee_zcl_multistate_input.h"

#define BATTERY_PERCENT        100
#define BATTERY_VOLTAGE        30    // 3.0 V (100 mV units), genPowerCfg EP1
#define LUMI_MANUF_CODE        0x115F  // LUMI United Technology, stock z2m lumi_basic
#define LUMI_BASIC_ATTR_FF01   0xFF01  // Aqara: TLV blob → lumi_basic, tag 1 = voltage mV, tag 3 = device temp °C
#define LUMI_FF01_TLV_LEN      7       // voltage (5) + device_temperature (3)
#define POST_JOIN_VOLTAGE_MS   10000
#define VOLTAGE_ON_BTN_MS      (60UL * 1000UL)

static int16_t _cachedLqi = -1;
static uint8_t lumi_ff01[1 + LUMI_FF01_TLV_LEN];

// Device IDs match WXKG07LM (0x5F01 / 0x5F02)
class AqaraButtonEP : public ZigbeeMultistate {
public:
    AqaraButtonEP(uint8_t endpoint, uint16_t device_id)
        : ZigbeeMultistate(endpoint) {
        _device_id = static_cast<esp_zb_ha_standard_devices_t>(device_id);
        _ep_config.app_device_id = device_id;
    }

    // Aqara lumi_basic: genBasic 0xFF01 = TLV [1,0x21,mV] + [3,0x28,temp°C] → voltage, battery, device_temperature
    bool addLumiBasicAttribute() {
        if (_endpoint != EP_BUTTON_1) {
            return true;
        }
        esp_zb_attribute_list_t *basic = esp_zb_cluster_list_get_cluster(
            _cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        if (!basic) {
            return false;
        }
        lumi_ff01[0] = LUMI_FF01_TLV_LEN;
        lumi_ff01[1] = 0x01;
        lumi_ff01[2] = 0x21;
        lumi_ff01[3] = 0xB8;
        lumi_ff01[4] = 0x0B;  // 3000 mV
        lumi_ff01[5] = 0x03;
        lumi_ff01[6] = 0x28;
        lumi_ff01[7] = 25;    // placeholder until first read
        uint8_t access = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
        esp_err_t ret = esp_zb_cluster_add_manufacturer_attr(
            basic, ESP_ZB_ZCL_CLUSTER_ID_BASIC, LUMI_BASIC_ATTR_FF01, LUMI_MANUF_CODE,
            ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, access, lumi_ff01);
        return ret == ESP_OK;
    }
};

static AqaraButtonEP zbButton1(EP_BUTTON_1, 0x5F01);
static AqaraButtonEP zbButton2(EP_BUTTON_2, 0x5F02);

static bool _connected = false;
static uint32_t _lastSteeringKick = 0;
static uint32_t _connectedAt = 0;
static uint32_t _lastVoltageReport = 0;
static bool _bindingStarted = false;
static bool _voltageSentOnJoin = false;

static bool _isJoined();

static int8_t _readDeviceTempC() {
    float t = temperatureRead();
    if (isnan(t)) {
        return 25;
    }
    if (t < -40.0f) {
        t = -40.0f;
    } else if (t > 125.0f) {
        t = 125.0f;
    }
    return (int8_t)lroundf(t);
}

static void _updateLumiFf01() {
    lumi_ff01[0] = LUMI_FF01_TLV_LEN;
    lumi_ff01[1] = 0x01;
    lumi_ff01[2] = 0x21;
    lumi_ff01[3] = 0xB8;
    lumi_ff01[4] = 0x0B;
    lumi_ff01[5] = 0x03;
    lumi_ff01[6] = 0x28;
    lumi_ff01[7] = (uint8_t)_readDeviceTempC();

    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return;
    }
    esp_zb_zcl_set_manufacturer_attribute_val(
        EP_BUTTON_1, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        LUMI_MANUF_CODE, LUMI_BASIC_ATTR_FF01, lumi_ff01, false);
    esp_zb_lock_release();
}

extern "C" void zigbeeSteeringKick(uint8_t param) {
    (void)param;
    if (esp_zb_bdb_is_factory_new() && !_isJoined()) {
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    }
}

static bool _isJoined() {
    return esp_zb_bdb_dev_joined() && Zigbee.connected();
}

static void _startFindingBinding() {
    if (_bindingStarted) return;
    _bindingStarted = true;
    esp_zb_bdb_finding_binding_start_target(EP_BUTTON_1, 240);
    esp_zb_bdb_finding_binding_start_target(EP_BUTTON_2, 240);
}

// Button reports: direct report to coordinator 0x0000 (genMultistateInput).
static bool _reportToCoordinator(uint8_t endpoint, uint16_t cluster_id, uint16_t attr_id,
                                 uint16_t manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC) {
    const bool manuf = (manuf_code != ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC);

    esp_zb_zcl_report_attr_cmd_t cmd = {};
    cmd.zcl_basic_cmd.src_endpoint = endpoint;
    cmd.zcl_basic_cmd.dst_endpoint = 1;
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd.clusterID = cluster_id;
    cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.manuf_specific = manuf ? 1 : 0;
    cmd.dis_default_resp = 1;
    cmd.manuf_code = manuf ? manuf_code : ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
    cmd.attributeID = attr_id;

    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return false;
    }
    esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&cmd);
    esp_zb_lock_release();
    return ret == ESP_OK;
}

static bool _reportPresentValue(uint8_t endpoint, ZigbeeMultistate& btn) {
    if (_reportToCoordinator(endpoint, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT,
                             ESP_ZB_ZCL_ATTR_MULTI_INPUT_PRESENT_VALUE_ID)) {
        return true;
    }
    return btn.reportMultistateInput();
}

// Stock z2m lumi_basic: tag 1 → voltage/battery, tag 3 → device_temperature
static void _reportLumiBasic() {
    _updateLumiFf01();
    _reportToCoordinator(EP_BUTTON_1, ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                         LUMI_BASIC_ATTR_FF01, LUMI_MANUF_CODE);
}

static void _maybeReportLumiBasic() {
    uint32_t now = millis();
    if (_lastVoltageReport != 0 && ((now - _lastVoltageReport) < VOLTAGE_ON_BTN_MS)) {
        return;
    }
    _lastVoltageReport = now;
    _reportLumiBasic();
}

int16_t zigbeeGetLinkQuality() {
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
}

void zigbeeInit() {
    zbButton1.addMultistateInput();
    zbButton2.addMultistateInput();
    zbButton1.setMultistateInputStates(4);  // single/double/triple/hold (Aqara WXKG07LM)
    zbButton2.setMultistateInputStates(4);
    zbButton1.addLumiBasicAttribute();

    zbButton1.setPowerSource(ZB_POWER_SOURCE_BATTERY, BATTERY_PERCENT, BATTERY_VOLTAGE);
    // genPowerCfg EP1 + lumi 0xFF01 - same as Aqara WXKG07LM

    zbButton1.setManufacturerAndModel("LUMI", "lumi.remote.b286acn02");
    zbButton2.setManufacturerAndModel("LUMI", "lumi.remote.b286acn02");

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

    if (esp_zb_bdb_is_factory_new() && !now_connected && Zigbee.started()
        && (_lastSteeringKick == 0) && (millis() >= 60000)) {
        _lastSteeringKick = millis();
        esp_zb_scheduler_alarm((esp_zb_callback_t)zigbeeSteeringKick, 0, 0);
    }

    if (now_connected && !_connected) {
        _connected = true;
        _connectedAt = millis();
        _voltageSentOnJoin = false;
        _startFindingBinding();
        LOG("[Zigbee] Connected\n");
        ledIndicator.setMode(LED_CONNECTED);
    } else if (!now_connected && _connected) {
        _connected = false;
        _bindingStarted = false;
        _voltageSentOnJoin = false;
        LOG("[Zigbee] Disconnected\n");
        ledIndicator.setMode(LED_SEARCHING);
    }

    if (now_connected && !_voltageSentOnJoin && ((millis() - _connectedAt) >= POST_JOIN_VOLTAGE_MS)) {
        _voltageSentOnJoin = true;
        _lastVoltageReport = millis();
        _reportLumiBasic();
    }

    if (now_connected) {
        int16_t lqi = zigbeeGetLinkQuality();
        if (lqi >= 0) {
            _cachedLqi = lqi;
        }
    }
}

void zigbeeSendAction(uint8_t buttonIndex, ButtonEvent event) {
    if (!_isJoined()) {
        return;
    }

    uint16_t value;
    const char* name = "";
    switch (event) {
        case BTN_SINGLE: value = 1; name = "single"; break;
        case BTN_DOUBLE: value = 2; name = "double"; break;
        case BTN_TRIPLE: value = 3; name = "triple"; break;
        case BTN_LONG:    value = 0;   name = "hold";    break;
        case BTN_RELEASE: value = 255; name = "release"; break;
        default: return;
    }

    uint8_t ep = (buttonIndex == 1) ? EP_BUTTON_1 : EP_BUTTON_2;
    LOG("[Zigbee] EP%d -> %s (%u), lqi=%d\n", ep, name, value, (int)zigbeeGetLinkQuality());

    ZigbeeMultistate& btn = (buttonIndex == 1)
        ? static_cast<ZigbeeMultistate&>(zbButton1)
        : static_cast<ZigbeeMultistate&>(zbButton2);
    if (!btn.setMultistateInput(value)) {
        return;
    }
    if (!_reportPresentValue(ep, btn)) {
        return;
    }
    _maybeReportLumiBasic();
}

bool zigbeeIsConnected() {
    return _connected;
}

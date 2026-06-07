const modernExtend = require('zigbee-herdsman-converters/lib/modernExtend');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const {Zcl} = require('zigbee-herdsman/dist/zspec/zcl/zcl-types');

const ALEXMYX_MANUF_CODE = 0x1378;
const CLUSTER_BTN_CONFIG = 0xfc01;

const fzLocal = {
    multistate_action: {
        cluster: 'genMultistateInput',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const lookup = {0: 'hold', 1: 'single', 2: 'double', 3: 'triple'};
            const action = lookup[msg.data.presentValue];
            if (!action) return;

            const epMap = {1: 'button_1', 2: 'button_2'};
            const button = epMap[msg.endpoint.ID];
            if (!button) return;

            return {action: `${action}_${button}`};
        },
    },
};

const definition = {
    zigbeeModel: ['Zigbee double button'],
    model: 'Zigbee double button',
    vendor: 'alexmyx',
    description: 'Zigbee double button (Seeed XIAO ESP32C6)',
    meta: {multiEndpoint: true},
    endpoint: (device) => ({
        button_1: 1,
        button_2: 2,
    }),
    fromZigbee: [fzLocal.multistate_action],
    exposes: [
        e.action([
            'single_button_1', 'double_button_1', 'triple_button_1', 'hold_button_1',
            'single_button_2', 'double_button_2', 'triple_button_2', 'hold_button_2',
        ]),
    ],
    extend: [
        modernExtend.deviceAddCustomCluster('manuSpecificAlexmyxBtnConfig', {
            ID: CLUSTER_BTN_CONFIG,
            manufacturerCode: ALEXMYX_MANUF_CODE,
            attributes: {
                double_click_ms: {ID: 0x0000, type: Zcl.DataType.UINT16},
                triple_click_ms: {ID: 0x0001, type: Zcl.DataType.UINT16},
                long_press_ms: {ID: 0x0002, type: Zcl.DataType.UINT16},
            },
            commands: {},
            commandsResponse: {},
        }),
        modernExtend.numeric({
            name: 'double_click_ms',
            cluster: 'manuSpecificAlexmyxBtnConfig',
            attribute: 'double_click_ms',
            entityCategory: 'config',
            access: 'ALL',
            valueMin: 200,
            valueMax: 1000,
            unit: 'ms',
            description: 'Double click timeout',
            endpoint: 'button_1',
            zigbeeCommandOptions: {manufacturerCode: ALEXMYX_MANUF_CODE},
        }),
        modernExtend.numeric({
            name: 'triple_click_ms',
            cluster: 'manuSpecificAlexmyxBtnConfig',
            attribute: 'triple_click_ms',
            entityCategory: 'config',
            access: 'ALL',
            valueMin: 200,
            valueMax: 1500,
            unit: 'ms',
            description: 'Triple click timeout',
            endpoint: 'button_1',
            zigbeeCommandOptions: {manufacturerCode: ALEXMYX_MANUF_CODE},
        }),
        modernExtend.numeric({
            name: 'long_press_ms',
            cluster: 'manuSpecificAlexmyxBtnConfig',
            attribute: 'long_press_ms',
            entityCategory: 'config',
            access: 'ALL',
            valueMin: 500,
            valueMax: 3000,
            unit: 'ms',
            description: 'Long press timeout',
            endpoint: 'button_1',
            zigbeeCommandOptions: {manufacturerCode: ALEXMYX_MANUF_CODE},
        }),
        modernExtend.deviceTemperature({endpoint: 'button_1'}),
    ],
};

module.exports = definition;

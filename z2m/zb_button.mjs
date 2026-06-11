/**
 * External converter for alexmyx Zigbee double button.
 * @see https://www.zigbee2mqtt.io/advanced/more/external-converters.html
 */
import {Zcl} from 'zigbee-herdsman';
import {deviceTemperature, deviceAddCustomCluster} from 'zigbee-herdsman-converters/lib/modernExtend';
import {presets, access} from 'zigbee-herdsman-converters/lib/exposes';

const e = presets;
const ea = access;

const ALEXMYX_MANUF_CODE = 0x1378;
const ZCL_OPTIONS = {manufacturerCode: ALEXMYX_MANUF_CODE};

const TIMING_ATTRS = {
    double_click_ms: {id: 0x0000, prop: 'double_click_ms_button_1', min: 200, max: 1000},
    triple_click_ms: {id: 0x0001, prop: 'triple_click_ms_button_1', min: 200, max: 1500},
    long_press_ms:   {id: 0x0002, prop: 'long_press_ms_button_1',   min: 500, max: 3000},
};

const TIMING_ATTR_IDS = Object.values(TIMING_ATTRS).map((c) => c.id);

function pickU16(data, name, id) {
    if (data[name] !== undefined) return data[name];
    if (data[id] !== undefined) return data[id];
    const sid = String(id);
    if (data[sid] !== undefined) return data[sid];
    return undefined;
}

function timingExpose(name, min, max, description) {
    return e.numeric(name, ea.ALL)
        .withEndpoint('button_1')
        .withUnit('ms')
        .withValueMin(min)
        .withValueMax(max)
        .withDescription(description)
        .withCategory('config');
}

const btnConfigBasic = deviceAddCustomCluster('genBasic', {
    name: 'genBasic',
    ID: 0x0000,
    manufacturerCode: ALEXMYX_MANUF_CODE,
    attributes: {
        double_click_ms: {
            name: 'double_click_ms',
            ID: 0x0000,
            type: Zcl.DataType.UINT16,
            manufacturerCode: ALEXMYX_MANUF_CODE,
            write: true,
        },
        triple_click_ms: {
            name: 'triple_click_ms',
            ID: 0x0001,
            type: Zcl.DataType.UINT16,
            manufacturerCode: ALEXMYX_MANUF_CODE,
            write: true,
        },
        long_press_ms: {
            name: 'long_press_ms',
            ID: 0x0002,
            type: Zcl.DataType.UINT16,
            manufacturerCode: ALEXMYX_MANUF_CODE,
            write: true,
        },
    },
    commands: {},
    commandsResponse: {},
});

const fzMultistateAction = {
    cluster: 'genMultistateInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const lookup = {0: 'hold', 1: 'single', 2: 'double', 3: 'triple', 255: 'release'};
        const action = lookup[msg.data.presentValue];
        if (!action) return;

        const epMap = {1: 'button_1', 2: 'button_2'};
        const button = epMap[msg.endpoint.ID];
        if (!button) return;

        return {action: `${action}_${button}`};
    },
};

const fzBtnConfig = {
    cluster: 'genBasic',
    type: ['attributeReport', 'readResponse'],
    options: [ZCL_OPTIONS],
    convert: (model, msg, publish, options, meta) => {
        if (msg.endpoint.ID !== 1) return;

        const result = {};
        for (const [name, cfg] of Object.entries(TIMING_ATTRS)) {
            const value = pickU16(msg.data, name, cfg.id);
            if (value !== undefined) {
                result[cfg.prop] = value;
            }
        }
        return Object.keys(result).length ? result : undefined;
    },
};

function resolveTimingKey(key) {
    if (TIMING_ATTRS[key]) {
        return [key, TIMING_ATTRS[key]];
    }
    const entry = Object.entries(TIMING_ATTRS).find(([, c]) => c.prop === key);
    return entry ?? [];
}

const tzBtnConfig = {
    key: [
        ...Object.keys(TIMING_ATTRS),
        ...Object.values(TIMING_ATTRS).map((c) => c.prop),
    ],
    convertSet: async (entity, key, value, meta) => {
        const [name, cfg] = resolveTimingKey(key);
        if (!name) return;
        const num = Number(value);
        if (!Number.isFinite(num) || num < cfg.min || num > cfg.max) {
            throw new Error(`${cfg.prop} out of range (${cfg.min}-${cfg.max})`);
        }
        const ep = meta.device.getEndpoint(1);
        await ep.write('genBasic', {[name]: num}, ZCL_OPTIONS);
        return {[cfg.prop]: num};
    },
    convertGet: async (entity, key, meta) => {
        const [name] = resolveTimingKey(key);
        if (!name) return;
        const ep = meta.device.getEndpoint(1);
        await ep.read('genBasic', [name], ZCL_OPTIONS);
    },
};

async function readBtnConfig(ep) {
    await ep.read('genBasic', TIMING_ATTR_IDS, ZCL_OPTIONS);
}

async function configure(device, coordinatorEndpoint, definition) {
    const ep = device.getEndpoint(definition.endpoint(device).button_1);
    if (!ep) return;

    try {
        await readBtnConfig(ep);
    } catch (e) {
        // firmware reports timing after interview
    }
}

async function onEvent(type, data, device) {
    if (type !== 'deviceInterview' || data.status !== 'successful') return;

    const ep = device.getEndpoint(1);
    if (!ep) return;

    for (const delayMs of [3000, 12000]) {
        await new Promise((resolve) => setTimeout(resolve, delayMs));
        try {
            await readBtnConfig(ep);
        } catch (e) {
            // non-fatal
        }
    }
}

export default {
    fingerprint: [{modelID: 'Zigbee double button', manufacturerName: 'alexmyx'}],
    zigbeeModel: ['Zigbee double button'],
    model: 'Zigbee double button',
    vendor: 'alexmyx',
    description: 'Zigbee double button (Seeed XIAO ESP32C6)',
    meta: {multiEndpoint: true},
    endpoint: (device) => ({
        button_1: 1,
        button_2: 2,
    }),
    fromZigbee: [fzMultistateAction, fzBtnConfig],
    toZigbee: [tzBtnConfig],
    exposes: [
        e.action([
            'single_button_1', 'double_button_1', 'triple_button_1', 'hold_button_1', 'release_button_1',
            'single_button_2', 'double_button_2', 'triple_button_2', 'hold_button_2', 'release_button_2',
        ]),
        timingExpose('double_click_ms', 200, 1000, 'Double click timeout'),
        timingExpose('triple_click_ms', 200, 1500, 'Triple click timeout'),
        timingExpose('long_press_ms', 500, 3000, 'Long press timeout'),
    ],
    extend: [
        btnConfigBasic,
        deviceTemperature({endpointNames: ['button_1']}),
    ],
    configure,
    onEvent,
};

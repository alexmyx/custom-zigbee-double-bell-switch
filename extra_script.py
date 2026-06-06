# Патчи Arduino-Zigbee под rejoin после power cycle.
Import("env")
import os

FRAMEWORK_REL = os.path.join("libraries", "Zigbee", "src", "ZigbeeCore.cpp")
MULTISTATE_REL = os.path.join("libraries", "Zigbee", "src", "ep", "ZigbeeMultistate.cpp")

PATCHES = [
    {
        "tag": "zigbee_button: skip auto factoryReset on LEAVE",
        "old": (
            '          log_i("Leave without rejoin, factory reset the device");\n'
            "          Zigbee.factoryReset(true);"
        ),
        "new": (
            '          log_i("Leave without rejoin, steering continues");\n'
            "          // Zigbee.factoryReset(true);  // zigbee_button: skip auto factoryReset on LEAVE"
        ),
    },
    {
        "tag": "zigbee_button: no single-channel NVRAM on reboot",
        "old": (
            "            // Save the channel mask to NVRAM in case of reboot which may be on a different channel after a change in the network\n"
            "            Zigbee.setNVRAMChannelMask(1 << esp_zb_get_current_channel());"
        ),
        "new": (
            "            // zigbee_button: no single-channel NVRAM on reboot\n"
            "            // Zigbee.setNVRAMChannelMask(1 << esp_zb_get_current_channel());"
        ),
    },
    {
        "tag": "zigbee_button: no single-channel NVRAM on join",
        "old": (
            "          // Set channel mask and write to NVRAM, so that the device will re-join the network faster after reboot (scan only on the current channel)\n"
            "          Zigbee.setNVRAMChannelMask(1 << esp_zb_get_current_channel());"
        ),
        "new": (
            "          // zigbee_button: no single-channel NVRAM on join\n"
            "          // Zigbee.setNVRAMChannelMask(1 << esp_zb_get_current_channel());"
        ),
    },
    {
        "tag": "zigbee_button: mark started on begin timeout",
        "old": (
            "    if (_role != ZIGBEE_COORDINATOR) {  // Only End Device and Router can rejoin\n"
            "      resetNVRAMChannelMask();\n"
            "    }\n"
            "  }\n"
            "  return started();"
        ),
        "new": (
            "    if (_role != ZIGBEE_COORDINATOR) {  // Only End Device and Router can rejoin\n"
            "      resetNVRAMChannelMask();\n"
            "    }\n"
            "    _started = true;  // zigbee_button: mark started on begin timeout\n"
            "  }\n"
            "  return started();"
        ),
        "replace_all": True,
    },
    {
        "tag": "zigbee_button: skip resetNVRAM on begin timeout",
        "old": (
            "    if (_role != ZIGBEE_COORDINATOR) {  // Only End Device and Router can rejoin\n"
            "      resetNVRAMChannelMask();\n"
            "    }\n"
            "    _started = true;  // zigbee_button: mark started on begin timeout\n"
            "  }\n"
            "  return started();"
        ),
        "new": (
            "    // zigbee_button: skip resetNVRAMChannelMask on timeout — ломает rejoin\n"
            "    _started = true;  // zigbee_button: mark started on begin timeout\n"
            "  }\n"
            "  return started();"
        ),
        "replace_all": True,
    },
    {
        "tag": "zigbee_button: mains ED node descriptor",
        "old": (
            "  //NOTE: This is a workaround to make battery powered devices to be discovered as battery powered\n"
            "  if (((zigbee_role_t)Zigbee.getRole() == ZIGBEE_END_DEVICE) && edBatteryPowered) {\n"
            "    zb_set_ed_node_descriptor(0, Zigbee.getRxOnWhenIdle(), 1);\n"
            "  }"
        ),
        "new": (
            "  if ((zigbee_role_t)Zigbee.getRole() == ZIGBEE_END_DEVICE) {\n"
            "    // zigbee_button: mains ED node descriptor\n"
            "    bool battery = edBatteryPowered;\n"
            "    zb_set_ed_node_descriptor(!battery, Zigbee.getRxOnWhenIdle(), 1);\n"
            "  }"
        ),
    },
    {
        "tag": "zigbee_button: factory-new steering retry",
        "old": (
            "        /* commissioning failed */\n"
            '        log_w("Commissioning failed (status: %s), retry init", esp_err_to_name(err_status));\n'
            "        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 2000);"
        ),
        "new": (
            "        /* commissioning failed */\n"
            "        if (esp_zb_bdb_is_factory_new()) {\n"
            '          log_w("Commissioning failed (status: %s), retry steering", esp_err_to_name(err_status));\n'
            "          esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);\n"
            "        } else {\n"
            '          log_w("Commissioning failed (status: %s), retry init", esp_err_to_name(err_status));\n'
            "          esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 2000);\n"
            "        }"
        ),
    },
]


def apply_patches(path, patches):
    if not os.path.isfile(path):
        print(f"[extra_script] not found: {path}")
        return False

    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    changed = False
    for patch in patches:
        if patch["tag"] in content:
            continue
        count = 2 if patch.get("replace_all") else 1
        if patch["old"] not in content:
            print(f"[extra_script] pattern missing for: {patch['tag']} in {path}")
            continue
        content = content.replace(patch["old"], patch["new"], count)
        changed = True
        print(f"[extra_script] Applied: {patch['tag']}")

    if changed:
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
    return changed


def patch_zigbee_core(source, target, env):
    framework = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    apply_patches(os.path.join(framework, FRAMEWORK_REL), PATCHES)
    apply_patches(os.path.join(framework, MULTISTATE_REL), [
        {
            "tag": "zigbee_button: multistate 4 states",
            "old": ".number_of_states = 3, .out_of_service = false, .present_value = 0",
            "new": ".number_of_states = 4, .out_of_service = false, .present_value = 0  // zigbee_button: multistate 4 states",
        },
    ])


env.AddPreAction("buildprog", patch_zigbee_core)

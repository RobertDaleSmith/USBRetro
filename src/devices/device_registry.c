// device_registry.c
#include "device_registry.h"
#include "sony_ds3.h"
#include "sony_ds4.h"
#include "sony_ds5.h"
#include "sony_psc.h"
#include "8bitdo_bta.h"
#include "8bitdo_m30.h"
#include "8bitdo_pce.h"
#include "gamecube_adapter.h"
#include "hori_horipad.h"
#include "hori_pokken.h"
#include "logitech_wingman.h"
#include "sega_astrocity.h"
#include "switch_pro.h"
#include "hid_gamepad.h"
#include "hid_keyboard.h"
#include "hid_mouse.h"
// Include other devices here

DeviceInterface* device_interfaces[CONTROLLER_TYPE_COUNT] = {0};

void register_devices() {
    device_interfaces[CONTROLLER_DUALSHOCK3] = &sony_ds3_interface;
    device_interfaces[CONTROLLER_DUALSHOCK4] = &sony_ds4_interface;
    device_interfaces[CONTROLLER_DUALSENSE] = &sony_ds5_interface;
    device_interfaces[CONTROLLER_PSCLASSIC] = &sony_psc_interface;
    device_interfaces[CONTROLLER_8BITDO_BTA] = &bitdo_bta_interface;
    device_interfaces[CONTROLLER_8BITDO_M30] = &bitdo_m30_interface;
    device_interfaces[CONTROLLER_8BITDO_PCE] = &bitdo_pce_interface;
    device_interfaces[CONTROLLER_HORIPAD] = &hori_horipad_interface;
    device_interfaces[CONTROLLER_POKKEN] = &hori_pokken_interface;
    device_interfaces[CONTROLLER_WINGMAN] = &logitech_wingman_interface;
    device_interfaces[CONTROLLER_ASTROCITY] = &sega_astrocity_interface;
    device_interfaces[CONTROLLER_GAMECUBE] = &gamecube_adapter_interface;
    device_interfaces[CONTROLLER_SWITCH] = &switch_pro_interface;
    device_interfaces[CONTROLLER_DINPUT] = &hid_gamepad_interface;
    device_interfaces[CONTROLLER_KEYBOARD] = &hid_keyboard_interface;
    device_interfaces[CONTROLLER_MOUSE] = &hid_mouse_interface;
    // Register other devices here
    // device_interfaces[1] = &another_device_interface;

    // disabled devices
    // device_interfaces[CONTROLLER_DRAGONRISE] = &dragonrise_interface; // deprecated
    // device_interfaces[CONTROLLER_8BITDO_NEO] = &bitdo_neo_interface; // incomplete
}

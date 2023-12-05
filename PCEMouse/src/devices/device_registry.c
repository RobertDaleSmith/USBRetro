// device_registry.c
#include "device_registry.h"
#include "sony_ds3.h"
#include "sony_ds4.h"
#include "sony_ds5.h"
#include "sony_psc.h"
#include "8bitdo_bta.h"
#include "8bitdo_neo.h"
#include "8bitdo_m30.h"
#include "8bitdo_pce.h"
#include "hori_horipad.h"
// Include other devices here

DeviceInterface* device_interfaces[MAX_DEVICE_TYPES] = {0};

void register_devices() {
    device_interfaces[0] = &sony_ds3_interface;
    device_interfaces[1] = &sony_ds4_interface;
    device_interfaces[2] = &sony_ds5_interface;
    device_interfaces[3] = &sony_psc_interface;
    device_interfaces[4] = &bitdo_bta_interface;
    device_interfaces[5] = &bitdo_neo_interface;
    device_interfaces[6] = &bitdo_m30_interface;
    device_interfaces[7] = &bitdo_pce_interface;
    device_interfaces[8] = &hori_horipad_interface;
    // Register other devices here
    // device_interfaces[1] = &another_device_interface;
}

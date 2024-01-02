// device_registry.h
#include "device_interface.h"

// Controller Types
typedef enum {
    CONTROLLER_UNKNOWN=-1,
    CONTROLLER_DUALSHOCK3,
    CONTROLLER_DUALSHOCK4,
    CONTROLLER_DUALSENSE,
    CONTROLLER_PSCLASSIC,
    CONTROLLER_PSVR2SENSE,
    CONTROLLER_8BITDO_BTA,
    CONTROLLER_8BITDO_M30,
    CONTROLLER_8BITDO_PCE,
    CONTROLLER_HORIPAD,
    CONTROLLER_POKKEN,
    CONTROLLER_WINGMAN,
    CONTROLLER_ASTROCITY,
    CONTROLLER_GAMECUBE,
    CONTROLLER_SWITCH,
    CONTROLLER_DINPUT,
    CONTROLLER_KEYBOARD,
    CONTROLLER_MOUSE,
    // Add more controller types here
    CONTROLLER_TYPE_COUNT // Automatically equals the number of controller types
} dev_type_t;

extern DeviceInterface* device_interfaces[CONTROLLER_TYPE_COUNT];

void register_devices();

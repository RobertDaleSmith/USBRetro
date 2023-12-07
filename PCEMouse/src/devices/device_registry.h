// device_registry.h
#include "device_interface.h"

// Controller Types
typedef enum {
    CONTROLLER_DUALSHOCK3,
    CONTROLLER_DUALSHOCK4,
    CONTROLLER_DUALSENSE,
    CONTROLLER_PSCLASSIC,
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
    // Add more controller types here
    CONTROLLER_TYPE_COUNT // Automatically equals the number of controller types
} ControllerType;

extern DeviceInterface* device_interfaces[CONTROLLER_TYPE_COUNT];

void register_devices();

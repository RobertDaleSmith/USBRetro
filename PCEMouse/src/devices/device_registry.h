// device_registry.h
#include "device_interface.h"

#define MAX_DEVICE_TYPES 7  // Define the max number of devices

extern DeviceInterface* device_interfaces[MAX_DEVICE_TYPES];

void register_devices();

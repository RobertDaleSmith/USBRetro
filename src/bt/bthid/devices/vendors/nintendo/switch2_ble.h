// switch2_ble.h - Nintendo Switch 2 Controller BLE Driver
// Handles Switch 2 Pro Controller, Joy-Con 2, and NSO GameCube controller over BLE

#ifndef SWITCH2_BLE_H
#define SWITCH2_BLE_H

#include "bt/bthid/bthid.h"

// Switch 2 BLE driver
extern const bthid_driver_t switch2_ble_driver;

// Register the Switch 2 BLE driver
void switch2_ble_register(void);

// Switch 2 Product IDs (VID 0x057E - Nintendo)
#define SWITCH2_VID         0x057E
#define SWITCH2_LJC_PID     0x2066  // Left Joy-Con 2
#define SWITCH2_RJC_PID     0x2067  // Right Joy-Con 2
#define SWITCH2_PRO2_PID    0x2069  // Pro Controller 2
#define SWITCH2_GC_PID      0x2073  // NSO GameCube Controller

// BLE manufacturer data company ID for Switch 2 controllers
#define SWITCH2_BLE_COMPANY_ID  0x0553

#endif // SWITCH2_BLE_H

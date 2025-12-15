// xbox_ble.h - Xbox BLE Controller Driver
// Handles Xbox Series X/S controllers over Bluetooth Low Energy

#ifndef XBOX_BLE_H
#define XBOX_BLE_H

#include "bt/bthid/bthid.h"

// Driver struct (for direct access if needed)
extern const bthid_driver_t xbox_ble_driver;

// Register the driver with bthid layer
void xbox_ble_register(void);

#endif // XBOX_BLE_H

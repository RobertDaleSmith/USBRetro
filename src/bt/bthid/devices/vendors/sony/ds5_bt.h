// ds5_bt.h - Sony DualSense Bluetooth Driver
// Handles PS5 DualSense controllers over Bluetooth

#ifndef DS5_BT_H
#define DS5_BT_H

#include "bt/bthid/bthid.h"

// DualSense Bluetooth driver
extern const bthid_driver_t ds5_bt_driver;

// Register the DualSense BT driver
void ds5_bt_register(void);

#endif // DS5_BT_H

// ds3_bt.h - Sony DualShock 3 Bluetooth Driver
// Handles DS3 controllers over Bluetooth

#ifndef DS3_BT_H
#define DS3_BT_H

#include "bt/bthid/bthid.h"

// DS3 Bluetooth driver
extern const bthid_driver_t ds3_bt_driver;

// Register the DS3 BT driver
void ds3_bt_register(void);

#endif // DS3_BT_H

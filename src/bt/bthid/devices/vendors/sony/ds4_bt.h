// ds4_bt.h - Sony DualShock 4 Bluetooth Driver
// Handles DS4 controllers over Bluetooth

#ifndef DS4_BT_H
#define DS4_BT_H

#include "bt/bthid/bthid.h"

// DS4 Bluetooth driver
extern const bthid_driver_t ds4_bt_driver;

// Register the DS4 BT driver
void ds4_bt_register(void);

#endif // DS4_BT_H

// xbox_bt.h - Xbox Bluetooth Controller Driver
// Handles Xbox One/Series controllers over Bluetooth

#ifndef XBOX_BT_H
#define XBOX_BT_H

#include "bt/bthid/bthid.h"

// Xbox Bluetooth driver
extern const bthid_driver_t xbox_bt_driver;

// Register the Xbox BT driver
void xbox_bt_register(void);

#endif // XBOX_BT_H

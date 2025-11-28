// switch_pro_bt.h - Nintendo Switch Pro Controller Bluetooth Driver
// Handles Switch Pro and Joy-Con controllers over Bluetooth

#ifndef SWITCH_PRO_BT_H
#define SWITCH_PRO_BT_H

#include "bt/bthid/bthid.h"

// Switch Pro Bluetooth driver
extern const bthid_driver_t switch_pro_bt_driver;

// Register the Switch Pro BT driver
void switch_pro_bt_register(void);

#endif // SWITCH_PRO_BT_H

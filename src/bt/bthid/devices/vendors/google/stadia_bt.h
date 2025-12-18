// stadia_bt.h - Google Stadia Controller Bluetooth driver
#ifndef STADIA_BT_H
#define STADIA_BT_H

#include "bt/bthid/bthid.h"

// Google Stadia Controller (BLE)
// VID: 0x18D1 (Google)
// PID: 0x9400

extern const bthid_driver_t stadia_bt_driver;

void stadia_bt_register(void);

#endif // STADIA_BT_H

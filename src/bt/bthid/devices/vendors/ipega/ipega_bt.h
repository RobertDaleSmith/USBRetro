// ipega_bt.h - iPega PG-9021 Bluetooth gamepad driver
#ifndef IPEGA_BT_H
#define IPEGA_BT_H

#include "bt/bthid/bthid.h"

// iPega PG-9021 (classic Bluetooth gamepad)
// VID: 0x1949 (IPEGA)
// PID: 0x0404 in BT mode (0x0402 when wired)
// Advertised name: "PG-9021" / "ipega classic gamepad"

extern const bthid_driver_t ipega_bt_driver;

void ipega_bt_register(void);

#endif // IPEGA_BT_H

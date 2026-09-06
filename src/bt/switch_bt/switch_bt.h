// switch_bt.h - Switch Pro Controller Bluetooth Classic HID-device output
// SPDX-License-Identifier: Apache-2.0
//
// Drives the transport-agnostic switch_proto engine over a BTstack Classic (BR/EDR)
// HID-device role: SDP record, L2CAP control/interrupt channels, GAP identity, and
// the input stream. Selected via BLE_MODE_SWITCH_BT on Classic-capable builds
// (CONFIG_BT_CLASSIC_OUTPUT); ble_output dispatches init/late_init/task here.

#ifndef SWITCH_BT_H
#define SWITCH_BT_H

#include <stdbool.h>

// Pre-BTstack init (load state). Safe to call before the stack is up.
void switch_bt_init(void);

// BTstack-dependent bring-up: SDP + HID device role + Classic GAP. Call after
// hci power-up (from the BT_POST_INIT hook).
void switch_bt_late_init(void);

// Periodic task: pull the merged controller state from the router and pump the
// device->host report stream. Call from the main loop.
void switch_bt_task(void);

// True while a Switch is connected on the interrupt channel.
bool switch_bt_is_connected(void);

#endif // SWITCH_BT_H

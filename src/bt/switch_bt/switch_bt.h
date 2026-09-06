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

// Register the Classic HID device role (SDP records + L2CAP services). MUST be
// called after l2cap_init() and sdp_init() — call it from the BT host setup right
// after those, in Switch-BT mode. Registering before l2cap_init is silently wiped.
void switch_bt_register_device(void);

// Periodic task: pull the merged controller state from the router and pump the
// device->host report stream. Call from the main loop.
void switch_bt_task(void);

// True while a Switch is connected on the interrupt channel.
bool switch_bt_is_connected(void);

// Enter "sync mode": forget the current Classic bond and re-advertise so the Switch's
// Change Grip/Order screen detects us as a fresh, unpaired controller (the firmware
// equivalent of holding the sync button). Safe to call from the main loop — the work
// is marshalled onto the BTstack run loop. Intended for a user-button click when in
// Switch-BT mode and not connected; press L+R on the console afterwards to pair.
void switch_bt_request_sync(void);

// Apply the Pro Controller Classic GAP identity (name + gamepad CoD + EIR). Called
// from the BT-host HCI_STATE_WORKING handler so it isn't clobbered by the host's
// default identity. No-op unless built with CONFIG_BT_CLASSIC_OUTPUT.
void switch_bt_apply_gap_identity(void);

#endif // SWITCH_BT_H

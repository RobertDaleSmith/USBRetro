// steam_controller_ble.h - Valve Steam Controller 1 (original) over Bluetooth LE
//
// The original Steam Controller, running the community/Valve nRF51822 BLE
// firmware, exposes the same Valve GATT service as the SC2 (100F6C32) but a
// different input characteristic (100F6C33) and a delta-compressed report
// format (see steam_controller_ble.c). The Valve GATT client in btstack_host
// brings it up, forces a fast connection interval, and routes its notifications
// here (synthetic VID/PID 28DE:1101 so this driver — not the SC2 one — matches).
//
// SPDX-License-Identifier: Apache-2.0

#ifndef STEAM_CONTROLLER_BLE_H
#define STEAM_CONTROLLER_BLE_H

#define SC1_BLE_VID  0x28DE
#define SC1_BLE_PID  0x1101   // synthetic; set by the Valve GATT client for the SC1

void steam_controller_ble_register(void);

#endif // STEAM_CONTROLLER_BLE_H

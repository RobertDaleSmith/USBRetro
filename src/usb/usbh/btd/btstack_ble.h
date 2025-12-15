// btstack_ble.h - BTstack-based BLE handler for HID over GATT
//
// Provides a clean interface for BLE HID devices using BTstack's
// Security Manager (for LE Secure Connections) and GATT client.

#ifndef BTSTACK_BLE_H
#define BTSTACK_BLE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef USE_BTSTACK
#define USE_BTSTACK 0
#endif

#if USE_BTSTACK

#include "bluetooth.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback types
typedef void (*btstack_ble_report_callback_t)(uint16_t handle, const uint8_t *report, uint16_t len);
typedef void (*btstack_ble_connect_callback_t)(uint16_t handle, bool connected);

// Initialize BTstack BLE subsystem
void btstack_ble_init(void);

// Power on the Bluetooth controller
void btstack_ble_power_on(void);

// Start/stop BLE scanning
void btstack_ble_start_scan(void);
void btstack_ble_stop_scan(void);

// Connect to a BLE device
void btstack_ble_connect(bd_addr_t addr, bd_addr_type_t addr_type);

// Register callbacks
void btstack_ble_register_report_callback(btstack_ble_report_callback_t callback);
void btstack_ble_register_connect_callback(btstack_ble_connect_callback_t callback);

// Must be called from main loop
void btstack_ble_process(void);

// Status queries
bool btstack_ble_is_initialized(void);
bool btstack_ble_is_powered_on(void);
bool btstack_ble_is_scanning(void);

#ifdef __cplusplus
}
#endif

#endif // USE_BTSTACK

#endif // BTSTACK_BLE_H

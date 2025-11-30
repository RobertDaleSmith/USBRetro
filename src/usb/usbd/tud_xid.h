// tud_xid.h - TinyUSB XID class driver for Original Xbox
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Custom USB device class driver implementing Xbox XID protocol.
// Reference: OGX-Mini (BSD-3-Clause)

#ifndef TUD_XID_H
#define TUD_XID_H

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "descriptors/xbox_og_descriptors.h"

// ============================================================================
// XID CONFIGURATION
// ============================================================================

#ifndef CFG_TUD_XID
#define CFG_TUD_XID 0
#endif

#ifndef CFG_TUD_XID_EP_BUFSIZE
#define CFG_TUD_XID_EP_BUFSIZE 32
#endif

// ============================================================================
// XID TYPES
// ============================================================================

typedef enum {
    XID_TYPE_GAMEPAD = 0,       // Standard Duke/Controller S
    XID_TYPE_STEELBATTALION,    // Steel Battalion controller (not implemented)
} xid_type_t;

// ============================================================================
// XID API
// ============================================================================

// Check if XID device is ready to send a report
bool tud_xid_ready(void);

// Send gamepad input report (20 bytes)
// Returns true if transfer was queued successfully
bool tud_xid_send_report(const xbox_og_in_report_t* report);

// Get rumble output report (6 bytes)
// Call this to retrieve the latest rumble values from host
// Returns true if rumble data is available
bool tud_xid_get_rumble(xbox_og_out_report_t* rumble);

// ============================================================================
// CLASS DRIVER (internal)
// ============================================================================

// Get the XID class driver for registration
const usbd_class_driver_t* tud_xid_class_driver(void);

#endif // TUD_XID_H

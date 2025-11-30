// tud_xinput.h - TinyUSB XInput class driver for Xbox 360
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Custom USB device class driver implementing Xbox 360 XInput protocol.
// XInput uses vendor class 0xFF, subclass 0x5D, protocol 0x01.
//
// Reference: GP2040-CE, OGX-Mini (MIT/BSD-3-Clause)

#ifndef TUD_XINPUT_H
#define TUD_XINPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "descriptors/xinput_descriptors.h"

// ============================================================================
// XINPUT CONFIGURATION
// ============================================================================

#ifndef CFG_TUD_XINPUT
#define CFG_TUD_XINPUT 0
#endif

#ifndef CFG_TUD_XINPUT_EP_BUFSIZE
#define CFG_TUD_XINPUT_EP_BUFSIZE 32
#endif

// ============================================================================
// XINPUT API
// ============================================================================

// Check if XInput device is ready to send a report
bool tud_xinput_ready(void);

// Send gamepad input report (20 bytes)
// Returns true if transfer was queued successfully
bool tud_xinput_send_report(const xinput_in_report_t* report);

// Get rumble/LED output report (8 bytes)
// Call this to retrieve the latest rumble/LED values from host
// Returns true if output data is available
bool tud_xinput_get_output(xinput_out_report_t* output);

// ============================================================================
// CLASS DRIVER (internal)
// ============================================================================

// Get the XInput class driver for registration
const usbd_class_driver_t* tud_xinput_class_driver(void);

#endif // TUD_XINPUT_H

// usbd.h - USB device output
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#ifndef USBD_H
#define USBD_H

#include <stdint.h>
#include <stdbool.h>
#include "core/output_interface.h"

// USB Device configuration
#define USB_DEVICE_VENDOR_ID   0xCAFE  // TODO: Get proper VID
#define USB_DEVICE_PRODUCT_ID  0x4001  // Generic HID gamepad
#define USB_DEVICE_BCD_DEVICE  0x0100  // v1.0

// String descriptors
#define USB_STRING_MANUFACTURER  "USBRetro"
#define USB_STRING_PRODUCT       "USB Gamepad Adapter"
#define USB_STRING_SERIAL        "000000000001"

// HID Interface number
#define ITF_NUM_HID  0

// Output modes (console types) - for future expansion
typedef enum {
    USB_OUTPUT_MODE_HID = 0,            // Generic HID gamepad (DInput)
    USB_OUTPUT_MODE_XBOX_ORIGINAL,      // Original Xbox
    USB_OUTPUT_MODE_XINPUT,             // Xbox 360/One/Series (XInput)
    USB_OUTPUT_MODE_PS3,                // PlayStation 3 (DInput/SixAxis)
    USB_OUTPUT_MODE_PS4,                // PlayStation 4
    USB_OUTPUT_MODE_SWITCH,             // Nintendo Switch (docked)
    USB_OUTPUT_MODE_COUNT
} usb_output_mode_t;

// Gamepad button masks (matches USBRetro button layout)
#define USB_GAMEPAD_MASK_B1    (1U << 0)   // Face bottom (A/Cross)
#define USB_GAMEPAD_MASK_B2    (1U << 1)   // Face right (B/Circle)
#define USB_GAMEPAD_MASK_B3    (1U << 2)   // Face left (X/Square)
#define USB_GAMEPAD_MASK_B4    (1U << 3)   // Face top (Y/Triangle)
#define USB_GAMEPAD_MASK_L1    (1U << 4)   // Left bumper
#define USB_GAMEPAD_MASK_R1    (1U << 5)   // Right bumper
#define USB_GAMEPAD_MASK_L2    (1U << 6)   // Left trigger (digital)
#define USB_GAMEPAD_MASK_R2    (1U << 7)   // Right trigger (digital)
#define USB_GAMEPAD_MASK_S1    (1U << 8)   // Select/Back
#define USB_GAMEPAD_MASK_S2    (1U << 9)   // Start
#define USB_GAMEPAD_MASK_L3    (1U << 10)  // Left stick click
#define USB_GAMEPAD_MASK_R3    (1U << 11)  // Right stick click
#define USB_GAMEPAD_MASK_A1    (1U << 12)  // Home/Guide
#define USB_GAMEPAD_MASK_A2    (1U << 13)  // Capture/Touchpad

// D-pad masks
#define USB_DPAD_MASK_UP       (1U << 0)
#define USB_DPAD_MASK_DOWN     (1U << 1)
#define USB_DPAD_MASK_LEFT     (1U << 2)
#define USB_DPAD_MASK_RIGHT    (1U << 3)

// Function declarations
void usbd_init(void);
void usbd_task(void);
bool usbd_send_report(uint8_t player_index);

// Output interface for app integration
extern const OutputInterface usbd_output_interface;

#endif // USBD_H

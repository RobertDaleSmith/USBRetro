// switch_descriptors.h - Nintendo Switch USB HID descriptors
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Nintendo Switch Pro Controller compatible descriptors.
// Uses HORI Pokken Controller VID/PID for broad compatibility.
//
// Reference: GP2040-CE, OGX-Mini (MIT/BSD-3-Clause)

#ifndef SWITCH_DESCRIPTORS_H
#define SWITCH_DESCRIPTORS_H

#include <stdint.h>
#include "tusb.h"

// ============================================================================
// SWITCH USB IDENTIFIERS
// ============================================================================

// HORI Pokken Controller - widely compatible with Switch
#define SWITCH_VID              0x0F0D  // HORI
#define SWITCH_PID              0x0092  // Pokken Controller
#define SWITCH_BCD_DEVICE       0x0100  // v1.0

// Alternative: Nintendo Pro Controller (requires handshake)
// #define SWITCH_VID           0x057E  // Nintendo
// #define SWITCH_PID           0x2009  // Pro Controller

// ============================================================================
// SWITCH BUTTON DEFINITIONS
// ============================================================================

// Button masks (16-bit)
#define SWITCH_MASK_Y       (1U <<  0)
#define SWITCH_MASK_B       (1U <<  1)
#define SWITCH_MASK_A       (1U <<  2)
#define SWITCH_MASK_X       (1U <<  3)
#define SWITCH_MASK_L       (1U <<  4)
#define SWITCH_MASK_R       (1U <<  5)
#define SWITCH_MASK_ZL      (1U <<  6)
#define SWITCH_MASK_ZR      (1U <<  7)
#define SWITCH_MASK_MINUS   (1U <<  8)
#define SWITCH_MASK_PLUS    (1U <<  9)
#define SWITCH_MASK_L3      (1U << 10)
#define SWITCH_MASK_R3      (1U << 11)
#define SWITCH_MASK_HOME    (1U << 12)
#define SWITCH_MASK_CAPTURE (1U << 13)

// D-pad / Hat switch values
#define SWITCH_HAT_UP        0x00
#define SWITCH_HAT_UP_RIGHT  0x01
#define SWITCH_HAT_RIGHT     0x02
#define SWITCH_HAT_DOWN_RIGHT 0x03
#define SWITCH_HAT_DOWN      0x04
#define SWITCH_HAT_DOWN_LEFT 0x05
#define SWITCH_HAT_LEFT      0x06
#define SWITCH_HAT_UP_LEFT   0x07
#define SWITCH_HAT_CENTER    0x08

// Analog stick range
#define SWITCH_JOYSTICK_MIN  0x00
#define SWITCH_JOYSTICK_MID  0x80
#define SWITCH_JOYSTICK_MAX  0xFF

// ============================================================================
// SWITCH REPORT STRUCTURES
// ============================================================================

// Input Report (gamepad state) - 8 bytes
typedef struct __attribute__((packed)) {
    uint16_t buttons;        // 16 button bits
    uint8_t  hat;            // D-pad (hat switch, 0-8)
    uint8_t  lx;             // Left stick X (0-255, 128 = center)
    uint8_t  ly;             // Left stick Y (0-255, 128 = center)
    uint8_t  rx;             // Right stick X (0-255, 128 = center)
    uint8_t  ry;             // Right stick Y (0-255, 128 = center)
    uint8_t  vendor;         // Vendor-specific byte
} switch_in_report_t;

_Static_assert(sizeof(switch_in_report_t) == 8, "switch_in_report_t must be 8 bytes");

// Output Report (rumble) - 8 bytes
typedef struct __attribute__((packed)) {
    uint8_t  data[8];        // Vendor-specific rumble data
} switch_out_report_t;

_Static_assert(sizeof(switch_out_report_t) == 8, "switch_out_report_t must be 8 bytes");

// ============================================================================
// SWITCH USB DESCRIPTORS
// ============================================================================

// HID Report Descriptor for Switch (86 bytes)
static const uint8_t switch_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x01,        //   Physical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x10,        //   Usage Maximum (Button 16)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x25, 0x07,        //   Logical Maximum (7)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x65, 0x14,        //   Unit (Eng Rot:Angular Pos)
    0x09, 0x39,        //   Usage (Hat switch)
    0x81, 0x42,        //   Input (Data,Var,Abs,Null)
    0x65, 0x00,        //   Unit (None)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const) - 4-bit padding
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x46, 0xFF, 0x00,  //   Physical Maximum (255)
    0x09, 0x30,        //   Usage (X) - Left Stick X
    0x09, 0x31,        //   Usage (Y) - Left Stick Y
    0x09, 0x32,        //   Usage (Z) - Right Stick X
    0x09, 0x35,        //   Usage (Rz) - Right Stick Y
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined)
    0x09, 0x20,        //   Usage (0x20)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs) - Vendor byte
    0x0A, 0x21, 0x26,  //   Usage (0x2621)
    0x95, 0x08,        //   Report Count (8)
    0x91, 0x02,        //   Output (Data,Var,Abs) - Rumble
    0xC0,              // End Collection
};

// Device descriptor
static const tusb_desc_device_t switch_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0x00,    // Use class from interface
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = SWITCH_VID,
    .idProduct          = SWITCH_PID,
    .bcdDevice          = SWITCH_BCD_DEVICE,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01
};

// Configuration descriptor (41 bytes total)
// 9 (config) + 9 (interface) + 9 (HID) + 7 (EP OUT) + 7 (EP IN) = 41
#define SWITCH_CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static const uint8_t switch_config_descriptor[] = {
    // Config descriptor
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, SWITCH_CONFIG_TOTAL_LEN, 0x80, 250),  // 500mA

    // Interface + HID + Endpoints
    // Interface
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_HID, 0, 0, 0,

    // HID descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(switch_report_descriptor)),

    // Endpoint OUT (for rumble)
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,

    // Endpoint IN (for reports)
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
};

// String descriptors
#define SWITCH_MANUFACTURER  "HORI CO.,LTD."
#define SWITCH_PRODUCT       "POKKEN CONTROLLER"

#endif // SWITCH_DESCRIPTORS_H

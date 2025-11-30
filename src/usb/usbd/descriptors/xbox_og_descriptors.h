// xbox_og_descriptors.h - Original Xbox (XID) USB descriptors
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Reference: OGX-Mini (BSD-3-Clause)
// Xbox uses a proprietary XID protocol instead of standard HID

#ifndef XBOX_OG_DESCRIPTORS_H
#define XBOX_OG_DESCRIPTORS_H

#include <stdint.h>
#include "tusb.h"

// ============================================================================
// XBOX OG USB IDENTIFIERS
// ============================================================================

#define XBOX_OG_VID             0x045E  // Microsoft
#define XBOX_OG_PID             0x0289  // Xbox Controller S
#define XBOX_OG_BCD_DEVICE      0x0121

// XID Interface Class/Subclass
#define XID_INTERFACE_CLASS     0x58
#define XID_INTERFACE_SUBCLASS  0x42

// ============================================================================
// XBOX OG CONTROL REQUEST CONSTANTS
// ============================================================================

// GET_DESC request (returns XID device descriptor)
#define XID_REQ_GET_DESC_TYPE   0xC1
#define XID_REQ_GET_DESC        0x06
#define XID_REQ_GET_DESC_VALUE  0x4200

// GET_CAP request (returns capabilities)
#define XID_REQ_GET_CAP_TYPE    0xC1
#define XID_REQ_GET_CAP         0x01
#define XID_REQ_GET_CAP_IN      0x0100
#define XID_REQ_GET_CAP_OUT     0x0200

// GET_REPORT request (returns current gamepad state)
#define XID_REQ_GET_REPORT_TYPE 0xA1
#define XID_REQ_GET_REPORT      0x01
#define XID_REQ_GET_REPORT_VAL  0x0100

// SET_REPORT request (receives rumble)
#define XID_REQ_SET_REPORT_TYPE 0x21
#define XID_REQ_SET_REPORT      0x09
#define XID_REQ_SET_REPORT_VAL  0x0200

// ============================================================================
// XBOX OG BUTTON DEFINITIONS
// ============================================================================

// Digital buttons (byte 2 of report)
#define XBOX_OG_BTN_DPAD_UP     (1 << 0)
#define XBOX_OG_BTN_DPAD_DOWN   (1 << 1)
#define XBOX_OG_BTN_DPAD_LEFT   (1 << 2)
#define XBOX_OG_BTN_DPAD_RIGHT  (1 << 3)
#define XBOX_OG_BTN_START       (1 << 4)
#define XBOX_OG_BTN_BACK        (1 << 5)
#define XBOX_OG_BTN_L3          (1 << 6)
#define XBOX_OG_BTN_R3          (1 << 7)

// ============================================================================
// XBOX OG REPORT STRUCTURES
// ============================================================================

// Input Report (gamepad state) - 20 bytes
typedef struct __attribute__((packed)) {
    uint8_t  reserved1;      // Always 0x00
    uint8_t  report_len;     // Always 0x14 (20)
    uint8_t  buttons;        // Digital buttons (DPAD, Start, Back, L3, R3)
    uint8_t  reserved2;      // Always 0x00
    uint8_t  a;              // A button (analog, 0-255)
    uint8_t  b;              // B button (analog, 0-255)
    uint8_t  x;              // X button (analog, 0-255)
    uint8_t  y;              // Y button (analog, 0-255)
    uint8_t  black;          // Black button (analog, 0-255) - maps to L1
    uint8_t  white;          // White button (analog, 0-255) - maps to R1
    uint8_t  trigger_l;      // Left trigger (analog, 0-255)
    uint8_t  trigger_r;      // Right trigger (analog, 0-255)
    int16_t  stick_lx;       // Left stick X (-32768 to 32767)
    int16_t  stick_ly;       // Left stick Y (-32768 to 32767)
    int16_t  stick_rx;       // Right stick X (-32768 to 32767)
    int16_t  stick_ry;       // Right stick Y (-32768 to 32767)
} xbox_og_in_report_t;

_Static_assert(sizeof(xbox_og_in_report_t) == 20, "xbox_og_in_report_t must be 20 bytes");

// Output Report (rumble) - 6 bytes
typedef struct __attribute__((packed)) {
    uint8_t  reserved;       // Always 0x00
    uint8_t  report_len;     // Always 0x06
    uint16_t rumble_l;       // Left motor (0-65535)
    uint16_t rumble_r;       // Right motor (0-65535)
} xbox_og_out_report_t;

_Static_assert(sizeof(xbox_og_out_report_t) == 6, "xbox_og_out_report_t must be 6 bytes");

// ============================================================================
// XBOX OG USB DESCRIPTORS
// ============================================================================

// Device descriptor
static const tusb_desc_device_t xbox_og_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,  // USB 1.1
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = XBOX_OG_VID,
    .idProduct          = XBOX_OG_PID,
    .bcdDevice          = XBOX_OG_BCD_DEVICE,
    .iManufacturer      = 0x00,
    .iProduct           = 0x00,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01
};

// XID interface descriptor macro
#define TUD_XID_DESC_LEN  (9 + 7 + 7)  // Interface + EP IN + EP OUT

#define TUD_XID_DESCRIPTOR(_itfnum, _epout, _epin) \
    /* Interface */ \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, XID_INTERFACE_CLASS, XID_INTERFACE_SUBCLASS, 0x00, 0x00, \
    /* Endpoint IN */ \
    7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(32), 4, \
    /* Endpoint OUT */ \
    7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(32), 4

// Configuration descriptor
#define XBOX_OG_CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_XID_DESC_LEN)

static const uint8_t xbox_og_config_descriptor[] = {
    // Config descriptor
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, XBOX_OG_CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    // XID Interface
    TUD_XID_DESCRIPTOR(0, 0x01, 0x81),
};

// XID Device Descriptor (returned via GET_DESC request)
static const uint8_t xbox_og_xid_descriptor[] = {
    0x10,                       // bLength
    0x42,                       // bDescriptorType (XID)
    0x00, 0x01,                 // bcdXid
    0x01,                       // bType (Gamepad)
    0x02,                       // bSubType (Controller S)
    0x14,                       // bMaxInputReportSize (20)
    0x06,                       // bMaxOutputReportSize (6)
    0xFF, 0xFF, 0xFF, 0xFF,     // wAlternateProductIds (not used)
    0xFF, 0xFF, 0xFF, 0xFF
};

// XID Input Capabilities (returned via GET_CAP IN request)
static const uint8_t xbox_og_xid_capabilities_in[] = {
    0x00,                       // Reserved
    0x14,                       // bLength (20)
    0xFF,                       // Buttons supported (all)
    0x00,                       // Reserved
    0xFF,                       // A supported
    0xFF, 0xFF, 0xFF,           // B, X, Y supported
    0xFF, 0xFF, 0xFF,           // Black, White, LT supported
    0xFF, 0xFF, 0xFF,           // RT, LX (low, high) supported
    0xFF, 0xFF, 0xFF,           // LY (low, high), RX (low) supported
    0xFF, 0xFF, 0xFF            // RX (high), RY (low, high) supported
};

// XID Output Capabilities (returned via GET_CAP OUT request)
static const uint8_t xbox_og_xid_capabilities_out[] = {
    0x00,                       // Reserved
    0x06,                       // bLength (6)
    0xFF, 0xFF,                 // Rumble L supported
    0xFF, 0xFF                  // Rumble R supported
};

#endif // XBOX_OG_DESCRIPTORS_H

// psclassic_descriptors.h - PlayStation Classic controller descriptors
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
// SPDX-FileCopyrightText: Copyright (c) 2024 Robert Dale Smith
//
// PlayStation Classic (PS1 Mini) USB controller emulation.
// VID/PID: 054C:0CDA (Sony Interactive Entertainment)
// Simple 10-button digital controller with D-pad (no analog sticks).

#ifndef PSCLASSIC_DESCRIPTORS_H
#define PSCLASSIC_DESCRIPTORS_H

#include <stdint.h>
#include "tusb.h"

// ============================================================================
// USB IDENTIFIERS
// ============================================================================

#define PSCLASSIC_VID           0x054C  // Sony Interactive Entertainment
#define PSCLASSIC_PID           0x0CDA  // PlayStation Classic Controller
#define PSCLASSIC_BCD           0x0100  // v1.00
#define PSCLASSIC_MANUFACTURER  "Sony Interactive Entertainment"
#define PSCLASSIC_PRODUCT       "Controller"

#define PSCLASSIC_ENDPOINT_SIZE 64

// ============================================================================
// BUTTON MASKS
// ============================================================================

// Button report (16 bits total)
// Bits 0-9: Button states
// Bits 10-13: D-pad encoded
// Bits 14-15: Padding

#define PSCLASSIC_MASK_TRIANGLE   (1U <<  0)
#define PSCLASSIC_MASK_CIRCLE     (1U <<  1)
#define PSCLASSIC_MASK_CROSS      (1U <<  2)
#define PSCLASSIC_MASK_SQUARE     (1U <<  3)
#define PSCLASSIC_MASK_L2         (1U <<  4)
#define PSCLASSIC_MASK_R2         (1U <<  5)
#define PSCLASSIC_MASK_L1         (1U <<  6)
#define PSCLASSIC_MASK_R1         (1U <<  7)
#define PSCLASSIC_MASK_START      (1U <<  8)
#define PSCLASSIC_MASK_SELECT     (1U <<  9)

// D-pad encoding (uses upper bits)
// These values are OR'd with button bits
#define PSCLASSIC_DPAD_UP_LEFT    0x0000
#define PSCLASSIC_DPAD_UP         0x0400
#define PSCLASSIC_DPAD_UP_RIGHT   0x0800
#define PSCLASSIC_DPAD_LEFT       0x1000
#define PSCLASSIC_DPAD_CENTER     0x1400
#define PSCLASSIC_DPAD_RIGHT      0x1800
#define PSCLASSIC_DPAD_DOWN_LEFT  0x2000
#define PSCLASSIC_DPAD_DOWN       0x2400
#define PSCLASSIC_DPAD_DOWN_RIGHT 0x2800

// ============================================================================
// REPORT STRUCTURE
// ============================================================================

typedef struct __attribute__((packed)) {
    uint16_t buttons;  // 10 buttons + d-pad encoding + padding
} psclassic_in_report_t;

// Helper to initialize report to neutral state
static inline void psclassic_init_report(psclassic_in_report_t* report) {
    report->buttons = PSCLASSIC_DPAD_CENTER;  // Neutral: all buttons released, d-pad centered
}

// ============================================================================
// DEVICE DESCRIPTOR
// ============================================================================

static const tusb_desc_device_t psclassic_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0x00,    // Use class info in Interface Descriptors
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = PSCLASSIC_VID,
    .idProduct          = PSCLASSIC_PID,
    .bcdDevice          = PSCLASSIC_BCD,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,    // No serial number
    .bNumConfigurations = 0x01
};

// ============================================================================
// HID REPORT DESCRIPTOR
// ============================================================================

// 49-byte HID report descriptor for PlayStation Classic
// 10 buttons + 2-bit X axis + 2-bit Y axis (D-pad) + padding
static const uint8_t psclassic_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)

    // 10 buttons (1 bit each)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0A,        //   Report Count (10)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x0A,        //   Usage Maximum (Button 10)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // D-pad as X/Y axes (2 bits each, values 0-2)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x02,        //   Logical Maximum (2)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x02,        //   Physical Maximum (2)
    0x75, 0x02,        //   Report Size (2)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // 2-bit padding to complete the 16-bit report
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x01,        //   Input (Const,Array,Abs) - padding

    0xC0,              // End Collection
};

// ============================================================================
// CONFIGURATION DESCRIPTOR
// ============================================================================

#define PSCLASSIC_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t psclassic_config_descriptor[] = {
    // Configuration descriptor
    0x09,                           // bLength
    TUSB_DESC_CONFIGURATION,        // bDescriptorType
    U16_TO_U8S_LE(PSCLASSIC_CONFIG_TOTAL_LEN), // wTotalLength
    0x01,                           // bNumInterfaces
    0x01,                           // bConfigurationValue
    0x00,                           // iConfiguration
    0xA0,                           // bmAttributes (Remote Wakeup)
    0x32,                           // bMaxPower (100mA)

    // Interface descriptor
    0x09,                           // bLength
    TUSB_DESC_INTERFACE,            // bDescriptorType
    0x00,                           // bInterfaceNumber
    0x00,                           // bAlternateSetting
    0x01,                           // bNumEndpoints
    TUSB_CLASS_HID,                 // bInterfaceClass
    0x00,                           // bInterfaceSubClass
    0x00,                           // bInterfaceProtocol
    0x00,                           // iInterface

    // HID descriptor
    0x09,                           // bLength
    HID_DESC_TYPE_HID,              // bDescriptorType
    U16_TO_U8S_LE(0x0111),          // bcdHID (1.11)
    0x00,                           // bCountryCode
    0x01,                           // bNumDescriptors
    HID_DESC_TYPE_REPORT,           // bDescriptorType[0]
    U16_TO_U8S_LE(sizeof(psclassic_report_descriptor)), // wDescriptorLength[0]

    // Endpoint descriptor (IN)
    0x07,                           // bLength
    TUSB_DESC_ENDPOINT,             // bDescriptorType
    0x81,                           // bEndpointAddress (EP1 IN)
    TUSB_XFER_INTERRUPT,            // bmAttributes
    U16_TO_U8S_LE(PSCLASSIC_ENDPOINT_SIZE), // wMaxPacketSize
    0x0A,                           // bInterval (10ms)
};

#endif // PSCLASSIC_DESCRIPTORS_H

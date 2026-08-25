// dualsense_descriptors.h - PlayStation 5 DualSense controller descriptors
// SPDX-License-Identifier: MIT
//
// DualSense (PS5) USB controller emulation for the "controller in the loop"
// auth passthrough (ds5_auth). Modeled on ps4_descriptors.h.
//
// ============================================================================
// !!! HARDWARE-SNIFF-GATED — descriptors are RECONSTRUCTED from the documented
// report layout, NOT dumped byte-exact from a real DualSense. A fingerprinting
// PS5 may require the exact 273-byte HID report descriptor + string/BOS
// descriptors of a genuine unit. Replace with a real capture before trusting
// console behavior. See .dev/docs/ds5-auth-passthrough-plan.md.
// ============================================================================

#ifndef DUALSENSE_DESCRIPTORS_H
#define DUALSENSE_DESCRIPTORS_H

#include <stdint.h>
#include <string.h>
#include "tusb.h"

// ============================================================================
// USB IDENTIFIERS
// ============================================================================
#define DS5_VID             0x054C  // Sony Interactive Entertainment
#define DS5_PID             0x0CE6  // DualSense Wireless Controller
#define DS5_BCD             0x0100
#define DS5_MANUFACTURER    "Sony Interactive Entertainment"
#define DS5_PRODUCT         "DualSense Wireless Controller"  // real DS5 iProduct (per DS5Dongle)
#define DS5_ENDPOINT_SIZE   64

// Hat switch (same encoding as DS4; 0x08 = neutral)
#define DS5_HAT_NOTHING     0x08

// Button masks — DualSense byte 8/9/10 layout
#define DS5_MASK_SQUARE     (1U << 0)
#define DS5_MASK_CROSS      (1U << 1)
#define DS5_MASK_CIRCLE     (1U << 2)
#define DS5_MASK_TRIANGLE   (1U << 3)

#define DS5_JOYSTICK_MID    0x80

// ============================================================================
// INPUT REPORT (Report ID 0x01, 64 bytes) — documented DualSense USB layout.
//   0:      report_id (0x01)
//   1-4:    LX, LY, RX, RY (0x80 center)
//   5-6:    L2, R2 analog triggers (0x00 rest)  <-- note: before buttons (unlike DS4)
//   7:      seq counter (vendor)
//   8:      hat(4) + square/cross/circle/triangle(4)
//   9:      L1/R1/L2/R2/Create/Options/L3/R3
//   10:     PS/Touchpad/Mute + counter(5)
//   11:     reserved
//   12-63:  vendor: IMU/touchpad/battery (zeroed until wired)
// ============================================================================
typedef struct __attribute__((packed)) {
    uint8_t report_id;          // 0x01
    uint8_t lx, ly, rx, ry;     // sticks
    uint8_t l2_trigger;         // byte 5
    uint8_t r2_trigger;         // byte 6
    uint8_t counter;            // byte 7 (vendor seq)

    // byte 8: hat + face buttons
    uint8_t dpad : 4;
    uint8_t square : 1;
    uint8_t cross : 1;
    uint8_t circle : 1;
    uint8_t triangle : 1;

    // byte 9
    uint8_t l1 : 1;
    uint8_t r1 : 1;
    uint8_t l2 : 1;
    uint8_t r2 : 1;
    uint8_t create : 1;   // Share/Create
    uint8_t options : 1;
    uint8_t l3 : 1;
    uint8_t r3 : 1;

    // byte 10
    uint8_t ps : 1;
    uint8_t touchpad : 1;
    uint8_t mute : 1;
    uint8_t counter2 : 5;

    uint8_t reserved;           // byte 11
    uint8_t vendor[52];         // bytes 12-63: IMU/touchpad/battery (zeroed)
} ds5_in_report_t;

_Static_assert(sizeof(ds5_in_report_t) == 64, "DualSense input report must be 64 bytes");

static inline void ds5_init_report(ds5_in_report_t* r) {
    memset(r, 0, sizeof(*r));
    r->report_id = 0x01;
    r->lx = r->ly = r->rx = r->ry = DS5_JOYSTICK_MID;
    r->dpad = DS5_HAT_NOTHING;
}

// ============================================================================
// DEVICE DESCRIPTOR
// ============================================================================
static const tusb_desc_device_t ds5_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = DS5_VID,
    .idProduct          = DS5_PID,
    .bcdDevice          = DS5_BCD,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01
};

// ============================================================================
// HID REPORT DESCRIPTOR — the REAL 321-byte DualSense USB descriptor, verbatim
// from a genuine unit (via awalol/DS5Dongle src/usb_descriptors.cpp). Input
// 0x01, output 0x02, feature 0x05/0x08-0x0C/0x20-0x22/0x80-0x85/0xA0/0xE0, and
// the auth feature reports 0xF0(set 63)/0xF1(get 63)/0xF2(status 15)/0xF4(set
// 63)/0xF5(get 3)/0xF6-0xF9. The declared input report is larger than the 64
// bytes we send; the DualSense also sends a short report and hosts accept it.
// ============================================================================
static const uint8_t ds5_report_descriptor[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x05,       // Usage (Game Pad)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x09, 0x33, 0x09, 0x34, // X,Y,Z,Rz,Rx,Ry
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x06, 0x81, 0x02,
    0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, // vendor 0x20 counter (1B)
    0x05, 0x01, 0x09, 0x39,                               // Hat switch
    0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
    0x65, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0F,       // 15 buttons
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0F, 0x81, 0x02,
    0x06, 0x00, 0xFF, 0x09, 0x21, 0x95, 0x0D, 0x81, 0x02, // vendor 0x21 (13B)
    0x06, 0x00, 0xFF, 0x09, 0x22, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x34, 0x81, 0x02, // vendor 0x22 (52B)
    0x85, 0x02, 0x09, 0x23, 0x95, 0x2F, 0x91, 0x02,       // Output 0x02 (47B)
    0x85, 0x05, 0x09, 0x33, 0x95, 0x28, 0xB1, 0x02,       // Feature 0x05 (40B)
    0x85, 0x08, 0x09, 0x34, 0x95, 0x2F, 0xB1, 0x02,       // Feature 0x08 (47B)
    0x85, 0x09, 0x09, 0x24, 0x95, 0x13, 0xB1, 0x02,       // Feature 0x09 (19B)
    0x85, 0x0A, 0x09, 0x25, 0x95, 0x1A, 0xB1, 0x02,       // Feature 0x0A (26B)
    0x85, 0x0B, 0x09, 0x41, 0x95, 0x29, 0xB1, 0x02,       // Feature 0x0B (41B)
    0x85, 0x0C, 0x09, 0x42, 0x95, 0x29, 0xB1, 0x02,       // Feature 0x0C (41B)
    0x85, 0x20, 0x09, 0x26, 0x95, 0x3F, 0xB1, 0x02,       // Feature 0x20 firmware (63B)
    0x85, 0x21, 0x09, 0x27, 0x95, 0x04, 0xB1, 0x02,       // Feature 0x21 (4B)
    0x85, 0x22, 0x09, 0x40, 0x95, 0x3F, 0xB1, 0x02,       // Feature 0x22 (63B)
    0x85, 0x80, 0x09, 0x28, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0x81, 0x09, 0x29, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0x82, 0x09, 0x2A, 0x95, 0x09, 0xB1, 0x02,
    0x85, 0x83, 0x09, 0x2B, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0x84, 0x09, 0x2C, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0x85, 0x09, 0x2D, 0x95, 0x02, 0xB1, 0x02,
    0x85, 0xA0, 0x09, 0x2E, 0x95, 0x01, 0xB1, 0x02,
    0x85, 0xE0, 0x09, 0x2F, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0xF0, 0x09, 0x30, 0x95, 0x3F, 0xB1, 0x02,       // 0xF0 auth set nonce (63B)
    0x85, 0xF1, 0x09, 0x31, 0x95, 0x3F, 0xB1, 0x02,       // 0xF1 auth get sig (63B)
    0x85, 0xF2, 0x09, 0x32, 0x95, 0x0F, 0xB1, 0x02,       // 0xF2 auth status (15B)
    0x85, 0xF4, 0x09, 0x35, 0x95, 0x3F, 0xB1, 0x02,       // 0xF4 (63B)
    0x85, 0xF5, 0x09, 0x36, 0x95, 0x03, 0xB1, 0x02,       // 0xF5 (3B)
    0x85, 0xF6, 0x09, 0x37, 0x95, 0x3F, 0xB1, 0x02,       // 0xF6 (63B)
    0x85, 0xF7, 0x09, 0x38, 0x95, 0x3F, 0xB1, 0x02,       // 0xF7 (63B)
    0x85, 0xF8, 0x09, 0x39, 0x95, 0x3F, 0xB1, 0x02,       // 0xF8 (63B)
    0x85, 0xF9, 0x09, 0x3A, 0x95, 0x3F, 0xB1, 0x02,       // 0xF9 (63B)
    0xC0,             // End Collection
};
_Static_assert(sizeof(ds5_report_descriptor) == 321, "DualSense HID report descriptor must be the real 321 bytes");

// ============================================================================
// CONFIGURATION DESCRIPTOR
// ============================================================================
#define DS5_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static const uint8_t ds5_config_descriptor[] = {
    0x09, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(DS5_CONFIG_TOTAL_LEN),
    0x01, 0x01, 0x00, 0x80, 0xFA,   // 1 iface, bus powered, 500mA

    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,

    0x09, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0x00, 0x01,
    HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(ds5_report_descriptor)),

    0x07, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(DS5_ENDPOINT_SIZE), 0x01,
    0x07, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(DS5_ENDPOINT_SIZE), 0x01,
};

// Auth report IDs
#define DS5_REPORT_ID_INPUT     0x01
#define DS5_REPORT_ID_OUTPUT    0x02
#define DS5_REPORT_ID_AUTH_NONCE    0xF0
#define DS5_REPORT_ID_AUTH_RESPONSE 0xF1
#define DS5_REPORT_ID_AUTH_STATUS   0xF2
#define DS5_REPORT_ID_AUTH_RESET    0xF3

// Auth reset page-size blob (0xF3): nonce/response page sizes (0x38 = 56)
static const uint8_t ds5_feature_f3[] = { 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00 };

// Firmware-info feature report 0x20 (63B) — a genuine DualSense's response
// (firmware date "Jul  4 2025 10:38:40" + versions). Hosts/PS5 query this during
// DualSense identification. From awalol/DS5Dongle src/fake_ds5.h report20[].
#define DS5_REPORT_ID_FIRMWARE  0x20
static const uint8_t ds5_feature_20[63] = {
    0x4a, 0x75, 0x6c, 0x20, 0x20, 0x34, 0x20, 0x32,
    0x30, 0x32, 0x35, 0x31, 0x30, 0x3a, 0x33, 0x38,
    0x3a, 0x34, 0x30, 0x03, 0x00, 0x0b, 0x00, 0x11,
    0x08, 0x00, 0x00, 0x2a, 0x00, 0x10, 0x01, 0x00,
    0x28, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x30, 0x06, 0x00, 0x00, 0x01,
    0x00, 0x03, 0x00, 0x10, 0x10, 0x03, 0x00, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#endif // DUALSENSE_DESCRIPTORS_H

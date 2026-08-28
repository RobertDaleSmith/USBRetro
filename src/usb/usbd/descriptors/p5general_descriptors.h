// p5general_descriptors.h - PlayStation 5 "P5General" output descriptors + report
// SPDX-License-Identifier: MIT
//
// Ported from GP2040-CE (MIT, (c) 2024 OpenStickCommunity) — see
// headers/drivers/p5general/P5GeneralDescriptors.h. Presents to the PS5 as the
// licensed "Activtor / P5General" controller (VID 0x2B81 / PID 0x0101). A real
// P5General dongle on the USB host port answers the F0/F1/F2 auth AND signs
// every 64-byte report (trailing hash[8]); we build the controller-state portion
// here and the dongle finishes it. See .dev/docs/ps5-p5general-*.md.
//
// C port note: GP2040's report struct uses C++ helper methods (set_x/set_y);
// here the touch X/Y are packed by hand (p5general_pack_touch), like ds5.

#ifndef P5GENERAL_DESCRIPTORS_H
#define P5GENERAL_DESCRIPTORS_H

#include <stdint.h>
#include "tusb.h"
#include "usb/usbh/hid/devices/vendors/sony/p5general_host.h"  // shared VID/PID + report IDs

// ============================================================================
// IDENTITY + CONSTANTS  (VID/PID + feature report IDs live in p5general_host.h)
// ============================================================================
#define P5GENERAL_ENDPOINT_SIZE 64
#define P5GENERAL_MANUFACTURER  "Activtor"
#define P5GENERAL_PRODUCT       "P5General"

// HAT (4-bit dpad)
#define P5GENERAL_HAT_UP        0x00
#define P5GENERAL_HAT_UPRIGHT   0x01
#define P5GENERAL_HAT_RIGHT     0x02
#define P5GENERAL_HAT_DOWNRIGHT 0x03
#define P5GENERAL_HAT_DOWN      0x04
#define P5GENERAL_HAT_DOWNLEFT  0x05
#define P5GENERAL_HAT_LEFT      0x06
#define P5GENERAL_HAT_UPLEFT    0x07
#define P5GENERAL_HAT_NOTHING   0x0F

// 8-bit sticks
#define P5GENERAL_JOYSTICK_MIN  0x00
#define P5GENERAL_JOYSTICK_MID  0x80
#define P5GENERAL_JOYSTICK_MAX  0xFF

// touchpad 1920x943
#define P5GENERAL_TP_X_MIN 0
#define P5GENERAL_TP_X_MAX 1920
#define P5GENERAL_TP_Y_MIN 0
#define P5GENERAL_TP_Y_MAX 943
#define P5GENERAL_TP_MAX_COUNT 128

// Feature report IDs (device <-> PS5 and device -> dongle share these)
#define P5GENERAL_REPORT_ID_INPUT       0x01
#define P5GENERAL_REPORT_ID_OUTPUT      0x02
#define P5GENERAL_REPORT_DEFINITION     0x03  // GET feature: device definition blob
#define P5GENERAL_SET_AUTH_PAYLOAD      0xF0  // SET feature: PS5 -> auth challenge (F0)
#define P5GENERAL_GET_SIGNATURE_NONCE   0xF1  // GET feature: signature/nonce (F1)
#define P5GENERAL_GET_SIGNING_STATE     0xF2  // GET feature: signing state (F2)

// ============================================================================
// REPORT STRUCTS (64-byte input report; byte-exact with GP2040 P5GenerorReport)
// ============================================================================
typedef struct __attribute__((packed)) {
    int16_t x;
    int16_t y;
    int16_t z;
} p5general_sensor_t;   // 6 bytes

typedef struct __attribute__((packed)) {
    uint8_t counter : 7;
    uint8_t unpressed : 1;
    uint8_t data[3];    // 12-bit X then 12-bit Y (see p5general_pack_touch)
} p5general_touch_xy_t; // 4 bytes

typedef struct __attribute__((packed)) {
    p5general_touch_xy_t p1;
    p5general_touch_xy_t p2;
} p5general_touchpad_t; // 8 bytes

typedef struct __attribute__((packed)) {
    uint8_t report_id;       // 0
    uint8_t left_stick_x;    // 1
    uint8_t left_stick_y;    // 2
    uint8_t right_stick_x;   // 3
    uint8_t right_stick_y;   // 4
    uint8_t left_trigger;    // 5
    uint8_t right_trigger;   // 6
    uint8_t reportCounter;   // 7

    uint8_t dpad : 4;        // 8
    uint8_t button_west : 1;
    uint8_t button_south : 1;
    uint8_t button_east : 1;
    uint8_t button_north : 1;

    uint8_t button_l1 : 1;   // 9
    uint8_t button_r1 : 1;
    uint8_t button_l2 : 1;
    uint8_t button_r2 : 1;
    uint8_t button_select : 1;
    uint8_t button_start : 1;
    uint8_t button_l3 : 1;
    uint8_t button_r3 : 1;

    uint8_t button_home : 1;   // 10
    uint8_t button_touchpad : 1;
    uint8_t : 6;

    uint8_t data_11;                 // 11
    uint32_t auth_seq_number;        // 12-15
    p5general_sensor_t gyroscope;    // 16-21
    p5general_sensor_t accelerometer;// 22-27
    uint16_t data_28_29;             // 28-29
    uint16_t data_30_31_0x001a;      // 30-31
    p5general_touchpad_t touchpad_data; // 32-39
    uint8_t data_40_55[16];          // 40-55
    uint8_t hash[8];                 // 56-63
} p5general_report_t;

_Static_assert(sizeof(p5general_report_t) == 64, "P5General report must be 64 bytes");

// Pack a 12-bit X / 12-bit Y into a touch point's 3-byte data field.
static inline void p5general_pack_touch(p5general_touch_xy_t* t, uint16_t x, uint16_t y)
{
    t->data[0] = x & 0xFF;
    t->data[1] = ((x >> 8) & 0x0F) | ((y & 0x0F) << 4);
    t->data[2] = (y >> 4) & 0xFF;
}

// ============================================================================
// DEVICE DESCRIPTOR
// ============================================================================
static const tusb_desc_device_t p5general_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = P5GENERAL_VID,
    .idProduct          = P5GENERAL_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01
};

// ============================================================================
// HID REPORT DESCRIPTOR — GP2040 p5general_report_descriptor verbatim (165 B)
// ============================================================================
static const uint8_t p5general_report_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x09, 0x33, 0x09, 0x34,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x06, 0x81, 0x02,
    0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02,
    0x05, 0x01, 0x09, 0x39,
    0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
    0x65, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0E,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0E, 0x81, 0x02,
    0x06, 0x00, 0xFF, 0x09, 0x21, 0x95, 0x0E, 0x81, 0x02,
    0x06, 0x00, 0xFF, 0x09, 0x22, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x34, 0x81, 0x02,
    0x85, 0x02, 0x09, 0x23, 0x95, 0x2F, 0x91, 0x02,
    0x85, 0x03, 0x0A, 0x21, 0x28, 0x95, 0x2F, 0xB1, 0x02,
    0x06, 0x80, 0xFF, 0x85, 0xE0, 0x09, 0x57, 0x95, 0x02, 0xB1, 0x02,
    0xC0,
    0x06, 0xF0, 0xFF, 0x09, 0x40, 0xA1, 0x01,
    0x85, 0xF0, 0x09, 0x47, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0xF1, 0x09, 0x48, 0x95, 0x3F, 0xB1, 0x02,
    0x85, 0xF2, 0x09, 0x49, 0x95, 0x0F, 0xB1, 0x02,
    0xC0,
};
_Static_assert(sizeof(p5general_report_descriptor) == 165, "P5General HID report descriptor must be 165 bytes");

// ============================================================================
// CONFIGURATION DESCRIPTOR — HID w/ IN(0x81)+OUT(0x02) interrupt EPs, joypad-os
// endpoint convention (same as the DualSense mode), not GP2040's 0x82/0x01.
// ============================================================================
#define P5GENERAL_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static const uint8_t p5general_config_descriptor[] = {
    0x09, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(P5GENERAL_CONFIG_TOTAL_LEN),
    0x01, 0x01, 0x00, 0x80, 0xFA,   // 1 iface, bus powered, 500mA

    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,

    0x09, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0x00, 0x01,
    HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(p5general_report_descriptor)),

    0x07, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(P5GENERAL_ENDPOINT_SIZE), 0x01,
    0x07, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(P5GENERAL_ENDPOINT_SIZE), 0x06,
};

// GET feature 0x03 "definition" blob (returned verbatim; GP2040 output_0x03[]).
static const uint8_t p5general_definition_0x03[] = {
    0x21, 0x28, 0x03, 0xC3, 0x00, 0x2C, 0x56,
    0x01, 0x00, 0xD0, 0x07, 0x00, 0x80, 0x04, 0x00,
    0x00, 0x80, 0x0D, 0x0D, 0x84, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#endif // P5GENERAL_DESCRIPTORS_H

// hid_descriptors.h - Generic HID gamepad descriptors
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
// Standard HID gamepad (16 buttons, 4 axes, 2 triggers, dpad)

#ifndef HID_DESCRIPTORS_H
#define HID_DESCRIPTORS_H

#include <stdint.h>

// HID Report Descriptor for Generic Gamepad
// 16 buttons, 4 analog axes (2 sticks), 2 triggers, 1 dpad
static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)

    // Buttons (16 buttons)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x10,        //   Usage Maximum (Button 16)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // D-pad (Hat Switch)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,Null)

    // Padding (4 bits to byte-align)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const,Var,Abs)

    // Left Stick X & Y
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // Right Stick X & Y
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // Triggers (L2, R2)
    0x05, 0x02,        //   Usage Page (Simulation Ctrls)
    0x09, 0xC5,        //   Usage (Brake)
    0x09, 0xC4,        //   Usage (Accelerator)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    0xC0,              // End Collection
};

// HID Report structure (matches descriptor above)
typedef struct __attribute__((packed)) {
    uint16_t buttons;   // 16 buttons (B1-B4, L1-R1, L2-R2, S1-S2, L3-R3, A1-A2)
    uint8_t  hat;       // D-pad (0-7 = directions, 8 = center)
    uint8_t  padding;   // Padding for byte alignment
    uint8_t  lx;        // Left stick X (0-255, 128 = center)
    uint8_t  ly;        // Left stick Y (0-255, 128 = center)
    uint8_t  rx;        // Right stick X (0-255, 128 = center)
    uint8_t  ry;        // Right stick Y (0-255, 128 = center)
    uint8_t  lt;        // Left trigger (0-255)
    uint8_t  rt;        // Right trigger (0-255)
} usbretro_hid_report_t;

// D-pad / Hat Switch values
#define HID_HAT_UP          0
#define HID_HAT_UP_RIGHT    1
#define HID_HAT_RIGHT       2
#define HID_HAT_DOWN_RIGHT  3
#define HID_HAT_DOWN        4
#define HID_HAT_DOWN_LEFT   5
#define HID_HAT_LEFT        6
#define HID_HAT_UP_LEFT     7
#define HID_HAT_CENTER      8

#endif // HID_DESCRIPTORS_H

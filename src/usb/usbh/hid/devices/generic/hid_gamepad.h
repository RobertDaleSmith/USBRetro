// hid_gamepad.h
#ifndef HID_GAMEPAD_HEADER_H
#define HID_GAMEPAD_HEADER_H

#include "../../hid_device.h"
#include "../../hid_utils.h"
#include "tusb.h"

#define INVALID_REPORT_ID -1 // means 1/X of half range of analog would be dead zone
#define DEAD_ZONE 4U
#define MAX_BUTTONS 12 // max generic HID buttons to map
#define HID_DEBUG 1

#define HID_GAMEPAD  0x00
#define HID_MOUSE    0x01
#define HID_KEYBOARD 0x02

typedef union
{
  struct
  {
    bool up : 1;
    bool right : 1;
    bool down : 1;
    bool left : 1;
    bool button1 : 1;
    bool button2 : 1;
    bool button3 : 1;
    bool button4 : 1;

    bool button5 : 1;
    bool button6 : 1;
    bool button7 : 1;
    bool button8 : 1;
    bool button9 : 1;
    bool button10 : 1;
    bool button11 : 1;
    bool button12 : 1;

    // TODO: add support for A1/A2 buttons for dinput/generic HID gamepad input
    // bool button13 : 1;
    // bool button14 : 1;

    uint8_t x, y, z, rz, rx, ry; // analog joystick/triggers
  };
  struct
  {
    uint8_t all_direction : 4;
    uint16_t all_buttons : 12;
    uint32_t analog_sticks : 32;
    uint16_t analog_triggers : 16;
  };
  uint64_t value : 64;
} dinput_gamepad_t;

extern DeviceInterface hid_gamepad_interface;

#endif

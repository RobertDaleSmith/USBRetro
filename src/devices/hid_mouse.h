// hid_mouse.h
#ifndef HID_MOUSE_HEADER_H
#define HID_MOUSE_HEADER_H

#include "device_interface.h"
#include "device_utils.h"
#include "tusb.h"

extern DeviceInterface hid_mouse_interface;

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

// Uncomment the following line if you desire button-swap when middle button is clicd:
// #define MID_BUTTON_SWAPPABLE  true

uint32_t buttons;
extern int16_t spinner;
uint8_t local_x;
uint8_t local_y;

#endif

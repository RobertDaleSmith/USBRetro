// hid_keyboard.h
#ifndef HID_KEYBOARD_HEADER_H
#define HID_KEYBOARD_HEADER_H

#include "device_interface.h"
#include "device_utils.h"
#include "tusb.h"

extern DeviceInterface hid_keyboard_interface;

uint32_t buttons;

#endif

// usbh.h - USB Host Layer
//
// Provides unified USB host handling across HID and X-input protocols.
// Abstracts feedback (rumble, LEDs, triggers) delivery to all USB input devices.

#ifndef USBH_H
#define USBH_H

#include <stdint.h>

// Initialize USB host layer (HID registry, etc.)
void usbh_init(void);

// Process USB host feedback tasks (rumble, LEDs, trigger thresholds)
// Combines console feedback with profile indicator feedback internally
void usbh_task(void);

#endif // USBH_H

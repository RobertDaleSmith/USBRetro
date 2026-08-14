// gamecube_device.h

#ifndef GAMECUBE_DEVICE_H
#define GAMECUBE_DEVICE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "tusb.h"
#include "hardware/pio.h"
#include "lib/joybus-pio/include/gamecube_definitions.h"
#include "core/buttons.h"
#include "core/uart.h"

// Define constants
#undef MAX_PLAYERS
#define MAX_PLAYERS 4
#define GC_KEY_NOT_FOUND 0x00 // Define lookup table (all initialized to NOT_FOUND to start)

#ifndef SHIELD_PIN_L
#define SHIELD_PIN_L 4  // Connector shielding mounted to GPIOs [4, 5,26,27]
#endif

#ifndef SHIELD_PIN_R
#define SHIELD_PIN_R 26
#endif

#ifndef BOOTSEL_PIN
#define BOOTSEL_PIN 11
#endif

#ifndef GC_DATA_PIN
#define GC_DATA_PIN 7
#endif

#ifndef GC_3V3_PIN
#define GC_3V3_PIN 6
#endif

// NGC button modes
#define BUTTON_MODE_0  0x00
#define BUTTON_MODE_1  0x01
#define BUTTON_MODE_2  0x02
#define BUTTON_MODE_3  0x03
#define BUTTON_MODE_4  0x04
#define BUTTON_MODE_KB 0x05

// ============================================================================
// GAMECUBE CONFIGURATION
// ============================================================================
// Button mapping profiles are defined at the app level in:
//   src/apps/usb2gc/profiles.h
//
// To customize button mapping, thresholds, or sensitivity:
// 1. Edit profiles.h in your app to create/modify profiles
// 2. Switch profiles at runtime by holding SELECT for 2 seconds, then D-pad Up/Down
//
// The profile system uses JP_BUTTON_* constants with GameCube-specific aliases
// (GC_BUTTON_A, GC_BUTTON_B, etc.) for readable profile definitions.

// Global variables
extern PIO pio;
extern bool gc_config_mode;  // True when no console was seen on the joybus data
                             // line (CDC config mode). Not the GC 3.3V pin --
                             // detection moved to the data line in a12cc10b and
                             // GC_3V3_PIN has been dead since.

// Function declarations
void ngc_init(void);

// Console presence detection (joybus data line). Call gamecube_console_detect()
// from app_get_output_interfaces() to choose play vs CDC config mode, then call
// gamecube_config_mode_task() from app_task() while gc_config_mode is true so a
// console that appears after boot is picked up without a replug.
uint gamecube_detect_pin(void);
bool gamecube_console_detect(void);
void gamecube_config_mode_task(void);

void __not_in_flash_func(core1_task)(void);
void __not_in_flash_func(update_output)(void);

#endif // GAMECUBE_DEVICE_H

// app.h - JAG2USB App Manifest
// Atari Jaguar controller to USB HID gamepad adapter
//
// This app reads a native Jaguar controller (standard 3-button pad or Pro
// Controller) and outputs a USB HID gamepad. The 12-key keypad is emitted as
// extra SInput gamepad buttons (via input_event.aux_buttons), so every key is
// bindable as a controller button in SDL/Steam/emulators:
//   key 1..9 -> Buttons 15,16,21,22,20,26,27,28,29   (aux 0..8)
//   key 0    -> Button 30   key * -> Button 31   key # -> Button 32
// Hold Pause+Option 2s to toggle Pro Controller mode (kp 7/8/9/4/6 become
// X/Y/Z/L/R gamepad buttons; the remaining 7 keys keep their aux buttons).

#ifndef APP_JAG2USB_H
#define APP_JAG2USB_H

// ============================================================================
// APP METADATA
// ============================================================================
#define APP_NAME "JAG2USB"
#define APP_DESCRIPTION "Atari Jaguar controller to USB HID gamepad adapter"
#define APP_AUTHOR "Robert Dale Smith"

// ============================================================================
// CORE DEPENDENCIES
// ============================================================================

// Output drivers
#define REQUIRE_USB_DEVICE 1
#define USB_OUTPUT_PORTS 1

// Services
#define REQUIRE_PLAYER_MANAGEMENT 1

// ============================================================================
// PIN CONFIGURATION
// ============================================================================
// Jaguar controller pins. J0..J3 are MCU outputs (active-low column selects);
// B0, B1, J8..J11 are MCU inputs with pull-ups. The pad has active logic and
// needs power: feed its VCC pin (DB15 pin 7) from the board's 3.3V — NOT 5V —
// so the return lines stay within the RP2040's non-5V-tolerant input range;
// tie GND (DB15 pin 9, NOT pin 8) to board GND. DB15 columns are pins 1-4 and
// rows are pins 6,10,11,12,13,14 (verified against the raphnet jaguar_usb
// pinout). Columns GP2..GP5, rows GP6..GP10 + J11 on GP26 (GP26 rather than
// GP11 to stay drop-in with the KB2040, which lacks GP11).
#define JAG_PIN_J0   2    // column select
#define JAG_PIN_J1   3
#define JAG_PIN_J2   4
#define JAG_PIN_J3   5
#define JAG_PIN_B0   6    // return line
#define JAG_PIN_B1   7
#define JAG_PIN_J8   8
#define JAG_PIN_J9   9
#define JAG_PIN_J10  10
#define JAG_PIN_J11  26

// ============================================================================
// ROUTING CONFIGURATION
// ============================================================================
#define ROUTING_MODE ROUTING_MODE_MERGE
#define MERGE_MODE MERGE_BLEND

// ============================================================================
// PLAYER MANAGEMENT
// ============================================================================
#define PLAYER_SLOT_MODE PLAYER_SLOT_FIXED
#define MAX_PLAYER_SLOTS 1                  // single pad (Team Tap is future work)
#define AUTO_ASSIGN_ON_PRESS 1

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================
#define BOARD "ada_kb2040"                  // KB2040 default
#define CPU_OVERCLOCK_KHZ 0                 // No overclock needed
#define UART_DEBUG 1

// ============================================================================
// APP INTERFACE
// ============================================================================
void app_init(void);
void app_task(void);

#endif // APP_JAG2USB_H

// chatpad.h - Xbox 360 Chatpad Key Definitions
// SPDX-License-Identifier: MIT

#ifndef CHATPAD_H
#define CHATPAD_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Chatpad Modifier Bits (byte 0 of chatpad data)
// ============================================================================

#define CHATPAD_MOD_SHIFT     0x01  // Shift key (green)
#define CHATPAD_MOD_ORANGE    0x02  // Orange modifier
#define CHATPAD_MOD_MESSENGER 0x04  // Messenger button (people icon)
#define CHATPAD_MOD_CAPSLOCK  0x20  // Caps lock active

// ============================================================================
// Chatpad Key Codes (bytes 1-2 of chatpad data)
// ============================================================================
// Two keys can be pressed simultaneously (key1 and key2)
// Value 0x00 means no key pressed

// Number row
#define CHATPAD_KEY_1         0x17
#define CHATPAD_KEY_2         0x16
#define CHATPAD_KEY_3         0x15
#define CHATPAD_KEY_4         0x14
#define CHATPAD_KEY_5         0x13
#define CHATPAD_KEY_6         0x12
#define CHATPAD_KEY_7         0x11
#define CHATPAD_KEY_8         0x67
#define CHATPAD_KEY_9         0x66
#define CHATPAD_KEY_0         0x65

// Top letter row (QWERTY)
#define CHATPAD_KEY_Q         0x27
#define CHATPAD_KEY_W         0x26
#define CHATPAD_KEY_E         0x25
#define CHATPAD_KEY_R         0x24
#define CHATPAD_KEY_T         0x23
#define CHATPAD_KEY_Y         0x22
#define CHATPAD_KEY_U         0x21
#define CHATPAD_KEY_I         0x76
#define CHATPAD_KEY_O         0x75
#define CHATPAD_KEY_P         0x64

// Middle letter row (ASDF)
#define CHATPAD_KEY_A         0x37
#define CHATPAD_KEY_S         0x36
#define CHATPAD_KEY_D         0x35
#define CHATPAD_KEY_F         0x34
#define CHATPAD_KEY_G         0x33
#define CHATPAD_KEY_H         0x32
#define CHATPAD_KEY_J         0x31
#define CHATPAD_KEY_K         0x77
#define CHATPAD_KEY_L         0x74
#define CHATPAD_KEY_COMMA     0x63  // , (also < with shift)

// Bottom letter row (ZXCV)
#define CHATPAD_KEY_Z         0x46
#define CHATPAD_KEY_X         0x45
#define CHATPAD_KEY_C         0x44
#define CHATPAD_KEY_V         0x43
#define CHATPAD_KEY_B         0x42
#define CHATPAD_KEY_N         0x41
#define CHATPAD_KEY_M         0x52
#define CHATPAD_KEY_PERIOD    0x53  // . (also > with shift)
#define CHATPAD_KEY_ENTER     0x62

// Special keys
#define CHATPAD_KEY_LEFT      0x55  // Left D-pad
#define CHATPAD_KEY_RIGHT     0x51  // Right D-pad
#define CHATPAD_KEY_SPACE     0x54
#define CHATPAD_KEY_BACK      0x71  // Backspace

// ============================================================================
// Helper Functions
// ============================================================================

// Check if a specific key is pressed in chatpad data
static inline bool chatpad_key_pressed(const uint8_t chatpad[3], uint8_t key)
{
    return (chatpad[1] == key) || (chatpad[2] == key);
}

// Check if a modifier is active
static inline bool chatpad_mod_active(const uint8_t chatpad[3], uint8_t mod)
{
    return (chatpad[0] & mod) != 0;
}

// Check if shift modifier is active
static inline bool chatpad_shift(const uint8_t chatpad[3])
{
    return chatpad_mod_active(chatpad, CHATPAD_MOD_SHIFT);
}

// Check if orange modifier is active
static inline bool chatpad_orange(const uint8_t chatpad[3])
{
    return chatpad_mod_active(chatpad, CHATPAD_MOD_ORANGE);
}

// Check if messenger modifier is active
static inline bool chatpad_messenger(const uint8_t chatpad[3])
{
    return chatpad_mod_active(chatpad, CHATPAD_MOD_MESSENGER);
}

// Check if any key (excluding modifiers) is pressed
static inline bool chatpad_any_key(const uint8_t chatpad[3])
{
    return (chatpad[1] != 0) || (chatpad[2] != 0);
}

#endif // CHATPAD_H

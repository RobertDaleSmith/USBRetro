# GameCube Button Mapping Profiles

## Overview

This document describes the button mapping profile system for the GameCube variant of USBRetro firmware. The profile system allows users to switch between different button layouts to accommodate various controller types and user preferences.

## Problem Statement

### Current Mapping Issues

The original single-mapping approach has several limitations:

1. **L1/R1 Mapping Conflict**: Modern controllers have both shoulder buttons (L1/R1) and analog triggers (L2/R2), but the current mapping sends both to GameCube L/R buttons, which doesn't match user expectations.

2. **Z Button Placement**: The GameCube Z button is a shoulder button, but it's currently mapped to Select/Back, which doesn't feel natural for GameCube games where Z is used frequently (grabbing, shielding, etc.).

3. **No Trigger-Only Control**: Users cannot use L2/R2 triggers independently for analog-only input without triggering the digital button press.

4. **Controller Layout Diversity**: Different controller types (Xbox, PlayStation, Switch, 8BitDo) have different button layouts and philosophies that don't align well with a single mapping.

### GameCube Controller Layout

For reference, the original GameCube controller has:
- **Face Buttons**: A (large), B, X, Y
- **D-Pad**: Up, Down, Left, Right
- **Shoulder Buttons**: L (analog + digital), R (analog + digital), Z (digital only)
- **Control Sticks**: Main stick, C-stick
- **System**: Start

The L and R triggers are **analog** (0-255 range) with a **digital click** at the end of travel.

## Proposed Profile System

### Profile Toggle

Users can switch profiles using **Start + D-Pad** button combinations (similar to PCEngine's mode switching):

```
Start + D-Pad Up    → Profile 0: Modern Default
Start + D-Pad Right → Profile 1: Nintendo Style
Start + D-Pad Down  → Profile 2: Classic GameCube
Start + D-Pad Left  → Profile 3: Copilot Mode
```

### Profile Definitions

#### Profile 0: "Modern Default" (Xbox/PlayStation Style)

**Target Controllers**: Xbox (360/One/Series), PlayStation (DS4/DualSense), Generic X-input/D-input

**Philosophy**: Maps face buttons 1:1 with Xbox layout, shoulders control Z, triggers control L/R analog.

| USB Input      | X-input Label | GameCube Output | Notes |
|----------------|---------------|-----------------|-------|
| B1             | A             | GC A            | Primary action |
| B2             | B             | GC B            | Secondary action |
| B3             | X             | GC X            | |
| B4             | Y             | GC Y            | |
| L1             | LB            | GC Z            | Left shoulder → Z |
| R1             | RB            | GC Z            | Right shoulder → Z |
| L2 (analog)    | LT            | GC L (analog)   | Trigger controls L analog + digital |
| R2 (analog)    | RT            | GC R (analog)   | Trigger controls R analog + digital |
| S1             | Back/Select   | GC L+R (light)  | Lightshield (both at threshold ~140) |
| S2             | Start         | GC Start        | |
| D-Pad          | D-Pad         | GC D-Pad        | |
| Left Stick     | LS            | GC Main Stick   | |
| Right Stick    | RS            | GC C-Stick      | |

**Analog Trigger Behavior**:
- L2/R2 analog value (0-255) → GC L/R analog (0-255)
- When analog > 250 → GC L/R digital button pressed
- S1 (Select/Back) → GC L+R both at analog value ~140 (lightshield for wavedashing in Smash)

**Best For**:
- Xbox controllers (360, One, Series X|S)
- PlayStation controllers (DS3, DS4, DualSense)
- Generic X-input controllers
- Users who want Z on shoulders and triggers for analog control

---

#### Profile 1: "Nintendo Style" (Switch Pro/Joy-Con)

**Target Controllers**: Switch Pro Controller, Joy-Con Grip, Nintendo-layout controllers

**Philosophy**: Matches Nintendo's A/B button placement (A on right, B on bottom), shoulders control Z.

| USB Input      | Switch Label  | GameCube Output | Notes |
|----------------|---------------|-----------------|-------|
| B1             | B             | GC A            | Nintendo B → GC A |
| B2             | A             | GC B            | Nintendo A → GC B |
| B3             | Y             | GC Y            | |
| B4             | X             | GC X            | |
| L1             | L             | GC Z            | Left shoulder → Z |
| R1             | R             | GC Z            | Right shoulder → Z |
| L2             | ZL            | GC L (analog)   | Trigger controls L analog + digital |
| R2             | ZR            | GC R (analog)   | Trigger controls R analog + digital |
| S1             | Minus         | GC L+R (light)  | Lightshield |
| S2             | Plus          | GC Start        | |
| D-Pad          | D-Pad         | GC D-Pad        | |
| Left Stick     | LS            | GC Main Stick   | |
| Right Stick    | RS            | GC C-Stick      | |

**Button Label Difference**:
- Nintendo uses A on the right, B on the bottom (opposite of Xbox)
- This profile swaps B1/B2 to match Nintendo philosophy
- Results in more intuitive "A to confirm, B to cancel" for Nintendo users

**Best For**:
- Switch Pro Controller
- Joy-Con in Grip
- 8BitDo controllers in Switch mode
- Users familiar with Nintendo button placement

---

#### Profile 2: "Classic GameCube" (Direct Mapping)

**Target Controllers**: GameCube-style USB controllers (8BitDo, Retro-Bit, etc.), controllers with fewer buttons

**Philosophy**: Direct 1:1 mapping for controllers that match GameCube layout.

| USB Input      | Generic Label | GameCube Output | Notes |
|----------------|---------------|-----------------|-------|
| B1             | A/Button 2    | GC A            | |
| B2             | B/Button 3    | GC B            | |
| B3             | X/Button 1    | GC X            | |
| B4             | Y/Button 4    | GC Y            | |
| L1             | L/Button 5    | GC L (button)   | Triggers digital + full analog (255) |
| R1             | R/Button 6    | GC R (button)   | Triggers digital + full analog (255) |
| L2             | L2/Button 7   | GC Z            | Triggers map to Z |
| R2             | R2/Button 8   | GC Z            | Triggers map to Z |
| S1             | Select/Button 9| GC Z           | Select also maps to Z |
| S2             | Start/Button 10| GC Start       | |
| D-Pad          | D-Pad         | GC D-Pad        | |
| Left Stick     | LS            | GC Main Stick   | |
| Right Stick    | RS            | GC C-Stick      | |

**Analog Trigger Behavior**:
- L1/R1 button press → GC L/R digital button + max analog (255)
- L2/R2 analog value → mapped to GC Z (digital only, no analog passthrough)
- This matches original GameCube controller behavior where L/R have analog + digital

**Best For**:
- 8BitDo GameCube controllers
- Retro-Bit GameCube USB controllers
- Controllers with limited buttons
- Users who want classic GameCube feel

---

#### Profile 3: "Copilot Mode" (Multi-Controller Input)

**Target Use Case**: Accessibility, co-op play, multiple controllers controlling one output

**Philosophy**: Combines inputs from up to 4 USB controllers into a single GameCube output.

**Behavior**:
- All connected controllers' inputs are merged
- Button presses from any controller trigger the corresponding GC button
- Analog sticks use "furthest from center" logic (whichever controller pushes furthest)
- Useful for:
  - Accessibility (one person controls movement, another controls buttons)
  - Co-op play (two players controlling one character)
  - Speedrunning (one hand on each controller)

**Mapping**: Uses Profile 0 (Modern Default) mapping for all controllers

**Best For**:
- Accessibility scenarios
- Cooperative play
- Speedrunning techniques
- Multiple controllers for different functions

---

## Implementation Details

### Button Combo Detection

Profile switching uses the PCEngine pattern:

```c
// Check for profile switch hotkeys (Start + D-Pad)
if (!(players[i].output_buttons & (USBR_BUTTON_S2))) // Start held
{
  if (!(players[i].output_buttons & USBR_BUTTON_DU))      // + Up
    players[i].button_mode = BUTTON_MODE_0; // Modern Default
  else if (!(players[i].output_buttons & USBR_BUTTON_DR)) // + Right
    players[i].button_mode = BUTTON_MODE_1; // Nintendo Style
  else if (!(players[i].output_buttons & USBR_BUTTON_DD)) // + Down
    players[i].button_mode = BUTTON_MODE_2; // Classic GC
  else if (!(players[i].output_buttons & USBR_BUTTON_DL)) // + Left
    players[i].button_mode = BUTTON_MODE_3; // Copilot Mode
}
```

### Button Mode Constants

```c
// NGC button modes (gamecube.h)
#define BUTTON_MODE_0  0x00  // Modern Default
#define BUTTON_MODE_1  0x01  // Nintendo Style
#define BUTTON_MODE_2  0x02  // Classic GameCube
#define BUTTON_MODE_3  0x03  // Copilot Mode
#define BUTTON_MODE_4  0x04  // (Reserved for future use)
#define BUTTON_MODE_KB 0x05  // Keyboard Mode (existing)
```

### Analog Trigger Threshold Logic

**Profile 0 & 1** (Modern/Nintendo):
```c
// L2/R2 analog controls GC L/R analog + digital
players[i].output_analog_l = analog_l; // Direct passthrough (0-255)
players[i].output_analog_r = analog_r;

// Digital button pressed when analog > 250
gc_report.l = (analog_l > 250) ? 1 : 0;
gc_report.r = (analog_r > 250) ? 1 : 0;

// L1/R1 shoulders control Z
gc_report.z |= ((byte & USBR_BUTTON_L1) == 0) ? 1 : 0;
gc_report.z |= ((byte & USBR_BUTTON_R1) == 0) ? 1 : 0;

// S1 (Select) for lightshield (L+R at threshold)
if ((byte & USBR_BUTTON_S1) == 0) {
  gc_report.l_analog = 140;
  gc_report.r_analog = 140;
}
```

**Profile 2** (Classic GC):
```c
// L1/R1 buttons control GC L/R digital + max analog
if ((byte & USBR_BUTTON_L1) == 0) {
  gc_report.l = 1;
  gc_report.l_analog = 255;
}
if ((byte & USBR_BUTTON_R1) == 0) {
  gc_report.r = 1;
  gc_report.r_analog = 255;
}

// L2/R2 control Z
gc_report.z |= ((byte & USBR_BUTTON_L2) == 0) ? 1 : 0;
gc_report.z |= ((byte & USBR_BUTTON_R2) == 0) ? 1 : 0;
```

### LED Feedback

Visual feedback via WS2812 RGB LED (if available):

| Profile | LED Color | Description |
|---------|-----------|-------------|
| 0       | Blue      | Modern Default |
| 1       | Red       | Nintendo Style |
| 2       | Purple    | Classic GameCube |
| 3       | Green     | Copilot Mode |
| KB      | Yellow    | Keyboard Mode (existing) |

```c
// LED color assignments
uint32_t profile_colors[] = {
  0x0000FF, // Profile 0: Blue
  0xFF0000, // Profile 1: Red
  0xFF00FF, // Profile 2: Purple
  0x00FF00, // Profile 3: Green
};
```

## User Guide

### Switching Profiles

1. **Hold Start** on your controller
2. **Press D-Pad direction** while holding Start:
   - **Up**: Modern Default (best for Xbox/PlayStation)
   - **Right**: Nintendo Style (best for Switch Pro)
   - **Down**: Classic GameCube (best for GC-style controllers)
   - **Left**: Copilot Mode (multiple controllers → one output)
3. Release buttons
4. LED will change color to indicate active profile (if equipped)

### Profile Recommendations

**If you have an Xbox controller (360/One/Series):**
- Use **Profile 0** (Modern Default)
- A=A, B=B, X=X, Y=Y
- Shoulders (LB/RB) control Z
- Triggers (LT/RT) control L/R analog

**If you have a Switch Pro Controller or Joy-Con:**
- Use **Profile 1** (Nintendo Style)
- A and B swapped to match Nintendo layout
- Shoulders (L/R) control Z
- Triggers (ZL/ZR) control L/R analog

**If you have a GameCube-style USB controller:**
- Use **Profile 2** (Classic GameCube)
- Direct 1:1 mapping
- L/R buttons control L/R with full analog
- Triggers control Z

**If you're playing with multiple people on one controller:**
- Use **Profile 3** (Copilot Mode)
- Connect multiple controllers via USB hub
- All inputs combine into one output

### Super Smash Bros. Melee Tips

**Wavedashing** (Profile 0 or 1):
- Press **Select/Back** to trigger lightshield (L+R at ~140 analog)
- Jump and press Select/Back simultaneously
- Tilt control stick diagonally

**Shield Dropping** (Profile 0 or 1):
- Full press of **L2** or **R2** for full shield
- Light press for lightshield

**L-Canceling** (All profiles):
- Press **L2** or **R2** (or L/R in Profile 2) just before landing

## Frequently Asked Questions

### Q: Will my profile setting be saved after power cycle?

A: Currently, profiles reset to Profile 0 (Modern Default) on power cycle. Future firmware updates may add EEPROM persistence.

### Q: Can I customize the button mappings?

A: Not without recompiling firmware. However, the 4 profiles should cover most use cases. If you need a custom mapping, you can modify the source code and build custom firmware.

### Q: Why do both L1 and R1 map to Z in Profile 0/1?

A: The GameCube Z button is frequently used (grabbing, shielding, etc.), and having it on both shoulders gives you flexibility to use whichever is more comfortable. You can press either or both.

### Q: What's the analog threshold for the digital L/R click?

A: 250/255 (~98% travel). This matches the original GameCube controller's digital click behavior.

### Q: Does Profile 3 (Copilot) work with different controller types?

A: Yes! You can mix controller types (e.g., Xbox + Switch Pro). Each uses Profile 0 mapping, and inputs are combined.

### Q: Can I use keyboard in these profiles?

A: Keyboard mode is separate. Press **Scroll Lock** or **F14** to toggle keyboard mode (existing functionality). These button profiles only apply to gamepad controllers.

## Technical Notes

### Button Bit Masks (USBRetro Internal)

```c
#define USBR_BUTTON_B1 0x00020 // A/B/Cross
#define USBR_BUTTON_B2 0x00010 // B/A/Circle
#define USBR_BUTTON_B3 0x02000 // X/Y/Square
#define USBR_BUTTON_B4 0x01000 // Y/X/Triangle
#define USBR_BUTTON_L1 0x04000 // LB/L/L1
#define USBR_BUTTON_R1 0x08000 // RB/R/R1
#define USBR_BUTTON_L2 0x00100 // LT/ZL/L2
#define USBR_BUTTON_R2 0x00200 // RT/ZR/R2
#define USBR_BUTTON_S1 0x00040 // Back/Minus/Share
#define USBR_BUTTON_S2 0x00080 // Start/Plus/Options
```

### GameCube Report Structure

```c
typedef struct {
  uint8_t a:1;          // A button (digital)
  uint8_t b:1;          // B button (digital)
  uint8_t x:1;          // X button (digital)
  uint8_t y:1;          // Y button (digital)
  uint8_t start:1;      // Start button (digital)
  uint8_t l:1;          // L digital click
  uint8_t r:1;          // R digital click
  uint8_t z:1;          // Z button (digital)
  uint8_t dpad_up:1;    // D-pad up
  uint8_t dpad_down:1;  // D-pad down
  uint8_t dpad_right:1; // D-pad right
  uint8_t dpad_left:1;  // D-pad left
  uint8_t stick_x;      // Main stick X (0-255, center=128)
  uint8_t stick_y;      // Main stick Y (0-255, center=128)
  uint8_t cstick_x;     // C-stick X (0-255, center=128)
  uint8_t cstick_y;     // C-stick Y (0-255, center=128)
  uint8_t l_analog;     // L analog (0-255)
  uint8_t r_analog;     // R analog (0-255)
} gc_report_t;
```

## Future Enhancements

Potential future additions:

1. **Profile Persistence**: Save last-used profile to flash memory
2. **Per-Controller Profiles**: Each connected controller remembers its own profile
3. **Custom Deadzone Settings**: Adjustable stick deadzones per profile
4. **Trigger Curve Adjustment**: Linear vs. exponential trigger response
5. **Macro Support**: Button combos for advanced techniques
6. **Profile 4**: Additional profile slot for custom community mappings

## Changelog

### v1.0.0 (Proposed)
- Initial profile system design
- 4 profiles: Modern Default, Nintendo Style, Classic GameCube, Copilot Mode
- Start + D-Pad profile switching
- LED feedback for profile indication
- Lightshield support (S1 button in Profile 0/1)

---

**Document Version**: 1.0.0 (Draft)
**Last Updated**: 2025-11-17
**Author**: USBRetro Development Team

# GameCube Joybus Protocol & Keyboard Implementation

**Well-documented Joybus protocol with reverse-engineered keyboard support**

Implemented by Robert Dale Smith (2022-2025)
Based on [joybus-pio](https://github.com/JonnyHaystack/joybus-pio) by JonnyHaystack (rewritten in C)

This document provides comprehensive technical reference for the GameCube Joybus controller protocol, with detailed coverage of the **reverse-engineered keyboard protocol**, profile system, and RP2040 PIO implementation.

---

## Table of Contents

- [Overview](#overview)
- [Physical Layer](#physical-layer)
- [Joybus Protocol Basics](#joybus-protocol-basics)
- [Controller Protocol](#controller-protocol)
- [Keyboard Protocol (Reverse-Engineered)](#keyboard-protocol-reverse-engineered)
- [Profile System](#profile-system)
- [PIO State Machine Implementation](#pio-state-machine-implementation)
- [Timing Requirements](#timing-requirements)
- [Implementation Notes](#implementation-notes)

---

## Overview

The **GameCube Joybus protocol** (also known as "GC-Joybus" or "SI protocol") is a bidirectional serial protocol developed by Nintendo for communication between the GameCube console and its peripherals. The protocol is used for:

- Standard GameCube controllers
- WaveBird wireless controllers (with receiver)
- GameCube Keyboard (for Phantasy Star Online Episode I & II)
- GameCube to GBA link cable
- Other licensed peripherals

### Key Characteristics

- **Single-wire bidirectional**: Data line is tri-state (console or controller drives)
- **250 kbit/s nominal bitrate**: 4µs per bit
- **9-bit byte encoding**: 8 data bits + 1 stop bit
- **Command-response protocol**: Console sends command, controller responds
- **Multiple report modes**: 6 different analog/button configurations
- **Rumble support**: Integrated motor control via stop bit
- **130MHz overclocking required**: RP2040 standard 125MHz insufficient for 4µs timing

### Historical Context

The Joybus protocol was originally developed for the Nintendo 64 and adapted for GameCube with higher bandwidth and more sophisticated features. The keyboard accessory was released only in Japan for Phantasy Star Online Episode I & II, making it a rare peripheral with minimal documentation.

---

## Physical Layer

### Connector Pinout

The GameCube controller port uses a **proprietary 6-pin connector**:

| Pin | Name | Direction | Description |
|-----|------|-----------|-------------|
| 1 | VCC | - | +5V power (from console) |
| 2 | DATA | Bidirectional | Tri-state data line (3.3V logic) |
| 3 | GND | - | Ground |
| 4 | GND | - | Ground (shield) |
| 5 | N/C | - | Not connected |
| 6 | 3.3V | - | +3.3V power (from console, optional) |

**Cable shielding**: The shield is connected to Pin 4 (GND). In the USBRetro implementation, GPIO pins are connected to the cable shield and driven to ground for EMI protection.

### Electrical Characteristics

- **Logic levels**: 3.3V CMOS (0V = LOW, 3.3V = HIGH)
- **Data line**: Open-drain with ~1kΩ pull-up resistor on console side
- **Idle state**: Data line HIGH (pulled up)
- **Drive strength**: Both console and controller can actively drive the line LOW
- **Power**: 5V @ ~200mA max (with rumble active)

### Tri-State Operation

```
Console transmit:    Console drives data line LOW/HIGH
Console receive:     Console releases line, controller drives
Controller transmit: Controller drives data line LOW/HIGH
Controller receive:  Controller releases line (pulled HIGH)
```

**Critical**: Both sides must release the line (go tri-state) when not transmitting to avoid bus contention.

---

## Joybus Protocol Basics

### Bit Encoding

Each bit is transmitted as a **4µs period** with 3 phases:

**Logical 0**:
```
    1µs      2µs      1µs
   ┌─────┐         ┌─────
   │ LOW │   LOW   │ HIGH  (pulled up after release)
───┘     └─────────┘
```

**Logical 1**:
```
    1µs      2µs      1µs
   ┌─────┐─────────┬─────
   │ LOW │  HIGH   │ HIGH
───┘     └─────────┘
```

**Encoding rule**:
- All bits start with **1µs LOW pulse**
- Bit value determines **2µs data period** (LOW = 0, HIGH = 1)
- Followed by **1µs delay** before next bit

**Sampling point**: Receiver samples at **1µs after falling edge** (middle of 2µs data period)

### 9-Bit Byte Encoding

The Joybus protocol uses **9-bit bytes** to support multi-byte messages without inter-byte gaps:

```
Bit 0-7:  Data byte (MSB first)
Bit 8:    Stop bit (0 = continue, 1 = stop)
```

**Example** (3-byte command `0x40 0x03 0x00`):
```
Byte 0: 0x40 | 0 → Continue to next byte (no stop)
Byte 1: 0x03 | 0 → Continue to next byte
Byte 2: 0x00 | 1 → Stop bit (end of command)
```

This allows the PIO state machine to chain bytes efficiently without CPU intervention.

### Command-Response Cycle

```
Console → Controller:  Command (1-3 bytes)
          [4µs reply delay]
Controller → Console:  Response (varies by command)
```

**Reply delay**: Controller must wait 4µs (1 bit period) after receiving command before responding.

### Timing Constraints

- **Command timeout**: 50µs (if no response received, console retries)
- **Inter-byte gap**: None (9-bit encoding allows continuous transmission)
- **Reset timeout**: 130µs (if line stays LOW/HIGH too long, protocol resets)

---

## Controller Protocol

### Command Set

| Command | Hex | Bytes | Description | Response |
|---------|-----|-------|-------------|----------|
| PROBE | 0x00 | 1 | Probe device type | 3 bytes (status) |
| RESET | 0xFF | 1 | Reset device | 3 bytes (status) |
| ORIGIN | 0x41 | 1 | Calibration request | 10 bytes (origin report) |
| RECALIBRATE | 0x42 | 3 | Recalibration | 10 bytes (origin report) |
| POLL | 0x40 | 3 | Poll controller state | 8 bytes (controller report) |
| **KEYBOARD** | **0x54** | **3** | **Poll keyboard state** | **8 bytes (keyboard report)** |
| GAME_ID | 0x1D | 11 | Read game disc ID | (varies) |

### Device Identification

**PROBE / RESET Response** (3 bytes):
```
Byte 0-1: Device type (big-endian uint16_t)
  0x0009 = Standard controller
  0x0900 = WaveBird receiver (no controller paired)
  0x0920 = WaveBird receiver (controller paired)
  0x2008 = Keyboard
  0x0800 = Steering wheel
  0x0200 = Bongos

Byte 2: Status flags
  Bit 0: Rumble motor supported
  Bit 1: Standard controller
  Bit 2-7: Reserved
```

**Example** (standard controller with rumble):
```
0x09 0x00 0x03
  ↑    ↑    ↑
  Type      Status (0x03 = rumble + standard)
```

### Controller Poll Command

**Command**: `0x40 0x03 0x00` (8 bytes requested, rumble stop bit)

**Rumble control via stop bit**:
- `0x40 0x03 | 0` → Rumble OFF (stop bit = 0)
- `0x40 0x03 | 1` → Rumble ON (stop bit = 1)

**Byte 1** (`0x03`): Report mode
- `0x00` = Mode 0 (4-bit triggers, 4-bit analog A/B)
- `0x01` = Mode 1 (4-bit C-stick, full triggers)
- `0x02` = Mode 2 (4-bit C-stick, 4-bit triggers)
- `0x03` = Mode 3 (standard - full 8-bit everything)
- `0x04` = Mode 4 (full C-stick, full analog A/B)

**Byte 2** (`0x00`): Reserved (always 0x00)

### Controller Report Format (Mode 3)

**8 bytes** (default mode used by most games):

```
Byte 0: [A | B | X | Y | Start | Origin | ErrLatch | ErrStat]
Byte 1: [DL | DR | DD | DU | Z | R | L | High1]
Byte 2: Left stick X (0x00 = left, 0x80 = center, 0xFF = right)
Byte 3: Left stick Y (0x00 = down, 0x80 = center, 0xFF = up)
Byte 4: C-stick X (same range)
Byte 5: C-stick Y (same range)
Byte 6: L analog (0x00 = released, 0xFF = fully pressed)
Byte 7: R analog (0x00 = released, 0xFF = fully pressed)
```

**Bit fields**:
- All buttons: Active HIGH (1 = pressed)
- `Origin` bit: LOW after console sends ORIGIN command
- `High1` bit: Always set to 1 (protocol marker)

### Origin Calibration

**ORIGIN Command** (`0x41`):
Requests the controller's neutral position (calibration data).

**Response** (10 bytes):
```
Bytes 0-7: Current controller state (same as POLL response)
Bytes 8-9: Reserved (0x00)
```

The console uses this to establish the controller's center position for analog sticks and triggers. Games typically request this on boot or when the controller is first connected.

---

## Keyboard Protocol (Reverse-Engineered)

### Discovery

The GameCube keyboard was released exclusively in Japan for **Phantasy Star Online Episode I & II**. The protocol was completely undocumented, requiring hardware analysis and iterative testing to decode.

**Key discovery**: The keyboard uses command `0x54` (not documented anywhere publicly) and returns an 8-byte report with **3 simultaneous keypresses** and a **rolling counter with XOR checksum**.

### Keyboard Poll Command

**Command**: `0x54 0x00 0x00` (8 bytes requested)

Unlike the controller, the keyboard does **not** support rumble, so the stop bit has no effect.

### Keyboard Report Format

**8 bytes**:

```
Byte 0: [Counter(4 bits) | Unknown(2 bits) | ErrLatch | ErrStat]
Byte 1: Unknown
Byte 2: Unknown
Byte 3: Unknown
Byte 4: Keypress 1 (GameCube keycode)
Byte 5: Keypress 2 (GameCube keycode)
Byte 6: Keypress 3 (GameCube keycode)
Byte 7: Checksum (keypress[0] ^ keypress[1] ^ keypress[2] ^ counter)
```

**Counter**: 4-bit value (0-15) that increments with each report. Used in checksum calculation.

**Checksum algorithm**:
```c
uint8_t checksum = keypress[0] ^ keypress[1] ^ keypress[2] ^ counter;
```

This prevents data corruption and validates that the report is intact.

### Keyboard Keycodes

The GameCube keyboard uses **proprietary keycodes** (0x00-0x61), different from USB HID:

#### Control Keys

| Keycode | Name | Description |
|---------|------|-------------|
| 0x00 | NONE | No key pressed |
| 0x06 | HOME | Home (Fn + Up) |
| 0x07 | END | End (Fn + Right) |
| 0x08 | PAGEUP | Page Up (Fn + Left) |
| 0x09 | PAGEDOWN | Page Down (Fn + Down) |
| 0x0A | SCROLLLOCK | Scroll Lock (Fn + Insert) |

#### Alphanumeric Keys

| Range | Keys |
|-------|------|
| 0x10-0x29 | A-Z |
| 0x2A-0x33 | 0-9 |

#### Function Keys

| Range | Keys |
|-------|------|
| 0x40-0x4B | F1-F12 |

#### Special Keys

| Keycode | Name | PSO Mapping (Normal / Shift) |
|---------|------|------------------------------|
| 0x34 | MINUS | - / = |
| 0x35 | CARET | ^ / ~ |
| 0x36 | YEN | \ / \| |
| 0x37 | AT | @ / ` |
| 0x38 | LEFTBRACKET | [ / { |
| 0x39 | SEMICOLON | ; / + |
| 0x3A | COLON | : / * |
| 0x3B | RIGHTBRACKET | ] / } |
| 0x3C | COMMA | , / < |
| 0x3D | PERIOD | . / > |
| 0x3E | SLASH | / / ? |
| 0x3F | BACKSLASH | \ / _ |

#### System Keys

| Keycode | Name |
|---------|------|
| 0x4C | ESC |
| 0x4D | INSERT |
| 0x4E | DELETE |
| 0x4F | GRAVE |
| 0x50 | BACKSPACE |
| 0x51 | TAB |
| 0x53 | CAPSLOCK |
| 0x54 | LEFTSHIFT |
| 0x55 | RIGHTSHIFT |
| 0x56 | LEFTCTRL |
| 0x57 | LEFTALT |
| 0x59 | SPACE |
| 0x61 | ENTER |

#### Arrow Keys

| Keycode | Name |
|---------|------|
| 0x5C | LEFT |
| 0x5D | DOWN |
| 0x5E | UP |
| 0x5F | RIGHT |

#### Japanese Layout Keys

| Keycode | Name | USB HID Mapping |
|---------|------|-----------------|
| 0x58 | LEFTUNK1 | GUI_LEFT (Muhenkan) |
| 0x5A | RIGHTUNK1 | GUI_RIGHT (Henkan/Zenkouho) |
| 0x5B | RIGHTUNK2 | APPLICATION (Hiragana/Katakana) |

These keys are specific to Japanese keyboards and may not have direct equivalents on Western keyboards.

### HID to GameCube Keycode Mapping

The USBRetro implementation provides a complete lookup table (`hid_to_gc_key[256]`) to translate USB HID keycodes to GameCube keycodes:

```c
uint8_t hid_to_gc_key[256] = {[0 ... 255] = GC_KEY_NOT_FOUND};

// Example mappings
hid_to_gc_key[HID_KEY_A] = GC_KEY_A;              // 0x04 → 0x10
hid_to_gc_key[HID_KEY_1] = GC_KEY_1;              // 0x1E → 0x2A
hid_to_gc_key[HID_KEY_F1] = GC_KEY_F1;            // 0x3A → 0x40
hid_to_gc_key[HID_KEY_SPACE] = GC_KEY_SPACE;      // 0x2C → 0x59
hid_to_gc_key[HID_KEY_ENTER] = GC_KEY_ENTER;      // 0x28 → 0x61
hid_to_gc_key[HID_KEY_EQUAL] = GC_KEY_CARET;      // 0x2E → 0x35
hid_to_gc_key[HID_KEY_GRAVE] = GC_KEY_YEN;        // 0x35 → 0x36
hid_to_gc_key[HID_KEY_PRINT_SCREEN] = GC_KEY_AT;  // 0x46 → 0x37
```

### Keyboard Mode Switching

Users can toggle between controller and keyboard modes using **Scroll Lock** or **F14**:

```c
if (keypress[0] == HID_KEY_SCROLL_LOCK || keypress[0] == HID_KEY_F14)
{
    if (players[0].button_mode != BUTTON_MODE_KB)
    {
        // Switch to keyboard mode
        players[0].button_mode = BUTTON_MODE_KB;
        GamecubeConsole_SetMode(&gc, GamecubeMode_KB);
        default_gc_status.device = GamecubeDevice_KEYBOARD;  // 0x2008
        gc_kb_led = 0x4;  // Turn on keyboard LED indicator
    }
    else
    {
        // Switch back to controller mode
        players[0].button_mode = BUTTON_MODE_3;
        GamecubeConsole_SetMode(&gc, GamecubeMode_3);
        default_gc_status.device = GamecubeDevice_CONTROLLER;  // 0x0009
        gc_kb_led = 0;
    }
}
```

When in keyboard mode, the console sends `0x54` commands instead of `0x40` poll commands.

### Phantasy Star Online Key Mappings

The keyboard was designed for PSO Episode I & II. Here are the in-game key mappings:

| Function | Key | Keycode |
|----------|-----|---------|
| Move forward | W | 0x26 |
| Move backward | S | 0x22 |
| Strafe left | A | 0x10 |
| Strafe right | D | 0x13 |
| Jump | Space | 0x59 |
| Action | Enter | 0x61 |
| Menu | ESC | 0x4C |
| Chat | / | 0x3E |
| Inventory | I | 0x18 |
| Map | M | 0x1C |

---

## Profile System

### Overview

The USBRetro GameCube implementation features a **sophisticated profile system** with flash-backed persistence, allowing users to switch between preconfigured button mappings and trigger behaviors optimized for specific games.

### Available Profiles

**1. Default** - Standard GameCube mapping
```
Face: B/A/Y/X → B/A/Y/X
L1→Nothing, R1→Z
LT/RT→L/R with threshold + analog passthrough
```

**2. SNES** - Original SNES controller mapping
```
Face: B/A/Y/X → B/A/Y/X
L1→L(full), R1→R(full)
S1(Select)→Z
```

**3. SSBM** - Super Smash Bros Melee competitive (Yoink1975's config)
```
L1(LB)→Z (grab)
R1(RB)→X (jump)
LT→L digital at 88% + L analog at 43 (17% light shield)
RT→L+R digital at 55% (quit combo)
Left stick: 85% sensitivity (precision)
```

**4. MKWii** - Mario Kart Wii drift mapping (Eggzact123's config)
```
L1(LB)→D-pad Up (menu navigation)
R1(RB)→R(full) (acceleration)
RT→Z at 10% threshold (instant drift)
```

**5. Fighting** - Fighting game mapping
```
L1→C-stick Up (quick smash)
Right stick: 0% sensitivity (disabled for in-game config)
```

### Profile Configuration Structure

Each profile defines:

```c
typedef struct {
    const char* name;                       // "ssbm"
    const char* description;                // "SSBM: LB→Z, LT→Light(43)..."

    // Trigger thresholds (0-255)
    uint8_t l2_threshold;                   // LT digital threshold
    uint8_t r2_threshold;                   // RT digital threshold

    // Custom trigger analog values
    uint8_t l2_analog_value;                // Custom L analog (0 = passthrough)
    uint8_t r2_analog_value;                // Custom R analog

    // Stick sensitivity (0.0-1.0)
    float left_stick_sensitivity;           // Left stick multiplier
    float right_stick_sensitivity;          // Right stick (or disable)

    // Button mappings (12 buttons)
    gc_button_output_t b1_button;           // B1 → GC output
    gc_button_output_t b2_button;           // B2 → GC output
    // ... (b3, b4, l1, r1, s1, s2, l3, r3, a1, a2)

    // Trigger behaviors
    gc_trigger_behavior_t l2_behavior;      // LT trigger mode
    gc_trigger_behavior_t r2_behavior;      // RT trigger mode
} gc_profile_t;
```

### Trigger Behaviors

```c
typedef enum {
    GC_TRIGGER_NONE,            // No action
    GC_TRIGGER_L_THRESHOLD,     // L digital at threshold + analog passthrough
    GC_TRIGGER_R_THRESHOLD,     // R digital at threshold + analog passthrough
    GC_TRIGGER_L_FULL,          // L digital + L analog forced to 255
    GC_TRIGGER_R_FULL,          // R digital + R analog forced to 255
    GC_TRIGGER_Z_INSTANT,       // Z button (uses trigger threshold)
    GC_TRIGGER_L_CUSTOM,        // L digital + custom analog value
    GC_TRIGGER_R_CUSTOM,        // R digital + custom analog value
    GC_TRIGGER_LR_BOTH,         // L+R digital (SSBM quit combo)
} gc_trigger_behavior_t;
```

### Profile Switching

**Method 1: Runtime hotkey**
- Hold **SELECT for 2 seconds**
- Press **D-pad Up** to cycle forward
- Press **D-pad Down** to cycle backward
- Multi-modal feedback: NeoPixel LED blinks, rumble pulses, player LEDs light up

**Method 2: Flash default**
- Edit `GC_DEFAULT_PROFILE_INDEX` in `gamecube_config.h`
- Rebuild firmware

### Flash Persistence

Profiles are saved to RP2040 flash memory with wear-leveling:

```c
typedef struct {
    uint8_t active_profile_index;  // Currently selected profile (0-4)
} flash_settings_t;
```

**Flash write strategy**:
- **Debounced writes**: 5-second delay after profile switch before writing to flash
- **Read on boot**: Restores last selected profile
- **Validation**: Checks profile index is valid before restoring
- **Wear leveling**: Minimizes flash writes to extend flash life

**Flash safety**:
- Core 1 must be paused during flash writes (100ms operation)
- Uses `flash_safe_execute()` wrapper to coordinate cores

### Multi-Modal Feedback

When switching profiles, the system provides feedback via multiple channels:

**1. NeoPixel LED** (WS2812):
- Blinks N times for profile N (1-5 blinks)
- Prevents accidental rapid switching (blocks while indicating)

**2. Rumble Motor**:
- Pulses N times for profile N
- Haptic confirmation for the user

**3. Player LEDs**:
- Port LEDs light up corresponding to profile number
- Visual feedback on which profile is active

**4. UART Debug**:
```c
printf("Profile switched to: %s (%s)\n",
       active_profile->name,
       active_profile->description);
```

---

## PIO State Machine Implementation

### PIO Program Overview

The joybus protocol is implemented as a single PIO program with two entry points:

**Entry Point 1: `read`** - Receive bytes from console
**Entry Point 2: `write`** - Transmit bytes to console

### Timing Parameters

```asm
.define public T1 10    ; 1µs LOW pulse (10 cycles @ 10MHz PIO clock)
.define public T2 20    ; 2µs data period (20 cycles)
.define public T3 10    ; 1µs delay (10 cycles)
```

**Total**: T1 + T2 + T3 = 40 cycles per bit

**PIO clock frequency**:
```c
int cycles_per_bit = joybus_T1 + joybus_T2 + joybus_T3;  // 40
int bitrate = 250000;  // 250 kbit/s
float div = clock_get_hz(clk_sys) / (cycles_per_bit * bitrate);
// At 130 MHz: div = 130000000 / (40 * 250000) = 13.0
```

### Read Entry Point

```asm
public read:
    set pindirs 0                      ; Set pin to input (tri-state)
read_loop:
    wait 0 pin 0 [T1 + T2 / 2 - 1]     ; Wait for falling edge + 1.5µs
    in pins, 1                         ; Sample bit (at midpoint of T2)
    wait 1 pin 0                       ; Wait for line to go HIGH again
    jmp read_loop                      ; Continue reading
```

**Operation**:
1. Configure data pin as input (release bus)
2. Wait for **falling edge** (start of bit)
3. Delay **1.5µs** (T1 + T2/2) to reach sampling point
4. Sample bit value
5. Wait for line to return HIGH (end of bit)
6. Loop for next bit

**Autopush**: ISR configured with 8-bit threshold → auto-pushes to RX FIFO after 8 bits.

### Write Entry Point

```asm
public write:
    set pindirs 1           ; Set pin to output
write_loop:
    set pins, 1             ; Set line HIGH (end previous pulse)
    pull ifempty block      ; Fetch next byte from TX FIFO
    out x, 1                ; Extract bit 8 (stop bit)
    jmp !osre write_bit     ; If bits 0-7 remain, write next bit
    jmp x!=y write_stop_bit ; If bit 8 == 1, this is a stop bit
    pull ifempty block      ; Bit 8 == 0, fetch next byte (no stop)
    out x, 1                ; Extract first bit of new byte
    jmp write_bit_fast      ; Write it (skip delays - already spent time)
write_bit:
    nop [3]                 ; Padding delay
write_bit_fast:
    nop [T3 - 9]            ; 1µs delay minus overhead
    set pins, 0 [T1 - 1]    ; 1µs LOW pulse
    mov pins, x [T2 - 2]    ; 2µs data period (bit value)
    jmp write_loop          ; Continue
write_stop_bit:
    nop [T3 - 6]            ; Delay (adjusted for overhead)
    set pins, 0 [T1 - 1]    ; 1µs LOW pulse
    set pins, 1 [T2 - 2]    ; 2µs HIGH (stop bit)
    jmp read                ; Switch to read mode after stop bit
```

**9-Bit Byte Encoding**:
- TX FIFO contains **32-bit words**
- Byte shifted into OSR with MSB first
- **Bit 8** = stop bit (0 = continue, 1 = stop)
- Bits 0-7 = data byte

**Example** (send `0x40` with stop):
```c
uint32_t data_shifted = (0x40 << 24) | (1 << 23);  // Byte in bits 31-24, stop in bit 23
pio_sm_put_blocking(pio, sm, data_shifted);
```

**Key optimization**: On stop bit = 0, the PIO immediately pulls the next byte and extracts its first bit, then uses `write_bit_fast` to skip the normal delay overhead (since time was already spent checking the stop bit).

### State Machine Configuration

```c
// Output shift: Left shift, no autopull, 9-bit threshold
sm_config_set_out_shift(&c, false, false, 9);

// Input shift: Left shift, autopush, 8-bit threshold
sm_config_set_in_shift(&c, false, true, 8);

// Clock divider: 130 MHz / (40 * 250 kHz) = 13.0
float div = clock_get_hz(clk_sys) / (cycles_per_bit * bitrate);
sm_config_set_clkdiv(&c, div);
```

**FIFO joining**: No FIFO joining (TX and RX FIFOs independent).

---

## Timing Requirements

### Critical Timing Constraints

**Joybus bit period**: 4µs (250 kbit/s)
- T1: 1µs LOW pulse
- T2: 2µs data period
- T3: 1µs delay

**Console timing tolerance**: ±0.5µs per bit (measured)

**RP2040 clock accuracy**:
- At 130 MHz with div=13.0 → PIO runs at 10 MHz → 0.1µs per cycle
- Jitter: <0.1µs (negligible)

### 130MHz Overclocking Requirement

**Standard RP2040**: 125 MHz (default)
**Required for joybus**: 130 MHz

**Calculation**:
```
125 MHz / (40 cycles * 250 kbit/s) = 12.5 (clock divider)
130 MHz / (40 cycles * 250 kbit/s) = 13.0 (clock divider)
```

With div=12.5, the timing is slightly off and causes intermittent communication errors. **130 MHz is the minimum** for reliable operation.

**Initialization**:
```c
void ngc_init()
{
    set_sys_clock_khz(130000, true);  // Overclock to 130 MHz
    stdio_init_all();                 // Reinitialize UART after clock change
}
```

**Safety**: The RP2040 is rated for up to 133 MHz overclocking, so 130 MHz is within safe limits with adequate cooling.

### Reply Delay

**Controller must wait 4µs** (1 bit period) after receiving a command before replying:

```c
#define gc_reply_delay (gc_incoming_bit_length_us - 1)  // 5µs - 1 = 4µs
busy_wait_us(gc_reply_delay);
joybus_send_bytes(&console->_port, (uint8_t *)&report, sizeof(report));
```

This prevents bus contention between console and controller.

### Timeout Values

```c
#define gc_incoming_bit_length_us 5  // 5µs per bit (includes margins)
#define gc_receive_timeout_us (gc_incoming_bit_length_us * 10)  // 50µs
#define gc_reset_wait_period_us 130  // 130µs reset timeout
```

**Timeout handling**:
- If no byte received within 50µs → command timed out
- If line held LOW/HIGH for >130µs → protocol error, reset

---

## Implementation Notes

### Dual-Core Architecture

**Core 0** (main):
- Runs USB polling loop (`tuh_task()`)
- Updates `players[].global_buttons` and analog values
- Calls `post_globals()` on USB input events
- Builds `gc_report` structure

**Core 1** (joybus):
- Dedicated to GameCube console communication
- Blocks on `GamecubeConsole_WaitForPoll()`
- Reads `gc_report` and sends to console
- Handles rumble feedback

**No mutexes needed**: `gc_report` is written atomically (single struct assignment), Core 1 reads it without locking.

### Digital-Only Trigger Support

Modern controllers have different trigger types:
- **Analog triggers**: Xbox, PlayStation (send both digital bit + analog value)
- **Digital-only triggers**: Switch Pro, PS3 (send only digital bit, analog = 0)

**Solution**: Convert digital button press to full analog value:

```c
// For digital-only controllers: convert button press to full analog
if (analog_l == 0 && (buttons & USBR_BUTTON_L2) == 0) {
    players[player_index].output_analog_l = 255;  // Treat as full press
}
if (analog_r == 0 && (buttons & USBR_BUTTON_R2) == 0) {
    players[player_index].output_analog_r = 255;
}
```

This ensures Switch Pro and PS3 controllers work correctly on GameCube.

### Profile-Based Trigger Thresholds

USB controllers send analog triggers starting at 0%. Modern controllers' firmware sets the **digital button bit at ~1-5% trigger travel**. This is too sensitive for GameCube games.

**Solution**: Use profile-based thresholds and **ignore** the controller's digital bit:

```c
// Save original digital button state
bool original_l2_pressed = (buttons & USBR_BUTTON_L2) == 0;

// Force L2/R2 to "not pressed" initially
players[player_index].output_buttons |= (USBR_BUTTON_L2 | USBR_BUTTON_R2);

// LT: Use profile threshold if analog present, else fall back to digital
if (analog_l > active_profile->l2_threshold ||
    (analog_l == 0 && original_l2_pressed))
{
    players[player_index].output_buttons &= ~USBR_BUTTON_L2;
}
```

**Result**: Users can configure per-profile thresholds (e.g., SSBM profile uses 88% threshold for L, 55% for R).

### SSBM Light Shield Implementation

Super Smash Bros Melee uses **light shielding** (pressing L/R lightly without clicking):

**Profile configuration**:
```c
.l2_threshold = 225,         // L digital triggers at 88%
.l2_analog_value = 43,       // L analog forced to 43 (~17%)
.l2_behavior = GC_TRIGGER_L_CUSTOM,  // Use custom analog value
```

**Implementation**:
```c
case GC_TRIGGER_L_CUSTOM:
    // Force L analog to custom value (light shield)
    if (active_profile->l2_analog_value > 0 &&
        new_report.l_analog < active_profile->l2_analog_value) {
        new_report.l_analog = active_profile->l2_analog_value;
    }
    // Set L digital at threshold
    if (l2_pressed) new_report.l = 1;
    break;
```

**Result**: LT at <88% = 17% light shield (no click). LT at 88%+ = light shield + L digital (click). Perfect for powershielding!

### SSBM Quit Combo (L+R)

The SSBM profile maps RT to L+R digital buttons for the pause menu quit combo:

```c
case GC_TRIGGER_LR_BOTH:
    if (r2_pressed) {
        new_report.l = 1;  // Set both L...
        new_report.r = 1;  // ...and R digital
    }
    break;
```

**Usage**: Press RT at 55% threshold → Instantly triggers L+R quit combo in SSBM.

### Stick Sensitivity Scaling

Profiles can scale analog stick range:

```c
static inline uint8_t scale_toward_center(uint8_t val, float scale, uint8_t center)
{
    int16_t rel = (int16_t)val - (int16_t)center;  // Relative to center
    int16_t scaled = (int16_t)(rel * scale);        // Scale
    int16_t result = scaled + (int16_t)center;      // Back to absolute
    if (result < 0) result = 0;                     // Clamp
    if (result > 255) result = 255;
    return (uint8_t)result;
}
```

**SSBM profile**: 85% left stick sensitivity reduces range for precision movement.
**Fighting profile**: 0% right stick sensitivity disables C-stick (allows in-game button config).

### Mouse Delta Accumulation

USB mice report at 125-1000Hz, GameCube polls at ~60Hz. To avoid jitter:

```c
// Accumulate high-frequency USB mouse reports
if (delta_x >= 128)
    players[player_index].global_x -= (256-delta_x);
else
    players[player_index].global_x += delta_x;

// Clamp to ±127 range
if (players[player_index].global_x > 127)
    delta_x = 0xff;
else if (players[player_index].global_x < -127)
    delta_x = 1;
else
    delta_x = 128 + players[player_index].global_x;
```

Smooths mouse input for games that support controller as mouse (e.g., homebrew).

### Safety Features

**1. GC 3.3V Detection**: Reboot to bootsel if GameCube not connected
```c
gpio_init(GC_3V3_PIN);
gpio_set_dir(GC_3V3_PIN, GPIO_IN);
gpio_pull_down(GC_3V3_PIN);
sleep_ms(200);
if (!gpio_get(GC_3V3_PIN)) reset_usb_boot(0, 0);  // No GameCube → bootsel mode
```

**2. Shield Pin Grounding**: EMI protection
```c
gpio_init(SHIELD_PIN_L);  // GPIO 4, 5 connected to shield
gpio_set_dir(SHIELD_PIN_L, GPIO_OUT);
gpio_put(SHIELD_PIN_L, 0);  // Drive shield to ground
```

**3. BOOTSEL Button**: Hardware firmware update button
```c
gpio_init(BOOTSEL_PIN);  // GPIO 11
gpio_set_dir(BOOTSEL_PIN, GPIO_IN);
gpio_pull_up(BOOTSEL_PIN);
```

---

## Quick Reference

### Command Summary

| Command | Hex | Description |
|---------|-----|-------------|
| PROBE | 0x00 | Probe device type → 3 bytes (status) |
| RESET | 0xFF | Reset device → 3 bytes (status) |
| ORIGIN | 0x41 | Calibration request → 10 bytes |
| RECALIBRATE | 0x42 | Recalibration → 10 bytes |
| POLL | 0x40 | Poll controller → 8 bytes |
| **KEYBOARD** | **0x54** | **Poll keyboard → 8 bytes** |

### Device Types

| Value | Device |
|-------|--------|
| 0x0009 | Standard controller |
| 0x0900 | WaveBird (no pairing) |
| 0x0920 | WaveBird (paired) |
| **0x2008** | **Keyboard** |
| 0x0800 | Steering wheel |
| 0x0200 | Bongos |

### Timing Summary

| Parameter | Value |
|-----------|-------|
| Bit period | 4µs |
| T1 (LOW pulse) | 1µs |
| T2 (data period) | 2µs |
| T3 (delay) | 1µs |
| Reply delay | 4µs |
| Receive timeout | 50µs |
| Reset timeout | 130µs |
| Required CPU clock | 130 MHz |

### Pin Assignments (Default KB2040)

| Function | GPIO | Description |
|----------|------|-------------|
| GC_DATA_PIN | 7 | Joybus data line |
| GC_3V3_PIN | 6 | 3.3V detection |
| SHIELD_PIN_L | 4, 5 | Shield ground |
| SHIELD_PIN_R | 26, 27 | Shield ground |
| BOOTSEL_PIN | 11 | Firmware update button |

---

## Acknowledgments

- **JonnyHaystack** - [joybus-pio](https://github.com/JonnyHaystack/joybus-pio) library (rewritten in C)
- **Nintendo** - Original Joybus protocol design
- **Yoink1975** - SSBM profile configuration
- **Eggzact123** - Mario Kart Wii profile configuration
- **Phantasy Star Online community** - Keyboard protocol hints

---

## References

- [N64brew Joybus Documentation](https://n64brew.dev/wiki/Joybus_Protocol)
- [Dolphin Emulator SI Documentation](https://github.com/dolphin-emu/dolphin)
- [GameCube Controller Protocol (Wiibrew)](https://wiibrew.org/wiki/GameCube_controller_protocol)
- [joybus-pio Library](https://github.com/JonnyHaystack/joybus-pio)

---

*The GameCube keyboard protocol documented here represents original reverse-engineering work. The 0x54 keyboard command and 8-byte report structure with XOR checksum were discovered through hardware analysis and testing with Phantasy Star Online Episode I & II.*

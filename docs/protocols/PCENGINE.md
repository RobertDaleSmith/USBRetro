# PCEngine / TurboGrafx-16 Controller Protocol

**Well-documented protocol with novel multitap and mouse implementation**

Implemented by Robert Dale Smith (2022-2023)
Based on foundational work by David Shadoff ([PCEMouse](https://github.com/dshadoff/PC_Engine_RP2040_Projects/tree/main/PCEMouse))

This document provides a comprehensive technical reference for the PCEngine/TurboGrafx-16 controller protocol, with emphasis on the multitap scanning mechanism, mouse support, and RP2040 PIO implementation strategies.

---

## Table of Contents

- [Overview](#overview)
- [Physical Layer](#physical-layer)
- [Protocol Basics](#protocol-basics)
- [2-Button Mode](#2-button-mode)
- [6-Button Mode](#6-button-mode)
- [3-Button Modes](#3-button-modes)
- [Mouse Protocol](#mouse-protocol)
- [Multitap Scanning](#multitap-scanning)
- [PIO State Machine Architecture](#pio-state-machine-architecture)
- [Timing Requirements](#timing-requirements)
- [Implementation Notes](#implementation-notes)

---

## Overview

The **PCEngine controller protocol** (also known as TurboGrafx-16 in North America) is a parallel 4-bit interface developed by NEC and Hudson Soft. The protocol is elegant in its simplicity yet flexible enough to support:

- Standard 2-button controllers
- 6-button fighting game pads
- Mouse input devices
- Multitap for up to 5 simultaneous players

### Key Characteristics

- **Parallel interface**: 4-bit data bus (D0-D3)
- **Active LOW encoding**: 0 = pressed, 1 = released
- **Scan-based**: Console controls timing via SEL and CLR signals
- **Nibble-multiplexed**: D-pad and buttons sent as separate 4-bit nibbles
- **Multitap support**: Up to 5 players via time-division multiplexing
- **Extensible**: 6-button mode adds extended button nibbles

### Historical Context

The PCEngine was released in 1987 (Japan) and 1989 (North America as TurboGrafx-16). The controller protocol was designed for simplicity and low cost, using common 74-series logic chips in the controller hardware. The multitap accessory enabled 5-player Bomberman, which became a defining feature of the platform.

---

## Physical Layer

### Connector Pinout

The PCEngine controller port uses an **8-pin DIN connector**:

| Pin | Name | Direction | Description |
|-----|------|-----------|-------------|
| 1 | VCC | - | +5V power supply |
| 2 | D0 | Output (from controller) | Data bit 0 |
| 3 | D1 | Output | Data bit 1 |
| 4 | D2 | Output | Data bit 2 |
| 5 | D3 | Output | Data bit 3 |
| 6 | SEL | Input (to controller) | Select signal |
| 7 | CLR | Input (to controller) | Clear/Enable signal |
| 8 | GND | - | Ground |

### Electrical Characteristics

- **Logic levels**: TTL compatible (0V = LOW, 5V = HIGH)
- **Data lines**: Open-collector with 1kΩ pull-up resistors on console side
- **Control signals**: Driven by console, interpreted by controller
- **Power**: 5V @ ~50mA per controller (no rumble in original hardware)

### Signal Behavior

```
CLR:  ────┐         ┌─────────────────┐
          └─────────┘                 └────  (LOW = scan active, HIGH = idle)

SEL:  ────┐   ┌───┐   ┌───┐   ┌───┐   ┌──  (toggles between nibbles)
          └───┘   └───┘   └───┘   └───┘

D0-3: ──[D-PAD]─[BTNS]─[D-PAD]─[BTNS]────  (responds to SEL transitions)
```

**Scan Sequence**:
1. CLR goes LOW → Start of scan cycle
2. SEL alternates HIGH/LOW → Controller outputs different nibbles
3. CLR goes HIGH → End of scan, controller resets

---

## Protocol Basics

### Nibble Encoding (Active LOW)

All data is transmitted as 4-bit nibbles with **active LOW** encoding:

- **0** (LOW) = Button/Direction **pressed**
- **1** (HIGH) = Button/Direction **released**
- **0xF** (all HIGH) = No input (default/idle state)

### Standard 2-Button Controller Data

Each scan reads **2 nibbles** (8 bits total):

**Nibble 1 (SEL=HIGH)**: D-Pad
```
Bit 3: Left  (0=pressed)
Bit 2: Down  (0=pressed)
Bit 1: Right (0=pressed)
Bit 0: Up    (0=pressed)
```

**Nibble 2 (SEL=LOW)**: Buttons
```
Bit 3: Run    (0=pressed)
Bit 2: Select (0=pressed)
Bit 1: II     (0=pressed)
Bit 0: I      (0=pressed)
```

### Example Encoding

**No buttons pressed**:
```
Nibble 1: 0xF (1111) - No directions
Nibble 2: 0xF (1111) - No buttons
Full byte: 0xFF
```

**Up + I button pressed**:
```
Nibble 1: 0xE (1110) - Up pressed (bit 0 = 0)
Nibble 2: 0xE (1110) - I pressed (bit 0 = 0)
Full byte: 0xEE
```

**Left + Down + II + Run pressed**:
```
Nibble 1: 0x3 (0011) - Left (bit 3=0) + Down (bit 2=0)
Nibble 2: 0x5 (0101) - Run (bit 3=0) + II (bit 1=0)
Full byte: 0x35
```

---

## 2-Button Mode

The standard PCEngine controller mode used by most games.

### Byte Format

```
Bit Position:  7     6      5      4    |  3    2       1   0
              Left  Down  Right   Up   | Run  Select   II  I
```

### State Machine

1. **CLR LOW**: Start scan
2. **SEL HIGH**: Read D-pad nibble (bits 7-4)
3. **SEL LOW**: Read button nibble (bits 3-0)
4. **CLR HIGH**: End scan, latch data

### Implementation

In the Joypad implementation, 2-button mode is the default. The full byte is constructed as:

```c
int8_t byte = (players[i].output_buttons & 0xff);
// Byte format: [Left, Down, Right, Up, Run, Select, II, I]
```

This byte is sent to the PIO state machine, which splits it into nibbles based on the SEL signal.

---

## 6-Button Mode

Extended mode for fighting games (Street Fighter II Championship Edition, Art of Fighting, etc.).

### Protocol Extension

6-button mode uses **two scan cycles** with different data on the second cycle:

**Cycle 1 (State 3, 1, 0)**: Standard 2-button data
```
Nibble 1: D-pad  [Left, Down, Right, Up]
Nibble 2: Buttons [Run, Select, II, I]
```

**Cycle 2 (State 2)**: Extended buttons
```
Nibble 1: Extended [III, IV, V, VI]  (bits 7-4)
Nibble 2: Reserved [0, 0, 0, 0]      (bits 3-0)
```

### State-Based Implementation

The Joypad implementation tracks a `state` variable (3 → 2 → 1 → 0) that cycles on each scan:

```c
if (is6btn && state == 2)
{
    byte = ((players[i].output_buttons>>8) & 0xf0);
}
```

- **State 3, 1, 0**: Send standard byte (D-pad + buttons I/II)
- **State 2**: Send extended byte (buttons III/IV/V/VI in high nibble)

### Button Mapping

| USB Input | PCEngine 6-Button |
|-----------|-------------------|
| B1 (A) | II |
| B2 (B) | I |
| B3 (X) | IV |
| B4 (Y) | III |
| L1 (LB) | VI |
| R1 (RB) | V |
| S1 (Select) | Select |
| S2 (Start) | Run |

### Mode Switching Hotkeys

Users can switch modes via hotkey combinations:

- **START + D-Up** → Enable 6-button mode
- **START + D-Down** → Enable 2-button mode
- **START + D-Right** → Enable 3-button mode (Select as III)
- **START + D-Left** → Enable 3-button mode (Run as III)

```c
if (!(players[i].output_buttons & (JP_BUTTON_S2 | JP_BUTTON_DU)))
    players[i].button_mode = BUTTON_MODE_6;
else if (!(players[i].output_buttons & (JP_BUTTON_S2 | JP_BUTTON_DD)))
    players[i].button_mode = BUTTON_MODE_2;
```

---

## 3-Button Modes

Special modes for games that only recognize 3 buttons (e.g., Street Fighter II on PCEngine CD).

### Mode Variants

**3-Button (Select as III)**:
- Buttons I, II work normally
- L1/R1 (or B3/B4) press → Triggers Select button
- Used when game reads "Select" as third attack button

**3-Button (Run as III)**:
- Buttons I, II work normally
- L1/R1 (or B3/B4) press → Triggers Run button
- Used when game reads "Run" as third attack button

### Implementation Logic

```c
// 3-button mode (Select as III)
if (is3btnSel)
{
    if ((~(players[i].output_buttons>>8)) & 0x30) // L1 or R1 pressed
    {
        byte &= 0b01111111;  // Clear Select bit (active LOW)
    }
}

// 3-button mode (Run as III)
if (is3btnRun)
{
    if ((~(players[i].output_buttons>>8)) & 0x30)
    {
        byte &= 0b10111111;  // Clear Run bit (active LOW)
    }
}
```

---

## Mouse Protocol

The PCEngine Mouse protocol sends 8-bit signed X/Y deltas broken into nibbles across 4 scan cycles.

### Protocol Structure

Each mouse update requires **4 scans** (states 3 → 2 → 1 → 0):

**High nibble**: Always contains buttons (Run, Select, II, I)
**Low nibble**: Contains movement data

```
State 3: [Buttons | X_high]  (X delta bits 7-4)
State 2: [Buttons | X_low]   (X delta bits 3-0)
State 1: [Buttons | Y_high]  (Y delta bits 7-4)
State 0: [Buttons | Y_low]   (Y delta bits 3-0)
```

### Delta Encoding

- **X-axis**: Left = `0x01` to `0x7F`, Right = `0x80` to `0xFF`
- **Y-axis**: Up = `0x01` to `0x7F`, Down = `0x80` to `0xFF`
- **Center/No movement**: `0x00`

### Movement Example

**Mouse moved right by 45 pixels, up by 23 pixels**:
```
X_delta = 45 (0x2D) = 0010 1101 binary
Y_delta = 23 (0x17) = 0001 0111 binary

State 3: [bbbb0010]  X high nibble
State 2: [bbbb1101]  X low nibble
State 1: [bbbb0001]  Y high nibble
State 0: [bbbb0111]  Y low nibble

(where bbbb = button state, e.g., 1111 if no buttons pressed)
```

### Delta Accumulation Strategy

USB mice report at ~125Hz to ~1000Hz, but PCEngine scans at ~60Hz. The implementation **accumulates deltas**:

```c
// Accumulate USB mouse reports
if (delta_x >= 128)
    players[player_index].global_x -= (256-delta_x);
else
    players[player_index].global_x += delta_x;

if (delta_y >= 128)
    players[player_index].global_y -= (256-delta_y);
else
    players[player_index].global_y += delta_y;

// Send accumulated deltas to console on next scan
players[player_index].output_analog_1x = players[player_index].global_x;
players[player_index].output_analog_1y = players[player_index].global_y;
```

After transmission (state 0 complete), deltas are **cleared**:

```c
players[i].global_x -= players[i].output_analog_1x;
players[i].global_y -= players[i].output_analog_1y;

players[i].output_analog_1x = 0;
players[i].output_analog_1y = 0;
```

This prevents drift while smoothing jitter from high-frequency USB reports.

### Compatible Games

- **Afterburner II** - Flight combat
- **Darius Plus** - Horizontal shooter
- **Lemmings** - Puzzle platformer

---

## Multitap Scanning

The PCEngine multitap supports **up to 5 players** via time-division multiplexing.

### Multitap Protocol

The multitap is a **passive device** containing:
- 5 controller ports
- Multiplexing logic (74-series shift registers)
- Single output to console

When the console scans, the multitap:
1. Reads Player 1 on first SEL toggle
2. Reads Player 2 on second SEL toggle
3. Continues through Player 5
4. Resets on CLR transition

### Data Packing

In the Joypad implementation, all 5 players are packed into two 32-bit words:

```
output_word_0 (32 bits):
┌─────────┬─────────┬─────────┬─────────┐
│ Player4 │ Player3 │ Player2 │ Player1 │
│  8 bits │  8 bits │  8 bits │  8 bits │
└─────────┴─────────┴─────────┴─────────┘

output_word_1 (32 bits):
┌─────────┬───────────────────────────┐
│ Player5 │         (unused)          │
│  8 bits │         24 bits           │
└─────────┴───────────────────────────┘
```

### PIO Sequencing

The `plex.pio` state machine handles player sequencing:

```asm
clr:
    set   y, 3        ; count 4-player output (Players 1-4)
    pull  block       ; pull output_word_1
    mov   x, osr      ; hold Player 5 in X register
    pull  block       ; pull output_word_0
    wait  0 pin 1     ; wait for CLR go low

sel:
    wait  1 pin 0     ; wait for SEL high
    out   PINS, 4     ; output D-pad nibble

    wait  irq 7       ; wait for SEL low or CLR high
    jmp   PIN, clr    ; restart if CLR high

    out   PINS, 4     ; output button nibble
    jmp   y--, sel    ; next player (decrement Y, loop)

    mov   osr, x      ; swap Player 5 into OSR
    set   x, 0        ; clear X register
    set   y, 1        ; continue for Player 5
    jmp   sel
```

**Key insight**: The PIO state machine automatically shifts through player bytes by consuming 8 bits (two 4-bit nibbles) per player per scan cycle.

### Timing Coordination

Three PIO state machines work together:

- **SM1 (plex.pio)**: Outputs data, sequences through players
- **SM2 (clock.pio)**: Monitors CLR signal, triggers Core 1
- **SM3 (select.pio)**: Monitors SEL signal, sets IRQ 7

IRQ 7 is used for **synchronization** between state machines.

---

## PIO State Machine Architecture

### State Machine Allocation

The implementation uses **3 PIO state machines** on PIO0:

| SM | Program | Purpose | Pins |
|----|---------|---------|------|
| SM1 | plex.pio | Data multiplexer & player sequencing | OUTD0-OUTD3 (output) |
| SM2 | clock.pio | CLR signal monitor | CLKIN_PIN (input) |
| SM3 | select.pio | SEL signal monitor | DATAIN_PIN (input) |

### plex.pio - Data Multiplexer

**Purpose**: Output player data nibbles based on SEL and CLR signals.

**Key features**:
- Pulls two 32-bit words from TX FIFO (double-buffered for Player 1-5 data)
- Outputs 4 bits at a time to D0-D3 pins
- Right-shifts through data (LSB first)
- Uses JMP PIN to detect CLR transitions
- Uses IRQ 7 for SEL synchronization

**Configuration**:
```c
sm_config_set_out_shift(&c,
    true,   // Shift-to-right = true (LSB first)
    false,  // Autopull disabled (manual PULL)
    31      // Autopull threshold (not used)
);

sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // Double TX FIFO size
```

### clock.pio - CLR Monitor

**Purpose**: Detect CLR signal transitions to trigger Core 1.

**Operation**:
```asm
clklp:
    wait 0 pin 0      ; wait for CLR LOW
    wait 1 pin 0      ; wait for CLR HIGH (negedge → posedge)
    irq 7 side 0      ; set IRQ 7, clear side-set pins
    in pins, 1        ; read CLR state into ISR
    jmp clklp         ; loop
```

**Purpose of IRQ 7 here**: Signals to plex.pio that SEL may have transitioned.

**Side-set**: Can optionally drive output pins to 0 (used for debugging or reset).

### select.pio - SEL Monitor

**Purpose**: Detect SEL signal transitions for synchronization.

**Operation**:
```asm
sellp:
    wait 1 pin 0      ; wait for SEL HIGH
    wait 0 pin 0      ; wait for SEL LOW (posedge → negedge)
    irq 7             ; set IRQ 7 to notify plex
    jmp sellp         ; loop
```

**IRQ 7 coordination**: Both clock.pio and select.pio set the same IRQ. The plex.pio waits on this IRQ to know when to output the next nibble or reset.

---

## Timing Requirements

### Scan Cycle Timing

**Typical timing** (measured on real hardware):
- CLR LOW duration: ~400-500µs
- SEL toggle period: ~50-100µs per toggle
- Full scan cycle: ~600-800µs

**Joypad timeout values**:
```c
#define RESET_PERIOD    600   // µs - timeout for end-of-scan detection
#define SCAN_TIMEOUT    550   // µs - max CLR LOW + SEL HIGH duration
```

### State Transition Flow

```
Time (µs)     CLR    SEL     State   Action
────────────────────────────────────────────────
0             HIGH   HIGH    Idle    Waiting for scan
50            LOW    HIGH    3       CLR negedge → Core 1 wakes
100           LOW    LOW     3       Output Player 1 D-pad
150           LOW    HIGH    3       Output Player 1 buttons
200           LOW    LOW     2       Output Player 2 D-pad
...           ...    ...     ...     ...
600           HIGH   HIGH    Reset   Timeout → state = 3
```

### Core 1 Blocking Loop

Core 1 runs an infinite loop synchronized to CLR transitions:

```c
void core1_entry(void)
{
    while (1)
    {
        // Block until CLR negedge (scan start)
        rx_bit = pio_sm_get_blocking(pio, sm2);

        // Lock output to prevent tearing
        output_exclude = true;

        // Push player data to PIO FIFO
        pio_sm_put(pio, sm1, output_word_1);  // Player 5
        pio_sm_put(pio, sm1, output_word_0);  // Players 1-4

        // Wait for scan to complete (CLR low, SEL high)
        loop_time = get_absolute_time();
        while ((gpio_get(CLKIN_PIN) == 0) && (gpio_get(DATAIN_PIN) == 1))
        {
            if (absolute_time_diff_us(loop_time, get_absolute_time()) > 550)
            {
                state = 0;  // Timeout → reset to state 0
                break;
            }
        }

        // Decrement state or handle mouse delta clear
        if (state != 0)
            state--;
        else
            /* clear mouse deltas */

        update_output();  // Refresh output words for next scan
        init_time = get_absolute_time();  // Reset timeout timer
    }
}
```

### Atomic Updates

To prevent **data tearing** (console reading partially updated data), the `output_exclude` flag gates updates:

```c
// Core 0 (USB processing) checks before updating players[]
if (!output_exclude)
{
    players[player_index].output_analog_1x = players[player_index].global_x;
    players[player_index].output_analog_1y = players[player_index].global_y;
    update_output();
}
```

When `output_exclude == true`, Core 0 skips output updates until the scan completes.

---

## Implementation Notes

### Turbo Button Feature

The implementation includes **auto-fire turbo** for buttons III and IV:

**Turbo timing**:
```c
cpu_frequency = clock_get_hz(clk_sys);       // e.g., 125 MHz
turbo_frequency = 1000000;                    // 1 MHz base
timer_threshold_a = cpu_frequency / (turbo_frequency * 2);   // ~15 Hz
timer_threshold_b = cpu_frequency / (turbo_frequency * 20);  // ~6 Hz
```

**Turbo logic**:
```c
turbo_timer++;
if (turbo_timer >= timer_threshold)
{
    turbo_timer = 0;
    turbo_state = !turbo_state;  // Toggle turbo
}

if (turbo_state)
{
    if ((~(players[i].output_buttons>>8)) & 0x20)  // Button III
        byte &= 0b11011111;  // Auto-press II (turbo)
    if ((~(players[i].output_buttons>>8)) & 0x10)  // Button IV
        byte &= 0b11101111;  // Auto-press I (turbo)
}
```

**Turbo speed control**:
- **L1 pressed**: Use `timer_threshold_a` (~15 Hz)
- **R1 pressed**: Use `timer_threshold_b` (~6 Hz)

### SOCD Cleaning

**Simultaneous Opposite Cardinal Directions** are resolved:

**Up + Down priority**: Up wins
```c
if (((~output_buttons) & 0x01) && ((~output_buttons) & 0x04)) {
    output_buttons ^= 0x04;  // Cancel Down
}
```

**Left + Right neutral**: Both cancelled
```c
if (((~output_buttons) & 0x02) && ((~output_buttons) & 0x08)) {
    output_buttons ^= 0x0a;  // Cancel both (XOR to set bits)
}
```

This prevents diagonal input exploits in certain games.

### EverDrive Pro Hotkey Workaround

The TurboEverDrive Pro uses hotkeys that conflict with game input. The implementation detects these hotkeys on Player 1 and suppresses input from other players:

```c
int16_t hotkey = 0;

if (i == 0)
{
    int16_t btns = (~players[i].output_buttons & 0xff);
    if      (btns == 0x82) hotkey = ~0x82;  // RUN + RIGHT
    else if (btns == 0x88) hotkey = ~0x88;  // RUN + LEFT
    else if (btns == 0x84) hotkey = ~0x84;  // RUN + DOWN
}

if (hotkey)
{
    byte &= hotkey;  // Suppress conflicting buttons
}
```

### Dual-Core Efficiency

**Core 0** (main):
- Runs USB polling loop (`tuh_task()`)
- Updates `players[].global_buttons` and `players[].global_x/y`
- Calls `post_globals()` on USB input events
- Checks `output_exclude` before updating output state

**Core 1** (console output):
- Dedicated to PCEngine protocol timing
- Blocks on PIO waiting for CLR signal
- Pushes data to PIO FIFO
- Manages `state` transitions
- Calls `update_output()` to refresh data

**No mutexes needed**: The `output_exclude` flag provides lock-free coordination.

### Memory-Mapped I/O

Critical functions are kept in **SRAM** (not XIP flash) for deterministic timing:

```c
void __not_in_flash_func(core1_entry)(void);
void __not_in_flash_func(update_output)(void);
void __not_in_flash_func(post_globals)(...);
void __not_in_flash_func(post_mouse_globals)(...);
```

Flash XIP adds ~100ns latency per access, which can disrupt tight timing loops.

---

## Quick Reference

### Button Bit Mapping

**Standard Byte** (States 3, 1, 0):
```
Bit:  7     6      5      4    |  3    2       1   0
     Left  Down  Right   Up   | Run  Select   II  I
```

**6-Button Extended** (State 2):
```
Bit:  7     6     5    4   |  3   2   1   0
     III   IV    V    VI  |  0   0   0   0
```

**Mouse Byte** (per state):
```
Bit:  7     6       5   4   |  3    2    1    0
     Run  Select   II  I   | [Movement Nibble]
```

### Pin Assignments (Default KB2040)

| Function | GPIO | Description |
|----------|------|-------------|
| DATAIN_PIN | 18 | SEL input |
| CLKIN_PIN | 19 | CLR input |
| OUTD0_PIN | 26 | Data bit 0 output |
| OUTD1_PIN | 27 | Data bit 1 output |
| OUTD2_PIN | 28 | Data bit 2 output |
| OUTD3_PIN | 29 | Data bit 3 output |

### PIO Resource Usage

| Resource | Usage |
|----------|-------|
| PIO blocks | 1 (PIO0) |
| State machines | 3 (SM1, SM2, SM3) |
| Instruction memory | ~30 instructions total |
| IRQ | IRQ 7 (for synchronization) |

---

## Acknowledgments

- **David Shadoff** - [PCEMouse](https://github.com/dshadoff/PC_Engine_RP2040_Projects/tree/main/PCEMouse) foundation
- **NEC / Hudson Soft** - Original PCEngine hardware design
- **Retro community** - Protocol documentation and testing

---

## References

- [PCEngine Development Wiki](https://www.nesdev.org/wiki/PC_Engine_hardware)
- [David Shadoff's RP2040 Projects](https://github.com/dshadoff/PC_Engine_RP2040_Projects)
- [PC Engine Software Bible](http://www.magicengine.com/mkit/doc_hard_pce.html)
- [TurboGrafx-16 Technical Specifications](https://en.wikipedia.org/wiki/TurboGrafx-16)

---

*This protocol implementation demonstrates efficient use of RP2040's dual-core architecture and PIO capabilities for precise timing-critical retro console protocols.*

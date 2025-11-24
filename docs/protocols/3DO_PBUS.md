# 3DO Player Bus (PBUS) Controller Protocol

**Serial shift register daisy-chain system supporting hot-swappable multi-device input**

Implemented by Robert Dale Smith (2025)
Based on 3DO Opera hardware documentation and PBUS protocol specification

This document provides a comprehensive technical reference for the 3DO Player Bus (PBUS) protocol, covering device identification, daisy-chain mechanics, bit structures for all supported device types, and RP2040 PIO implementation strategies.

---

## Table of Contents

- [Overview](#overview)
- [Physical Layer](#physical-layer)
- [Protocol Architecture](#protocol-architecture)
- [Device Types & Bit Structures](#device-types--bit-structures)
  - [Control Pad (Joypad)](#control-pad-joypad)
  - [Analog Controller (Flightstick)](#analog-controller-flightstick)
  - [Mouse](#mouse)
  - [Lightgun](#lightgun)
  - [Arcade Controls](#arcade-controls)
- [Daisy Chain Mechanism](#daisy-chain-mechanism)
- [Device Identification](#device-identification)
- [Initialization & Hot-Swapping](#initialization--hot-swapping)
- [PIO Implementation](#pio-implementation)
- [Timing Requirements](#timing-requirements)
- [Implementation Notes](#implementation-notes)

---

## Overview

The **3DO Player Bus (PBUS)** is a serial shift register protocol developed by The 3DO Company for the Opera hardware platform (1993). The protocol enables sophisticated multi-device input handling with hot-swap support and automatic device detection.

### Key Characteristics

- **Serial shift register**: Clock-synchronized bit-by-bit transmission
- **Daisy-chainable**: Up to ~56 devices theoretically supported (448 bits per field)
- **Hot-swappable**: Device IDs transmitted with every packet enable dynamic reconfiguration
- **Self-identifying**: Each device transmits its type code in every response
- **Bidirectional**: Console sends data out while simultaneously reading extension devices
- **Zero-terminated**: String of zeros marks end of daisy chain

### Historical Context

The PBUS protocol appears in patent WO09410636a1 (1994), designed for 3DO Interactive Multiplayer set-top hardware. The protocol reflects bandwidth constraints of mid-90s consumer electronics while providing advanced features like analog input and multi-device coordination. The clever ID encoding exploits physical impossibility (joysticks can't press up+down simultaneously) to eliminate hardware conflicts.

---

## Physical Layer

### Connector Pinout

The PBUS uses a proprietary 15-pin D-sub connector (detailed in FZ-1 Technical Guide):

| Pin | Name | Direction | Description |
|-----|------|-----------|-------------|
| 1 | DATA_OUT | Output (to devices) | Serial data to first device |
| 2 | CLK | Output (to devices) | Clock signal for bit synchronization |
| 3 | DATA_IN | Input (from devices) | Serial data from daisy chain |
| 4-5 | GND | - | Ground |
| 6-8 | VCC | - | +5V power supply |
| 9-15 | NC | - | Not connected / reserved |

**Note**: Exact pinout varies by 3DO model. Above represents typical configuration based on Waveshare RP2040 Zero adaptation:
- CLK_PIN: GPIO 2
- DATA_OUT_PIN: GPIO 3
- DATA_IN_PIN: GPIO 4
- CS_CTRL_PIN: GPIO 5

### Electrical Characteristics

- **Logic levels**: TTL compatible (assumed 0V = LOW, 5V = HIGH)
- **Clock frequency**: Field-synchronized (60 Hz NTSC / 50 Hz PAL)
- **Data timing**: Synchronized to CLK edges (sample on rising, shift on falling)
- **Power**: 5V per device (current draw varies by device type)

### Signal Behavior

```
CLK:       ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐     (Clock from console)
           ┘ └─┘ └─┘ └─┘ └─┘ └───

DATA_OUT:  ──█─█─█─█─█─█─█─█─────  (Console sends controller data)

DATA_IN:   ──────█─█─█─█─█─█─────  (Simultaneous read from extension)
```

**Bidirectional Operation**:
- Console shifts out controller data to first device
- Simultaneously reads data from end of daisy chain
- Each device in chain shifts in data while shifting out to next device
- One clock cycle = one bit transmitted/received

---

## Protocol Architecture

### Data Structure

All PBUS transmissions follow this structure:

```
[4-bit ID] [Device-specific data] [0x00 terminator]
     └─ Identifies device type      └─ End-of-chain marker
```

**Key Protocol Features**:

1. **Minimum 8 bits per device**: Each device must transmit at least 1 byte
2. **Maximum 448 bits per field**: Total bandwidth limit (~56 bytes)
3. **ID-first transmission**: Device type always sent before data
4. **Zero-string termination**: Multiple 0x00 bytes indicate no more devices

### Bit Transmission Order

- **MSB first**: Most significant bit transmitted first
- **Active-HIGH encoding**: 1 = pressed/active, 0 = not pressed (opposite of PCEngine)
- **Nibble-aligned**: Device IDs use 4-bit codes

---

## Device Types & Bit Structures

### Control Pad (Joypad)

**Total length**: 8 bits (1 byte)

```
Bit 7 6 5 4 3 2 1 0
    │ │ │ │ │ │ │ └─ FIRE-1 (B button)
    │ │ │ │ │ │ └─── FIRE-2 (A button)
    │ │ │ │ │ └───── SWITCH-1 (C button)
    │ │ │ │ └─────── SWITCH-2
    │ │ │ └───────── LEFT
    │ │ └─────────── RIGHT
    │ └───────────── UP
    └─────────────── DOWN
```

**ID Detection**: First 2 bits of nibble must be non-zero (01, 10, or 11)

**Button States** (active-HIGH):
- 1 = Button pressed / D-pad direction active
- 0 = Button released / D-pad neutral

**Example Byte Values**:
- `0x8F` = D-pad neutral, all buttons pressed
- `0x40` = RIGHT pressed, no buttons
- `0xFF` = All inputs active (impossible in practice)

**Special Characteristics**:
- Cannot report simultaneous opposing directions (up+down, left+right)
- This physical constraint enables ID encoding (first 2 bits never 00)
- Escape sequence: 1100 (reserved for protocol extensions)

### Analog Controller (Flightstick)

**Total length**: 72 bits (9 bytes)

```
Byte 0: 0x01          (ID byte 1)
Byte 1: 0x7B          (ID byte 2)
Byte 2: 0x08          (Length field)
Byte 3: Horizontal    (10-bit X-axis, upper 8 bits)
Byte 4: Vertical      (10-bit Y-axis, upper 8 bits)
Byte 5: Depth         (10-bit Z-axis, upper 8 bits)
Byte 6: [H2][V2][D2][pad]  (Lower 2 bits of each axis + padding)
Byte 7: Button states 1 (DPAD + A/B/C/Trigger)
Byte 8: Button states 2 (L/R/X/P)
```

**Analog Ranges**:
- Center: ~512 (0x200 in 10-bit)
- Min: 0 (full left/up/push)
- Max: 1023 (full right/down/pull)

**Button Mapping** (Bytes 7-8):
- Same as standard joypad, plus FIRE (trigger) button
- 10 total buttons: FIRE, A, B, C, X, P, L, R, + DPAD (4 directions)

### Mouse

**Total length**: 24 bits (3 bytes)

```
Byte 0: 0x49          (Mouse ID)
Byte 1: [Δ X-high 4 bits][buttons]
Byte 2: Δ X low 8 bits
Byte 3: [Δ Y-high 2 bits][Δ Y low 6 bits]
```

**Button Layout** (in byte 1):
- Bit 7: LEFT button
- Bit 6: MIDDLE button
- Bit 5: RIGHT button
- Bit 4: SHIFT button (modifier)

**Delta Encoding**:
- **X-axis**: 10-bit signed value (-512 to +511)
  - Upper 4 bits in byte 1, lower 8 bits in byte 2
- **Y-axis**: 10-bit signed value (-512 to +511)
  - Upper 2 bits in byte 1, lower 6 bits in byte 3
- Negative values use two's complement
- Deltas are relative motion since last frame

**Conversion Example**:
```c
int16_t delta_x = ((buffer[1] & 0x0F) << 8) | buffer[2];
if (delta_x & 0x800) delta_x |= 0xF000; // Sign extend

int16_t delta_y = ((buffer[1] & 0x03) << 6) | (buffer[2] & 0x3F);
if (delta_y & 0x200) delta_y |= 0xFC00; // Sign extend
```

### Lightgun

**Total length**: 32 bits (4 bytes)

```
Byte 0: 0x4D          (Lightgun ID)
Byte 1: Counter[19:12] (Timing counter, upper 8 bits)
Byte 2: Counter[11:4]  (Timing counter, middle 8 bits)
Byte 3: [Counter[3:0]][Line[4:0]][Buttons]
```

**Timing Counter** (20-bit):
- Measures time from field start to beam detection
- Used with timing constants to calculate X position
- NTSC defaults: XSCANTIME=1030, TIMEOFFSET=-12835

**Line Pulse Count** (5-bit):
- Number of scanlines where beam was detected
- Used with YSCANTIME constant to calculate Y position
- NTSC default: YSCANTIME=12707

**Button States**:
- TRIGGER: Primary fire button
- SERVICE: Arcade service button
- COIN: Arcade coin button
- START: Arcade start button
- HOLSTER/OPTION: Mode switch

**Position Calculation**:
```c
x_position = (counter * XSCANTIME) + TIMEOFFSET;
y_position = line_count * YSCANTIME;
```

### Arcade Controls

**Total length**: 16 bits (2 bytes)

```
Byte 0: 0xC0          (Arcade ID - "SILLY_CONTROL_PAD")
Byte 1: [COIN_P1][COIN_P2][START_P1][START_P2][SERVICE][pad][pad][pad]
```

**Button Mapping**:
- Bit 7: COIN (Player 1)
- Bit 6: COIN (Player 2)
- Bit 5: START (Player 1)
- Bit 4: START (Player 2)
- Bit 3: SERVICE (arcade maintenance)
- Bits 2-0: Padding (reserved)

**Use Case**:
- Orbatak arcade controls for arcade game support
- Separates coin/start inputs from gameplay buttons
- Designed for arcade cabinet integration

---

## Daisy Chain Mechanism

### Physical Topology

```
┌─────────┐         ┌────────┐         ┌────────┐         ┌────────┐
│ Console │────────▶│ USB    │────────▶│ 3DO    │────────▶│ 3DO    │
│         │◀────────│ Adapter│◀────────│ Pad #1 │◀────────│ Pad #2 │
└─────────┘         └────────┘         └────────┘         └────────┘
     │                   │                   │                   │
   CLK ────────────────────────────────────────────────────────▶
  D_OUT ──█████──────────────────────────────────────────────▶
  D_IN  ◀────────────────────────────────────────────█████───
```

### Data Flow

**Console Perspective**:
1. Console shifts out controller data on DATA_OUT (USB adapter data)
2. Simultaneously samples DATA_IN for extension device data
3. Each clock cycle: send 1 bit, receive 1 bit
4. After 8-448 bits, complete device data captured

**Device Perspective** (each device in chain):
1. Shift in data from previous device (or console)
2. When counter reaches device's bit count, start outputting own data
3. Shift own data out to next device
4. After transmission complete, pass through subsequent device data

**Passthrough Buffering**:
- USB adapter must buffer extension data from one field
- Next field: send USB data + buffered extension data
- This creates 1-frame latency for extension controllers

### Example 3-Device Chain

```
Field N:
Console sends:  [USB Pad Data: 16 bits]
Console reads:  [3DO Pad 1: 8 bits][3DO Pad 2: 8 bits][0x0000 end]

Field N+1:
Console sends:  [USB Pad Data: 16 bits][Buffered 3DO data: 16 bits + end]
Console reads:  [New 3DO Pad 1: 8 bits][New 3DO Pad 2: 8 bits][0x0000 end]
```

---

## Device Identification

### ID Encoding Scheme

The PBUS uses a clever encoding that exploits physical controller constraints:

**Joypad ID Rules**:
- First 2 bits (bits 7-6) CANNOT be 00
- Physical impossibility: joystick can't press up+down simultaneously
- Valid IDs: 01QQ, 10QQ, 11QQ (where Q = 0 or 1)
- This reserves 00XX pattern for other devices

**Other Device IDs**:
- First 2 bits = 00 indicates non-joypad device
- Followed by unique device-specific patterns
- Examples: 0x01 (flightstick prefix), 0x49 (mouse), 0x4D (lightgun), 0xC0 (arcade)

### ID Detection Algorithm

```c
uint8_t byte1 = buffer[offset];
uint8_t id_nibble = (byte1 >> 4) & 0x0F;

if ((id_nibble & 0b1100) != 0) {
    // Joypad: First 2 bits are non-zero
    device_type = DEVICE_JOYPAD;
    bytes_to_read = 1;
} else if (byte1 == 0x01) {
    // Check next byte for flightstick signature
    if (buffer[offset+1] == 0x7B && buffer[offset+2] == 0x08) {
        device_type = DEVICE_FLIGHTSTICK;
        bytes_to_read = 9;
    }
} else if (byte1 == 0x49) {
    device_type = DEVICE_MOUSE;
    bytes_to_read = 3;
} else if (byte1 == 0x4D) {
    device_type = DEVICE_LIGHTGUN;
    bytes_to_read = 4;
} else if (byte1 == 0xC0) {
    device_type = DEVICE_ARCADE;
    bytes_to_read = 2;
}
```

### End-of-Chain Detection

**String of Zeros**: Multiple 0x00 bytes indicate no more devices

```c
bool is_end_of_chain(uint8_t* buffer, size_t offset, size_t length) {
    // Check for 4+ consecutive zero bytes
    for (size_t i = 0; i < 4 && (offset + i) < length; i++) {
        if (buffer[offset + i] != 0x00) {
            return false;
        }
    }
    return true;
}
```

**Why String Instead of Single Zero**:
- Prevents false detection from legitimate 0x00 in device data
- Provides noise immunity
- Clear unambiguous termination marker

---

## Initialization & Hot-Swapping

### System Initialization Sequence

**Console Bootup**:
1. Console sends "string of zeros" on DATA_OUT
2. All devices receive zeros as initialization signal
3. Devices reset internal state or ignore as don't-care
4. No handshake required - devices self-configure

**First Data Read**:
1. Console clocks out data (zeros initially)
2. Devices respond with ID + data
3. Console builds device map from received IDs
4. Portfolio OS loads appropriate drivers dynamically

### Hot-Swap Support

**Device Addition**:
- New device appears in next frame's data stream
- ID code identifies device type automatically
- OS loads driver if not already present
- No console reset required

**Device Removal**:
- Device stops responding
- Earlier terminator (zeros) detected
- OS gracefully handles missing device
- No console reset required

**Driver Loading**:
- Portfolio OS supports dynamic driver loading
- Drivers can be in filesystem or transmitted via PBUS
- Driver format: AIF (Arm Image Format)
- ID code → driver mapping:
  - 0x01: StickDriver (flightstick)
  - 0x49: MouseDriver (cport49.rom)
  - 0x4D: LightGunRom
  - 0xC0: Arcade controls

### Device Map Example

```
Frame 1: [Joypad 0x8F][Mouse 0x49...]
         → OS detects: 1 joypad, 1 mouse

Frame 2: [Joypad 0x8F][Mouse 0x49...][Joypad 0xBF]
         → OS detects: 2 joypads, 1 mouse (hot-add)

Frame 3: [Joypad 0x8F][Joypad 0xBF]
         → OS detects: 2 joypads (mouse removed)
```

---

## PIO Implementation

### RP2040 PIO State Machine

The PBUS protocol is implemented using RP2040's PIO (Programmable I/O) for precise bit timing:

**PIO Program** (`output.pio`):
```asm
.program output

.define CLK_PIN 2

public entry_point:
  wait 0 gpio CLK_PIN           ; Wait for clock low
.wrap_target
start:
  out pins, 1                   ; Shift out 1 bit to DATA_OUT
  wait 1 gpio CLK_PIN           ; Wait for clock high
  wait 0 gpio CLK_PIN           ; Wait for clock low
  in pins, 1                    ; Shift in 1 bit from DATA_IN
.wrap
```

**Key Features**:
- Clock-synchronized: waits for CLK edges
- Simultaneous TX/RX: output and input in same cycle
- Auto-wrapping: continuous operation without CPU intervention
- DMA-driven: bulk transfers via DMA channels

### DMA Architecture

**Dual-channel DMA**:

```c
// Channel 0: OUTPUT - Send data to console
dma_channel_config cfg_out = dma_channel_get_default_config(CHAN_OUTPUT);
channel_config_set_transfer_data_size(&cfg_out, DMA_SIZE_8);
channel_config_set_read_increment(&cfg_out, true);
channel_config_set_dreq(&cfg_out, pio_get_dreq(pio, sm, true));

// Channel 1: INPUT - Receive extension data
dma_channel_config cfg_in = dma_channel_get_default_config(CHAN_INPUT);
channel_config_set_transfer_data_size(&cfg_in, DMA_SIZE_8);
channel_config_set_write_increment(&cfg_in, true);
channel_config_set_dreq(&cfg_in, pio_get_dreq(pio, sm, false));
```

**Buffer Management**:
```c
uint8_t controller_buffer[201];  // 448 bits max / 8 = 56 bytes (padded to 201)

// Output: USB controllers + buffered extension data from previous frame
start_dma_transfer(CHAN_OUTPUT, &controller_buffer[0], 201);

// Input: New extension data (will be sent next frame)
start_dma_transfer(CHAN_INPUT, &controller_buffer[total_usb_size], 201 - total_usb_size);
```

### IRQ Handling

**Console Scan Detection**:
```c
void on_pio0_irq(void) {
    // Console started new scan
    dma_channel_abort(dma_channels[CHAN_OUTPUT]);
    dma_channel_abort(dma_channels[CHAN_INPUT]);

    // Prepare buffer with USB data + buffered extension data
    prepare_controller_buffer();

    // Restart DMA transfers
    start_dma_transfer(CHAN_OUTPUT, buffer, size);
    start_dma_transfer(CHAN_INPUT, buffer + usb_size, remaining);
}
```

---

## Timing Requirements

### Field Timing

**NTSC** (60 Hz):
- Field duration: 16.67 ms
- Max bit time: 16.67 ms / 448 bits ≈ 37.2 µs per bit
- Typical clock: ~6 kHz (166 µs per bit)

**PAL** (50 Hz):
- Field duration: 20 ms
- Max bit time: 20 ms / 448 bits ≈ 44.6 µs per bit
- Typical clock: ~5 kHz (200 µs per bit)

### Clock Edges

**Sample Timing**:
- Data valid on CLK rising edge
- Sample DATA_IN on CLK rising edge
- Shift DATA_OUT on CLK falling edge
- Setup time: ~100 ns (typical TTL)

**Propagation Delay**:
- Each device in chain adds ~50-100 ns delay
- Max chain length limited by cumulative delay
- Practical limit: ~10-15 devices before timing issues

---

## Implementation Notes

### Extension Controller Detection

To detect extension controllers, parse the input buffer after each field:

```c
uint8_t count = 0;
size_t offset = total_usb_size;  // Start after USB data

while (offset < buffer_size) {
    uint8_t byte1 = buffer[offset];

    // Check for end-of-chain
    if (is_string_of_zeros(&buffer[offset])) {
        break;
    }

    // Identify device and skip appropriate bytes
    if (is_joypad(byte1)) {
        count++;
        offset += 1;
    } else if (is_flightstick(byte1, &buffer[offset])) {
        count++;
        offset += 9;
    } else if (is_mouse(byte1)) {
        count++;
        offset += 3;
    }
    // ... handle other device types
}

return count;  // Total extension controllers detected
```

### Passthrough Buffering Strategy

**Double Buffering**:
```
Buffer A: USB data sent this frame + Extension data from last frame
Buffer B: Extension data received this frame (will be sent next frame)

Frame N:
  Send: Buffer A
  Receive: Buffer B → becomes Buffer A for Frame N+1

Frame N+1:
  Send: Buffer A (was Buffer B)
  Receive: new Buffer B
```

### Common Pitfalls

1. **Inverted Logic Confusion**:
   - PBUS uses active-HIGH (opposite of PCEngine)
   - 1 = pressed, 0 = released
   - Don't forget to invert when porting from other protocols

2. **ID Detection Order**:
   - Check joypad ID first (most common)
   - Then check specific device IDs (mouse, gun, etc.)
   - Flightstick requires 3-byte signature check

3. **End Marker Sensitivity**:
   - Single 0x00 byte is NOT end-of-chain
   - Must verify "string of zeros" (4+ bytes)
   - Prevents false positives from data

4. **Buffer Size**:
   - Allocate full 56 bytes even if unused
   - Prevents overflow when many devices connected
   - 201-byte buffer used in USBRetro (includes padding)

5. **Frame Latency**:
   - Extension controllers have 1-frame delay (16-33ms)
   - Imperceptible for most games
   - Critical for precision timing games (consider compensation)

---

## Acknowledgments

This protocol documentation and implementation would not have been possible without the contributions and prior work of:

### Technical References
- **3dodev.com**: Comprehensive PBUS protocol specification and Opera hardware documentation that served as the primary technical reference for this implementation
- **Patent WO09410636a1**: Original PBUS protocol design documentation (1994)

### Prior Art and Inspiration
- **FCare's USBTo3DO**: The original USB-to-3DO adapter project that pioneered USB controller conversion for the 3DO platform and demonstrated the feasibility of modern USB input on vintage hardware
- **SNES23DO Project**: Provided valuable bit-level parsing techniques for extension controller detection and served as a reference for implementing daisy-chain passthrough buffering

### Community and Testing
- The 3DO developer community at 3dodev.com for preserving and sharing technical documentation
- Early adopters and testers who provided feedback during development

This implementation builds upon these foundations while adding support for modern USB controllers, profile switching, and comprehensive extension controller detection.

---

## References

### Official Documentation
- **3DO Opera Hardware Documentation**: [https://3dodev.com/documentation/hardware/opera/pbus](https://3dodev.com/documentation/hardware/opera/pbus)
- **Patent WO09410636a1**: PBUS protocol design (1994)
- **FZ-1 Technical Guide**: Physical connector specifications

### Related Projects
- **USBTo3DO by FCare**: Original USB-to-3DO adapter ([GitHub](https://github.com/FCare/USBTo3DO))
- **SNES23DO**: SNES-to-3DO adapter with extension parsing ([GitHub](https://github.com/RobertDaleSmith/snes23do))

### Implementation
- **USBRetro 3DO Module**: `/src/console/3do/` - Full PBUS implementation with extension detection
- **PIO State Machines**: `/src/console/3do/output.pio` - Clock-synchronized bidirectional I/O
- **Extension Parsing**: `/src/console/3do/3do.c` - `parse_extension_controllers()` function

---

**Document Version**: 1.0
**Last Updated**: January 2025
**Implementation Status**: Complete with extension controller detection

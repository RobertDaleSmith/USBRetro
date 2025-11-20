# GameCube / Wii Adapter

USB controller adapter for GameCube and Wii consoles with advanced features.

## Features

### 🎮 Controller Profiles

Switch between optimized button mappings for different games:

- **Default** - Standard mapping for most games
- **SNES** - SNES-style layout (shoulder buttons → triggers)
- **SSBM** - Super Smash Bros. Melee competitive (custom triggers, 85% stick)
- **MKWII** - Mario Kart Wii optimized
- **Fighting** - Fighting game layout

**Switching Profiles:**
1. Hold **Start + Select** (or equivalent on your controller)
2. Press **D-Pad direction**:
   - **Up** → Default
   - **Left** → SNES
   - **Down** → SSBM
   - **Right** → MKWii
3. Controller will rumble and LED will flash to confirm
4. Profile saves to flash memory (persists across power cycles)

### ⌨️ Keyboard Mode

Use a USB keyboard as a GameCube keyboard:

- Press **Scroll Lock** or **F14** to toggle keyboard mode
- LED indicator shows when active
- All standard keys mapped to GameCube keyboard protocol
- Useful for Phantasy Star Online and other keyboard-compatible games

### 🤝 Copilot Mode

Combine up to 4 USB controllers into a single GameCube controller:

- Connect multiple controllers to USB hub
- All inputs merge into one controller output
- Perfect for accessibility or assisted gameplay
- Each player can control different buttons simultaneously

### 📳 Rumble Support

Full rumble/vibration support for compatible controllers:
- Xbox controllers (all generations)
- PlayStation DualShock 3/4/5
- Switch Pro Controller
- 8BitDo controllers

## Button Mappings

### Default Profile

| USB Input | GameCube Output |
|-----------|----------------|
| B1 (A/Cross) | B |
| B2 (B/Circle) | A |
| B3 (X/Square) | Y |
| B4 (Y/Triangle) | X |
| L1 (LB/L) | L |
| R1 (RB/R) | R |
| L2 (LT/ZL) | L Analog |
| R2 (RT/ZR) | R Analog |
| S1 (Back/Select) | Z |
| S2 (Start) | Start |
| A2 | Z |
| D-Pad | D-Pad |
| Left Stick | Control Stick |
| Right Stick | C-Stick |

### SNES Profile

Optimized for SNES-style games (Super Mario World, etc.):

| USB Input | GameCube Output |
|-----------|----------------|
| B1 (A) | A |
| B2 (B) | B |
| B3 (X) | Y |
| B4 (Y) | X |
| L1 (LB) | L Digital + Analog |
| R1 (RB) | R Digital + Analog |
| L2 (LT) | (none) |
| R2 (RT) | (none) |

### SSBM Profile

Competitive Super Smash Bros. Melee mapping:

| USB Input | GameCube Output | Notes |
|-----------|----------------|-------|
| B1 (A) | B | Attack |
| B2 (B) | A | Jump |
| B3 (X) | Y | Jump |
| B4 (Y) | X | Jump |
| L1 (LB) | Z | Grab |
| R1 (RB) | X | Jump |
| L2 (LT @ 88%) | L Digital + 17% Analog | Light shield |
| R2 (RT @ 55%) | L + R Digital | Quit combo |
| Left Stick | Control Stick (85%) | Reduced for precision |

**Key Features:**
- Light shield at 17% on L trigger
- Quick quit combo (L+R+Start) on RT
- Multiple jump buttons (A, X, Y, RB)
- 85% stick sensitivity for precise movement

### MKWii Profile

Mario Kart Wii optimized:

| USB Input | GameCube Output | Notes |
|-----------|----------------|-------|
| L2 (LT) | L + R Digital | Drift |
| R2 (RT) | (none) | |
| L1 (LB) | (none) | |
| R1 (RB) | Z | Item |

### Fighting Profile

2D fighting game layout:

| USB Input | GameCube Output |
|-----------|----------------|
| B1 (A) | A | Light punch |
| B2 (B) | B | Light kick |
| B3 (X) | Y | Medium punch |
| B4 (Y) | X | Medium kick |
| L1 (LB) | L | Heavy punch |
| R1 (RB) | R | Heavy kick |

## Hardware Requirements

- **Board**: Adafruit KB2040 (default), also supports Pico, QT Py, Waveshare RP2040-Zero
- **Clock**: 130MHz (overclocked for joybus timing)
- **Protocol**: GameCube joybus via PIO (timing-critical)
- **Connector**: GameCube controller cable (cut and solder to board pins)

## Special Notes

- **Profile persistence**: Uses last 4KB of flash memory (~100K write cycles)
- **Flash writes**: Debounced 5 seconds after profile change to reduce wear
- **Dual-core**: Core 0 handles USB input, Core 1 runs joybus protocol
- **Timing**: Joybus requires precise timing - flash writes pause Core 1 briefly (~100ms)

## Troubleshooting

**Controller not detected:**
- Check GameCube cable connections
- Verify 3.3V power and ground pins
- Check data pin assignment in firmware

**Rumble not working:**
- Only compatible controllers support rumble
- Check USB power supply (rumble requires more current)
- Some controllers need initialization time

**Profile not saving:**
- Wait 5 seconds after profile change for flash write
- Check that flash memory isn't corrupted
- Reflash firmware if needed

**Keyboard mode not activating:**
- Press Scroll Lock or F14 key
- Check LED indicator
- Some keyboards may not be compatible

## Product Links

- [GC USB Adapter](https://controlleradapter.com/products/gc-usb) - Pre-built hardware
- [GitHub Releases](https://github.com/RobertDaleSmith/USBRetro/releases) - Latest firmware

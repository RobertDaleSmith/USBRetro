# Nuon DVD Player Adapter

USB controller adapter for Nuon DVD game players with spinner support.

## Features

### 🎮 Standard Controller

Full Nuon controller emulation:
- 8 digital buttons (A, B, Start, Nuon, C-Up, C-Down, C-Left, C-Right)
- D-Pad navigation
- Analog support via USB controller triggers/sticks

### 🌀 Spinner Controller

Tempest 3000 spinner mode using mouse input:
- USB mouse X-axis → Spinner rotation
- Precise control for Tempest 3000
- Auto-detects when mouse connected

## Button Mappings

### Standard Controller

| USB Input | Nuon Output |
|-----------|-------------|
| B1 (A/Cross) | A |
| B2 (B/Circle) | C-Down |
| B3 (X/Square) | B |
| B4 (Y/Triangle) | C-Left |
| L1 (LB/L) | L |
| R1 (RB/R) | R |
| L2 (LT/ZL) | C-Up |
| R2 (RT/ZR) | C-Right |
| S1 (Back/Select) | Nuon |
| S2 (Start) | Start |
| A2 (Touchpad/Capture) | Nuon |
| D-Pad | D-Pad |

### Spinner Mode (Mouse)

| USB Mouse | Nuon Spinner |
|-----------|--------------|
| X Movement | Spinner Rotation |
| Left Click | Fire |
| Right Click | (not mapped) |

**Tempest 3000 Controls:**
- Mouse left/right → Spinner rotation
- Left click → Fire
- Keyboard/Controller → Secondary buttons

## Hardware Requirements

- **Board**: Adafruit KB2040 (default), also supports Pico, QT Py, Waveshare RP2040-Zero
- **Protocol**: Custom serial protocol via PIO
  - `polyface_read.pio` - Read from console
  - `polyface_send.pio` - Send to console
- **Connector**: Nuon controller port (custom pinout)

## Nuon Controller Protocol

Nuon uses a custom bidirectional serial protocol:
- Bidirectional communication
- Console polls controller at ~60Hz
- Controller responds with button state
- Supports hot-plugging

## Spinner Sensitivity

Default spinner sensitivity optimized for Tempest 3000:
- 1:1 mouse movement to rotation
- Adjustable in firmware (`NUON_SPINNER_SCALE`)
- No acceleration curve (linear response)

## Compatible Nuon Players

- Samsung DVD-N501
- Samsung DVD-N504 / N505
- Toshiba SD-2300
- Motorola Streamaster 5000
- RCA DRC300N / DRC480N

## Compatible Games

### Standard Controller:
- Iron Soldier 3
- Ballistic
- Space Invaders XL
- Merlin Racing
- Freefall 3050 A.D.

### Spinner Required:
- **Tempest 3000** - Premium spinner experience with USB mouse

## Troubleshooting

**Controller not detected:**
- Check Nuon port connections
- Verify power and ground
- Check data pins match protocol

**Spinner too sensitive:**
- Adjust `NUON_SPINNER_SCALE` in firmware
- Try different mouse DPI settings
- Some mice have on-the-fly DPI adjustment

**Buttons not responding:**
- Verify button mapping matches game expectations
- Check USB controller is detected
- Test with known-good controller

**Tempest 3000 spinner issues:**
- Use optical mouse (not ball mouse)
- Check mouse polling rate
- Verify USB mouse is detected

## Special Notes

- Nuon protocol is timing-sensitive (uses PIO)
- Mouse input provides best Tempest 3000 experience
- Some Nuon players may have voltage differences (3.3V vs 5V)

## Product Links

- [NUON USB Adapter](https://controlleradapter.com/products/nuon-usb) - Pre-built hardware
- [GitHub Releases](https://github.com/RobertDaleSmith/USBRetro/releases) - Latest firmware

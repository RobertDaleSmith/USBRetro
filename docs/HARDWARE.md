# Hardware Compatibility

Complete list of supported USB input devices, RP2040 boards, and console outputs.

## Supported USB Input Devices

### USB Controllers

#### Xbox Controllers
- Xbox Original (Duke/S-Controller)
- Xbox 360 (wired and wireless with adapter)
- Xbox One (all revisions)
- Xbox Series X|S

**Features:**
- Full button and analog support
- Rumble feedback
- X-input protocol

#### PlayStation Controllers
- PlayStation Classic Controller
- DualShock 3 (PS3)
- DualShock 4 (PS4)
- DualSense (PS5)

**Features:**
- Full button and analog support
- Rumble feedback (DS3/DS4/DS5)
- Touchpad button (DS4/DS5)
- Adaptive trigger threshold (DualSense)

#### Nintendo Controllers
- **Switch Pro Controller** - Full support with rumble
- **Joy-Con Grip** - Dual Joy-Cons in grip mode
- **Joy-Con Single** - Individual Joy-Con support
- **GameCube Adapter** - Official Nintendo GameCube adapter (4 ports)

**Features:**
- Full button and analog support
- Rumble feedback (Pro Controller)
- Capture button support
- Home button support

#### 8BitDo Controllers

**Wireless Controllers:**
- PCEngine 2.4g Controller
- M30 2.4g Controller (Genesis/Mega Drive)
- M30 Bluetooth Controller
- NeoGeo Controller

**USB Adapters:**
- Wireless USB Adapter (Grey/Red)
- Wireless USB Adapter 2 (Black/Red)

**Features:**
- Full button support
- Analog triggers (M30)
- Turbo functionality
- Mode switching

#### Other Supported Controllers
- **Logitech Wingman Action Pad** - Classic PC gamepad
- **Sega Astrocity Mini Controller** - Arcade stick
- **Hori Pokken Tournament Controller** - Fight stick
- **Hori Horipad** - Generic Hori gamepads
- **DragonRise Generic USB** - Generic HID controllers
- **Generic DirectInput Controllers** - Most D-input gamepads
- **Generic HID Gamepads** - Standard USB HID joysticks

### USB Keyboards

**All standard USB HID keyboards supported:**
- Full key mapping to controller buttons
- GameCube: Dedicated keyboard mode (Scroll Lock/F14)
- Arrow keys → D-Pad
- WASD → Left stick
- Space/Enter → Action buttons

**Tested Keyboards:**
- Apple Magic Keyboard
- Logitech K120/K380
- Generic USB keyboards
- Mechanical keyboards (Cherry MX, etc.)

### USB Mice

**All standard USB HID mice supported:**
- Optical mice
- Laser mice
- Gaming mice with high DPI
- Trackballs

**Functionality by Console:**
- PCEngine: Mouse emulation (Afterburner II, Darius Plus)
- Nuon: Spinner emulation (Tempest 3000)
- 3DO: Mouse emulation (native 3DO mouse protocol)
- GameCube: Mouse → stick emulation

**Not Supported:**
- Scroll wheel (ignored)
- Extra mouse buttons (buttons 4+)

### USB Hubs

**Multi-player support via USB hubs:**
- Up to 8 simultaneous devices (3DO)
- Up to 5 simultaneous devices (PCEngine)
- Up to 4 simultaneous devices (Loopy, GameCube)
- Any standard USB 2.0 hub
- Powered hubs recommended for 4+ devices

**Tested Hubs:**
- Anker 4-Port USB 3.0 Hub
- Amazon Basics 7-Port USB Hub
- Generic USB 2.0 hubs

**Limitations:**
- Total current draw must not exceed USB spec
- Some controllers require more power (rumble)
- Use powered hub for 3+ high-power devices

## Supported Console Outputs

### PCEngine / TurboGrafx-16

- **Players**: Up to 5 via multitap
- **Input Types**: Controller, Mouse
- **Features**: 2/3/6-button modes, Turbo buttons
- **Protocol**: PIO-based scanning
- **Documentation**: [PCENGINE.md](consoles/PCENGINE.md)

### GameCube / Wii

- **Players**: 1 (per adapter)
- **Input Types**: Controller, Keyboard
- **Features**: Profiles, Rumble, Copilot mode
- **Protocol**: Joybus via PIO (130MHz clock required)
- **Documentation**: [GAMECUBE.md](consoles/GAMECUBE.md)

### Nuon DVD Players

- **Players**: 1
- **Input Types**: Controller, Spinner (mouse)
- **Features**: Spinner mode, In-Game Reset (IGR)
- **Protocol**: Polyface serial via PIO
- **Documentation**: [NUON.md](consoles/NUON.md)

### 3DO Interactive Multiplayer

- **Players**: Up to 8 via PBUS daisy chain
- **Input Types**: Controller, Joystick, Mouse
- **Features**: Extension passthrough, Profiles
- **Protocol**: PBUS serial via PIO
- **Documentation**: [3DO.md](consoles/3DO.md)

### Casio Loopy

- **Players**: Up to 4
- **Input Types**: Controller
- **Features**: Basic controller support
- **Protocol**: PIO-based
- **Status**: Experimental
- **Documentation**: [LOOPY.md](consoles/LOOPY.md)

### Xbox One S (Internal Mod)

- **Players**: Multiple via USB hub
- **Input Types**: Controller
- **Features**: USB passthrough to Xbox One
- **Protocol**: I2C to Xbox One controller chip
- **Status**: Hardware mod required
- **Documentation**: [XBOXONE.md](consoles/XBOXONE.md)

## Supported RP2040 Boards

### Adafruit KB2040 (Recommended)

**Default board for most products**

- **Features**: USB-C, 21 GPIO pins, boot button, WS2812 RGB LED
- **Form Factor**: Pro Micro compatible (1.3" × 0.7")
- **Products**: USB2PCE, USB2GC, USB2Nuon, USB23DO, USB2Loopy
- **Purchase**: [Adafruit](https://www.adafruit.com/product/5302)

**Why KB2040?**
- USB-C connector (modern, reversible)
- Built-in RGB LED (status indicator)
- Pro Micro footprint (fits existing designs)
- Widely available
- Good GPIO breakout

### Raspberry Pi Pico

- **Features**: Micro-USB, 26 GPIO pins, boot button
- **Form Factor**: Unique Pico layout (2.1" × 0.8")
- **Products**: All consoles supported
- **Purchase**: [Raspberry Pi](https://www.raspberrypi.com/products/raspberry-pi-pico/)

**Considerations:**
- Micro-USB (older connector)
- No built-in RGB LED
- More GPIO available
- Lower cost than KB2040

### Adafruit QT Py RP2040

- **Features**: USB-C, 11 GPIO pins, boot button, WS2812 RGB LED
- **Form Factor**: Tiny (1" × 0.7")
- **Products**: USB2XB1 (Xbox One internal mod)
- **Purchase**: [Adafruit](https://www.adafruit.com/product/4900)

**Use Cases:**
- Space-constrained applications
- Xbox One S internal mod
- Portable/embedded projects

**Limitations:**
- Fewer GPIO pins (11 vs 21)
- Harder to solder (smaller pads)

### Waveshare RP2040-Zero

- **Features**: USB-C, 20 GPIO pins, boot/reset buttons, WS2812 RGB LED
- **Form Factor**: Ultra-compact (0.9" × 0.7")
- **Products**: All consoles supported (experimental)
- **Purchase**: [Waveshare](https://www.waveshare.com/rp2040-zero.htm)

**Features:**
- Smallest RP2040 board
- USB-C connector
- Built-in RGB LED
- Castellated edges for embedding

## Board Comparison

| Board | USB | GPIO | LED | Size | Cost | Best For |
|-------|-----|------|-----|------|------|----------|
| KB2040 | USB-C | 21 | RGB | Medium | $10 | **General use (recommended)** |
| Pico | Micro | 26 | No | Large | $4 | Budget builds |
| QT Py | USB-C | 11 | RGB | Tiny | $10 | Space-limited mods |
| Waveshare | USB-C | 20 | RGB | Smallest | $6 | Embedded/experimental |

## Power Requirements

### USB Power Budget

- **USB 2.0 Port**: 500mA max
- **RP2040 Board**: ~50-100mA
- **Per Controller**: 50-500mA (varies)
- **Rumble**: +100-300mA per controller

### Recommendations

**1-2 Controllers:**
- Bus-powered USB hub OK
- No external power needed

**3+ Controllers:**
- Use powered USB hub
- Especially if using rumble
- Prevents brownouts

**High-Power Devices:**
- Xbox controllers with rumble
- PlayStation controllers with rumble
- RGB gaming peripherals
- Use powered hub

## DIY Hardware

### General Requirements

1. **RP2040 Board** (KB2040 recommended)
2. **USB Cable** (USB-C or Micro-USB)
3. **Console Connector** (specific to target console)
4. **Wires** (22-26 AWG)
5. **Soldering Iron** and solder
6. **Optional**: Level shifters, resistors, capacitors

### Console-Specific Pinouts

See individual console documentation for pinouts:
- [PCEngine Pinout](consoles/PCENGINE.md#pin-configuration)
- [GameCube Pinout](consoles/GAMECUBE.md#hardware-requirements)
- [Nuon Pinout](consoles/NUON.md#hardware-requirements)
- [3DO Pinout](consoles/3DO.md#hardware-requirements)

### Common Mistakes

- Reversed power polarity
- Wrong voltage (5V vs 3.3V)
- Cold solder joints
- Crossed data lines
- Missing pullup resistors
- Incorrect GPIO pin assignments

## Where to Buy

### RP2040 Boards

- [Adafruit](https://www.adafruit.com/) - KB2040, QT Py
- [Raspberry Pi](https://www.raspberrypi.com/) - Pico
- [Waveshare](https://www.waveshare.com/) - RP2040-Zero
- [Pimoroni](https://shop.pimoroni.com/) - Various RP2040 boards

### Pre-Built Adapters

- [Controller Adapter](https://controlleradapter.com/) - Ready-to-use products
  - USB2PCE
  - USB2GC (GCUSB)
  - USB2Nuon (NUONUSB)
  - USB23DO

### Console Connectors

- **eBay** - Replacement controller cables
- **AliExpress** - Bulk connectors
- **Console5** - Retro console parts
- **Retro Game Cave** - Specialty connectors

## Community Builds

Share your build on Discord: [discord.usbretro.com](https://discord.usbretro.com/)

See what others have built and get help with your project!

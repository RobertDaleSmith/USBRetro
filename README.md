# USBRetro

<p align="center"><img src="docs/images/USBRetro_Outline.png" width="300"/></p>
<p align="center">
  <img src="https://img.shields.io/github/license/RobertDaleSmith/USBRetro" />
  <img src="https://img.shields.io/github/actions/workflow/status/RobertDaleSmith/USBRetro/.github/workflows/build.yml" />
</p>

**Use modern USB controllers on retro consoles.** USBRetro is firmware for RP2040-based adapters that converts USB controllers, keyboards, and mice to native controller protocols for classic gaming systems.

---

## Supported Consoles

| Console | Features | Documentation |
|---------|----------|---------------|
| **PCEngine / TurboGrafx-16** | Multitap (5 players), Mouse, 2/3/6-button | [Docs](docs/consoles/PCENGINE.md) |
| **GameCube / Wii** | Profiles, Rumble, Keyboard mode | [Docs](docs/consoles/GAMECUBE.md) |
| **Nuon DVD Players** | Controller, Spinner (Tempest 3000), IGR | [Docs](docs/consoles/NUON.md) |
| **3DO Interactive Multiplayer** | 8 players, Mouse, Extension passthrough | [Docs](docs/consoles/3DO.md) |
| **Casio Loopy** | 4 players (experimental) | [Docs](docs/consoles/LOOPY.md) |
| **Xbox One S** | USB host mod, Full passthrough | [Docs](docs/consoles/XBOXONE.md) |

---

## Products

Pre-built adapters available at **[controlleradapter.com](https://controlleradapter.com)**:

- **[USB-2-PCE](https://controlleradapter.com/products/usb-2-pce)** - PCEngine/TurboGrafx-16 adapter
- **[GC USB](https://controlleradapter.com/products/gc-usb)** - GameCube/Wii adapter with profiles
- **[NUON USB](https://controlleradapter.com/products/nuon-usb)** - Nuon DVD player adapter
- **[USB-2-3DO](https://controlleradapter.com/products/usb-2-3do)** - 3DO adapter with 8-player support

---

## Supported USB Devices

**Controllers:**
- Xbox (OG/360/One/Series X|S)
- PlayStation (Classic/DS3/DS4/DualSense)
- Nintendo Switch (Pro/Joy-Cons)
- 8BitDo (PCE 2.4g, M30, Adapters)
- Generic HID gamepads

**Peripherals:**
- USB Keyboards (full HID support)
- USB Mice (PCEngine mouse, Nuon spinner, 3DO mouse)
- USB Hubs (up to 8 devices for 3DO)

**[Complete hardware compatibility list](docs/HARDWARE.md)**

---

## For Users: Updating Firmware

### Quick Flash

1. **Download** latest `.uf2` from [Releases](https://github.com/RobertDaleSmith/USBRetro/releases)
2. **Enter bootloader**:
   - **USB-2-PCE / NUON / 3DO**: Hold BOOT + connect USB-C
   - **GC USB**: Just connect USB-C (no button)
3. **Drag** `.uf2` file to `RPI-RP2` drive
4. **Done!** Drive auto-ejects when complete

**[Full installation guide](docs/INSTALLATION.md)**

---

## For Developers: Building Firmware

### Quick Start

```bash
# Install ARM toolchain (macOS)
brew install --cask gcc-arm-embedded cmake git

# Clone and initialize
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro && make init

# Build specific product
make usb2gc         # GameCube adapter
make usb2pce        # PCEngine adapter
make usb2nuon       # Nuon adapter
make usb23do        # 3DO adapter
```

### Build Commands

```bash
make init          # Initialize submodules (one-time setup)
make all           # Build all products
make clean         # Clean build artifacts

# Build specific products
make usb2pce       # USB2PCE (KB2040 + PCEngine)
make usb2gc        # USB2GC (KB2040 + GameCube)
make usb2nuon      # USB2Nuon (KB2040 + Nuon)
make usb23do       # USB23DO (KB2040 + 3DO)
make usb2loopy     # USB2Loopy (KB2040 + Casio Loopy)
make usb2xb1       # USB2XB1 (QT Py + Xbox One)
make snes23do      # SNES23DO (SNES→3DO bridge)
```

Output firmware files appear in `releases/` directory.

**[Complete build guide](docs/BUILD.md)**

---

## Documentation

- **[Installation Guide](docs/INSTALLATION.md)** - Flashing firmware, troubleshooting
- **[Hardware Compatibility](docs/HARDWARE.md)** - Supported controllers, boards, DIY builds
- **[Build Guide](docs/BUILD.md)** - Developer setup, architecture

### Console-Specific Guides

- **[GameCube/Wii](docs/consoles/GAMECUBE.md)** - Profiles, keyboard mode, rumble
- **[PCEngine/TurboGrafx-16](docs/consoles/PCENGINE.md)** - Multitap, mouse, button modes
- **[Nuon](docs/consoles/NUON.md)** - Controller, Tempest 3000 spinner, IGR
- **[3DO](docs/consoles/3DO.md)** - 8-player support, mouse, profiles
- **[Casio Loopy](docs/consoles/LOOPY.md)** - Experimental support
- **[Xbox One S](docs/consoles/XBOXONE.md)** - USB host mod installation

---

## Architecture

USBRetro uses a modular architecture:

- **RP2040** - Dual-core ARM Cortex-M0+ microcontroller
- **TinyUSB** - USB host stack for polling USB devices
- **PIO** - Programmable I/O for timing-critical console protocols
- **Dual-Core**: Core 0 handles USB input, Core 1 runs console output
- **Router** - Flexible input→output routing (SIMPLE/MERGE/BROADCAST)
- **Apps** - Per-product configuration (router, profiles, features)

**See [CLAUDE.md](CLAUDE.md) for detailed architecture**

---

## Community & Support

- **Discord**: [discord.usbretro.com](https://discord.usbretro.com/) - Community chat
- **Issues**: [GitHub Issues](https://github.com/RobertDaleSmith/USBRetro/issues) - Bug reports
- **Email**: support@controlleradapter.com - Product support

---

## Acknowledgements

- [Ha Thach](https://github.com/hathach/) - [TinyUSB](https://github.com/hathach/tinyusb)
- [David Shadoff](https://github.com/dshadoff) - [PCEMouse](https://github.com/dshadoff/PC_Engine_RP2040_Projects/tree/main/PCEMouse) foundation
- [FCare](https://github.com/FCare) - [USBTo3DO](https://github.com/FCare/USBTo3DO) 3DO protocol implementation
- [Ryzee119](https://github.com/Ryzee119) - [tusb_xinput](https://github.com/Ryzee119/tusb_xinput/)
- [SelvinPL](https://github.com/SelvinPL/) - [lufa-hid-parser](https://gist.github.com/SelvinPL/99fd9af4566e759b6553e912b6a163f9)
- [JonnyHaystack](https://github.com/JonnyHaystack/) - [joybus-pio](https://github.com/JonnyHaystack/joybus-pio)

---

## License

Copyright 2021-2025 Robert Dale Smith (ControllerAdapter.com)

Licensed under the Apache License, Version 2.0 - see [LICENSE](LICENSE) for details.

**Trademark:** The USBRetro name and branding require permission for commercial use. See [TRADEMARK.md](TRADEMARK.md) for details.

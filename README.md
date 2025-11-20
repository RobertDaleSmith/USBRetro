# USBRetro

<p align="center"><img src="docs/images/USBRetro_Outline.png" width="300"/></p>
<p align="center">
  <img src="https://img.shields.io/github/license/RobertDaleSmith/USBRetro" />
  <img src="https://img.shields.io/github/actions/workflow/status/RobertDaleSmith/USBRetro/.github/workflows/build.yml" />
</p>

**Use modern USB controllers on retro consoles.** USBRetro is firmware for RP2040-based adapters that converts USB controllers, keyboards, and mice to native controller protocols for classic gaming systems.

---

## 🎮 Supported Consoles

| Console | Features | Documentation |
|---------|----------|---------------|
| **PCEngine / TurboGrafx-16** | Multitap (5 players), Mouse, 2/3/6-button | [📖 Docs](docs/consoles/PCENGINE.md) |
| **GameCube / Wii** | Profiles, Rumble, Keyboard mode, Copilot | [📖 Docs](docs/consoles/GAMECUBE.md) |
| **Nuon DVD Players** | Standard controller, Spinner (Tempest 3000) | [📖 Docs](docs/consoles/NUON.md) |
| **Xbox One S** | USB host mod, Full passthrough | [📖 Docs](docs/consoles/XBOXONE.md) |

---

## 🛒 Products

Pre-built adapters available at **[controlleradapter.com](https://controlleradapter.com)**:

- **[USB-2-PCE](https://controlleradapter.com/products/usb-2-pce)** - PCEngine/TurboGrafx-16 adapter
- **[GC USB](https://controlleradapter.com/products/gc-usb)** - GameCube/Wii adapter with profiles
- **[NUON USB](https://controlleradapter.com/products/nuon-usb)** - Nuon DVD player adapter

---

## 🎯 Supported USB Devices

**Controllers:**
- ✅ Xbox (OG/360/One/Series X|S)
- ✅ PlayStation (Classic/DS3/DS4/DualSense)
- ✅ Nintendo Switch (Pro/Joy-Cons)
- ✅ 8BitDo (PCE 2.4g, M30, Adapters)
- ✅ Generic HID gamepads

**Peripherals:**
- ✅ USB Keyboards (full HID support)
- ✅ USB Mice (PCEngine mouse, Nuon spinner)
- ✅ USB Hubs (up to 5 devices)

👉 **[Complete hardware compatibility list](docs/HARDWARE.md)**

---

## 📥 For Users: Updating Firmware

### Quick Flash

1. **Download** latest `.uf2` from [Releases](https://github.com/RobertDaleSmith/USBRetro/releases)
2. **Enter bootloader**:
   - **USB-2-PCE / NUON**: Hold BOOT + connect USB-C
   - **GC USB**: Just connect USB-C (no button)
3. **Drag** `.uf2` file to `RPI-RP2` drive
4. **Done!** Drive auto-ejects when complete

👉 **[Full installation guide](docs/INSTALLATION.md)**

---

## 🚀 For Developers: Building Firmware

### Quick Start

```bash
# Install ARM toolchain (macOS)
brew install --cask gcc-arm-embedded cmake git

# Clone and initialize
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro && make init

# Build specific product
make gcusb         # GameCube adapter
make usb2pce       # PCEngine adapter
make nuonusb       # Nuon adapter
```

### Build Commands

```bash
make init          # Initialize submodules (one-time setup)
make build         # Build all products
make clean         # Clean build artifacts

# Build specific products
make gcusb         # GCUSB (KB2040 + GameCube)
make usb2pce       # USB2PCE (KB2040 + PCEngine)
make nuonusb       # NUONUSB (KB2040 + Nuon)
make xboxadapter   # Xbox Adapter (QT Py + Xbox One)

# Build by console (defaults to KB2040 board)
make ngc           # GameCube/Wii
make pce           # PCEngine/TurboGrafx-16
make nuon          # Nuon
make xb1           # Xbox One
```

Output firmware files appear in `releases/` directory.

👉 **[Complete build guide](docs/BUILD.md)**

---

## 📚 Documentation

- **[Installation Guide](docs/INSTALLATION.md)** - Flashing firmware, troubleshooting
- **[Hardware Compatibility](docs/HARDWARE.md)** - Supported controllers, boards, DIY builds
- **[Build Guide](docs/BUILD.md)** - Developer setup, Docker, advanced builds

### Console-Specific Guides

- **[GameCube/Wii](docs/consoles/GAMECUBE.md)** - Profiles, keyboard mode, copilot, button mappings
- **[PCEngine/TurboGrafx-16](docs/consoles/PCENGINE.md)** - Multitap, mouse, 2/3/6-button modes
- **[Nuon](docs/consoles/NUON.md)** - Standard controller, Tempest 3000 spinner
- **[Xbox One S](docs/consoles/XBOXONE.md)** - USB host mod installation

---

## 🤝 Community & Support

- **Discord**: [discord.usbretro.com](https://discord.usbretro.com/) - Community chat
- **Issues**: [GitHub Issues](https://github.com/RobertDaleSmith/USBRetro/issues) - Bug reports
- **Email**: support@controlleradapter.com - Product support

---

## 🛠️ Project Architecture

USBRetro uses:
- **RP2040** - Dual-core ARM Cortex-M0+ microcontroller
- **TinyUSB** - USB host stack (polls USB devices)
- **PIO** - Programmable I/O for timing-critical console protocols
- **Dual-Core**: Core 0 handles USB input, Core 1 runs console output

**Key Features:**
- Compile-time console selection (separate builds per console)
- Fixed-slot player management (no shifting on disconnect)
- Device registry pattern for USB controller support
- Flash persistence for settings (GameCube profiles)

👉 **See [CLAUDE.md](CLAUDE.md) for architecture details**

---

## 🙏 Acknowledgements

- [Ha Thach](https://github.com/hathach/) - [TinyUSB](https://github.com/hathach/tinyusb)
- [David Shadoff](https://github.com/dshadoff) - [PCEMouse](https://github.com/dshadoff/PC_Engine_RP2040_Projects/tree/main/PCEMouse) foundation
- [FCare](https://github.com/FCare) - [USBTo3DO](https://github.com/FCare/USBTo3DO) 3DO protocol implementation
- [Ryzee119](https://github.com/Ryzee119) - [tusb_xinput](https://github.com/Ryzee119/tusb_xinput/)
- [SelvinPL](https://github.com/SelvinPL/) - [lufa-hid-parser](https://gist.github.com/SelvinPL/99fd9af4566e759b6553e912b6a163f9)
- [JonnyHaystack](https://github.com/JonnyHaystack/) - [joybus-pio](https://github.com/JonnyHaystack/joybus-pio)

---

## 📄 License

Copyright 2021-2025 Robert Dale Smith (ControllerAdapter.com)

Licensed under the Apache License, Version 2.0 - see [LICENSE](LICENSE) for details.

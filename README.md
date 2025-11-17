# USBRetro

<p align="center"><img src="res/images/USBRetro_Outline.png" width="300"/></p>
<p align="center">
  <img src="https://img.shields.io/github/license/RobertDaleSmith/USBRetro" />
  <img src="https://img.shields.io/github/actions/workflow/status/RobertDaleSmith/USBRetro/.github/workflows/build.yml" />
</p>

**Use modern USB controllers on retro consoles.** USBRetro converts USB controllers, keyboards, and mice to native controller protocols for PCEngine, GameCube, Nuon, Xbox One, and more.

---

## 🚀 Quick Start (Developers)

**Build firmware in 3 commands:**

```bash
# 1. Install ARM toolchain (macOS)
brew install --cask gcc-arm-embedded cmake git

# 2. Clone and initialize
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro && make init

# 3. Build firmware
make build
```

Firmware files appear in `releases/` directory. Flash with `make flash`.

<details>
<summary><b>Linux/Debian Setup</b></summary>

```bash
# Install toolchain
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi git

# Clone and build
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro && make init && make build
```
</details>

<details>
<summary><b>Windows Setup</b></summary>

**Option 1: WSL2 (Recommended)**
```bash
# In WSL2 Ubuntu terminal
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi git make

# Clone and build
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro && make init && make build
```

**Option 2: Native Windows**
1. Download and install [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (14.2.rel1 or later)
2. Install [CMake](https://cmake.org/download/) and [Git](https://git-scm.com/download/win)
3. Install [Make for Windows](https://gnuwin32.sourceforge.net/packages/make.htm) or use Git Bash
4. Add toolchain to PATH: `C:\Program Files\Arm GNU Toolchain\14.2.rel1\bin`
5. Set environment variable: `PICO_TOOLCHAIN_PATH=C:\Program Files\Arm GNU Toolchain\14.2.rel1`
6. Clone and build:
```bash
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro
make init
make build
```

**Option 3: Docker Desktop**
```bash
# Build using Docker
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro
docker build -t usbretro .
```
</details>

<details>
<summary><b>Build Commands Reference</b></summary>

```bash
make init          # Initialize submodules (run once)
make build         # Build all products
make clean         # Clean build artifacts
make flash         # Flash latest firmware

# Build specific products
make usb2pce       # USB2PCE (KB2040 + PCEngine)
make gcusb         # GCUSB (KB2040 + GameCube)
make nuonusb       # NUON USB (KB2040 + Nuon)
make xboxadapter   # Xbox Adapter (QT Py + Xbox One)

# Build by console (defaults to KB2040)
make pce           # PCEngine/TurboGrafx-16
make ngc           # GameCube/Wii
make nuon          # Nuon DVD Players
make xb1           # Xbox One
make loopy         # Casio Loopy
```

See [BUILD.md](BUILD.md) for advanced build options and troubleshooting.
</details>

---

## 🎮 Supported Hardware

### USB Controllers (Input)
- ✅ Xbox Controllers (OG/360/One/Series X|S)
- ✅ PlayStation Controllers (Classic/DS3/DS4/DualSense)
- ✅ Nintendo Switch Pro Controller & Joy-Cons
- ✅ 8BitDo Controllers (PCEngine 2.4g, M30, Wireless Adapters)
- ✅ Generic HID Gamepads & Joysticks
- ✅ USB Keyboards & Mice
- ✅ USB Hubs (up to 5 devices)

### Retro Consoles (Output)
- ✅ **PCEngine/TurboGrafx-16** - Multitap (1-5 players), 2/3/6-button, Mouse
- ✅ **GameCube/Wii** - Standard controller with rumble, Keyboard mode, Copilot (4 controllers → 1)
- ✅ **Nuon DVD Players** - Standard controller, Spinner (Tempest 3000)
- ✅ **Xbox One S** - USB host mod (button/analog passthrough)
- 🚧 **Casio Loopy** - Experimental

<details>
<summary><b>Detailed Compatibility List</b></summary>

#### USB Input Devices
- [x] USB Hubs (up to 5 devices)
- [x] USB HID Keyboards/Mice (maps to controller)
- [x] X-input Controllers (Xbox OG/360/One/SeriesX|S)
- [x] D-input Controllers (generic HID gamepad|joystick)
- [x] PlayStation (PSClassic/DS3/DS4/DualSense)
- [x] Switch (SwitchPro/JoyConGrip)
- [x] 8BitDo Controllers/Adapters
    - [x] PCEngine 2.4g
    - [x] M30 2.4g/BT
    - [ ] NeoGeo 2.4g
    - [x] Wireless Adapter 1 (Grey/Red)
    - [ ] Wireless Adapter 2 (Black/Red)
- [x] Logitech Wingman Action Pad
- [x] Sega Astrocity controller/joystick
- [x] Hori Pokken and Horipad controllers
- [x] DragonRise Generic USB controllers

#### Retro Console Outputs
- [x] PCEngine/TurboGrafx-16
    - [x] Multitap (1-5 players)
    - [x] PCEngine Mouse
    - [x] 2/3/6-button Controller
- [x] Nuon DVD Players
    - [x] Standard controller
    - [x] Spinner controller (Tempest 3000)
- [x] GameCube/Wii
    - [x] Standard Controller (with rumble)
    - [x] GameCube Keyboard (scroll lock to enable)
    - [x] Copilot (combine up to 4 controllers)
- [x] Xbox One S (USB host controller mod)
    - [x] Full button/analog passthrough
    - [ ] Rumble passthrough
- [ ] CD-i (planned)
- [ ] 3DO (planned)
- [ ] Sega Dreamcast (planned)
</details>

---

## 📥 For Users: Updating Firmware

### Pre-Built Hardware

Purchase ready-to-use adapters at [controlleradapter.com](https://controlleradapter.com):
- [USB-2-PCE](https://controlleradapter.com/products/usb-2-pce) - PCEngine/TurboGrafx-16
- [GC USB](https://controlleradapter.com/products/gc-usb) - GameCube/Wii
- [NUON USB](https://controlleradapter.com/products/nuon-usb) - Nuon DVD Players

### Flashing Instructions

**USB-2-PCE / NUON USB:**
1. Download the latest `.uf2` file from [Releases](https://github.com/RobertDaleSmith/USBRetro/releases)
2. Disconnect adapter from console and all USB devices
3. Hold BOOT button while connecting USB-C to computer
4. Drag `.uf2` file to `RPI-RP2` drive
5. Drive will auto-eject when complete 🚀

**GC USB:**
1. Download the latest `usbretro_ngc.uf2` file
2. Disconnect adapter from console and all USB devices
3. Connect USB-C to computer (no BOOT button needed)
4. Drag `.uf2` file to `RPI-RP2` drive
5. Drive will auto-eject when complete 🚀

---

## 🎯 Button Mappings

### Input Map
| USBRetro    | X-input     | Switch      | PlayStation | DirectInput |
| ----------- | ----------- | ----------- | ----------- | ----------- |
| B1          | A           | B           | Cross       | 2           |
| B2          | B           | A           | Circle      | 3           |
| B3          | X           | Y           | Square      | 1           |
| B4          | Y           | X           | Triangle    | 4           |
| L1          | LB          | L           | L1          | 5           |
| R1          | RB          | R           | R1          | 6           |
| L2          | LT          | ZL          | L2          | 7           |
| R2          | RT          | ZR          | R2          | 8           |
| S1          | Back        | Minus       | Select/Share| 9           |
| S2          | Start       | Options     | Start/Option| 10          |
| L3          | LS          | LS          | L3          | 11          |
| R3          | RS          | RS          | R3          | 12          |
| A1          | Guide       | Home        | PS          | 13          |
| A2          |             | Capture     | Touchpad    | 14          |

### Output Map
| USBRetro    | PCEngine      | Nuon        | GameCube    | Xbox One    |
| ----------- | ------------- | ----------- | ----------- | ----------- |
| B1          | II            | A           | B           | A           |
| B2          | I             | C-Down      | A           | B           |
| B3          | IV (turbo II) | B           | Y           | X           |
| B4          | III (turbo I) | C-Left      | X           | Y           |
| L1          | VI            | L           | L           | LB          |
| R1          | V             | R           | R           | RB          |
| L2          |               | C-Up        | L (switch Z)| LT          |
| R2          |               | C-Right     | R (switch Z)| RT          |
| S1          | Select        | Nuon        | Z           | Back        |
| S2          | Run           | Start       | Start       | Start       |
| L3          |               |             |             | LS          |
| R3          |               |             |             | RS          |
| A1          |               |             |             | Guide       |
| A2          |               | Nuon        | Z           |             |

---

## 🤝 Community & Support

- **Discord**: Join our community at [discord.usbretro.com](https://discord.usbretro.com/)
- **Issues**: Report bugs or request features via [GitHub Issues](https://github.com/RobertDaleSmith/USBRetro/issues)
- **Documentation**: See [BUILD.md](BUILD.md) for detailed build instructions

---

## 🙏 Acknowledgements

- [Ha Thach](https://github.com/hathach/) - [TinyUSB library](https://github.com/hathach/tinyusb)
- [David Shadoff](https://github.com/dshadoff) - [PCEMouse](https://github.com/dshadoff/PC_Engine_RP2040_Projects/tree/main/PCEMouse) foundation
- [Ryzee119](https://github.com/Ryzee119) - [tusb_xinput](https://github.com/Ryzee119/tusb_xinput/) library
- [SelvinPL](https://github.com/SelvinPL/) - [lufa-hid-parser](https://gist.github.com/SelvinPL/99fd9af4566e759b6553e912b6a163f9) example
- [JonnyHaystack](https://github.com/JonnyHaystack/) - [joybus-pio](https://github.com/JonnyHaystack/joybus-pio) library

---

## 📄 License

Licensed under the MIT License - see [LICENSE](LICENSE) for details.

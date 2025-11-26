# USBRetro Build Guide

Complete guide for building USBRetro firmware on macOS, Linux, and Windows.

## Table of Contents

- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Building Firmware](#building-firmware)
- [Product Build Matrix](#product-build-matrix)
- [Troubleshooting](#troubleshooting)
- [Advanced Topics](#advanced-topics)

---

## Quick Start

**For macOS users who just want to build:**

```bash
# 1. Install ARM toolchain (one-time)
brew install --cask gcc-arm-embedded cmake git

# 2. Clone and initialize submodules
git clone https://github.com/RobertDaleSmith/USBRetro.git
cd USBRetro
make init          # Installs pico-sdk and TinyUSB as submodules

# 3. Build a product
make usb2pce       # or: usb2gc, usb2nuon, usb23do, usb2loopy
```

Output: `releases/usb2pce_usbretro_pce.uf2` (ready to flash!)

---

## Prerequisites

### macOS

#### Required Tools

1. **ARM GCC Toolchain** (Official from ARM, not Homebrew formula!)
   ```bash
   brew install --cask gcc-arm-embedded
   ```
   - Installs to: `/Applications/ArmGNUToolchain/<version>/arm-none-eabi/`
   - Current tested version: 14.2.rel1
   - **Do NOT use** `brew install arm-none-eabi-gcc` (missing newlib/nosys.specs)

2. **CMake** (3.13+ required)
   ```bash
   brew install cmake
   ```

3. **Git**
   ```bash
   brew install git
   ```

**Note**: Pico SDK and TinyUSB are included as submodules - no separate installation needed! Just run `make init` after cloning.

#### Optional Tools

- **picotool** (for UF2 inspection): `brew install picotool`
- **minicom** (for serial debugging): `brew install minicom`

### Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 build-essential git python3
```

### Windows

#### Option 1: WSL2 (Recommended)

Use WSL2 with Ubuntu and follow Linux instructions above.

#### Option 2: Native Windows

1. Download and install [Raspberry Pi Pico Windows Installer](https://github.com/raspberrypi/pico-setup-windows/releases)
2. Clone USBRetro and initialize:
   ```powershell
   git clone https://github.com/RobertDaleSmith/USBRetro.git
   cd USBRetro
   make init
   ```

---

## Building Firmware

### Build Specific Products

```bash
make usb2pce       # USB2PCE (KB2040 + PCEngine)
make usb2gc        # USB2GC (KB2040 + GameCube)
make usb2nuon      # USB2Nuon (KB2040 + Nuon)
make usb23do       # USB23DO (KB2040 + 3DO)
make usb2loopy     # USB2Loopy (KB2040 + Casio Loopy)
make usb2xb1       # USB2XB1 (QT Py + Xbox One)
make snes23do      # SNES23DO (KB2040 + SNES→3DO bridge)
```

**Output**: `releases/<product>_usbretro_<console>.uf2`

### Build All Products

```bash
make all           # Build all products
make clean         # Clean build artifacts
```

### Legacy Product Aliases

For backwards compatibility with existing scripts:

```bash
make gcusb         # Alias for usb2gc
make nuonusb       # Alias for usb2nuon
make xboxadapter   # Alias for usb2xb1
```

### Show Available Commands

```bash
make help          # Display all available targets
```

---

## Product Build Matrix

USBRetro supports multiple **board × console** combinations:

| Product | Board | Console | Command | Output File |
|---------|-------|---------|---------|-------------|
| **USB2PCE** | KB2040 | PCEngine | `make usb2pce` | `usb2pce_usbretro_pce.uf2` |
| **USB2GC** | KB2040 | GameCube | `make usb2gc` | `usb2gc_usbretro_ngc.uf2` |
| **USB2Nuon** | KB2040 | Nuon | `make usb2nuon` | `usb2nuon_usbretro_nuon.uf2` |
| **USB23DO** | KB2040 | 3DO | `make usb23do` | `usb23do_usbretro_3do.uf2` |
| **USB2Loopy** | KB2040 | Casio Loopy | `make usb2loopy` | `usb2loopy_usbretro_loopy.uf2` |
| **USB2XB1** | QT Py | Xbox One | `make usb2xb1` | `usb2xb1_usbretro_xb1.uf2` |
| **SNES23DO** | KB2040 | SNES→3DO | `make snes23do` | `snes23do_usbretro_snes3do.uf2` |

### Board Variants

| Board | PICO_BOARD Value | Notes |
|-------|------------------|-------|
| Raspberry Pi Pico | `pico` | Standard RP2040 board |
| Adafruit KB2040 | `adafruit_kb2040` | **Default for most products** |
| Adafruit QT Py RP2040 | `adafruit_qtpy_rp2040` | Used for Xbox adapter |

### Console Outputs

| Console | Features | Special Notes |
|---------|----------|---------------|
| **PCEngine** | Multitap (5 players), Mouse | PIO-based protocol |
| **GameCube** | Profiles, Rumble, Keyboard | **Requires 130MHz clock** |
| **Nuon** | Spinner, IGR (In-Game Reset) | PIO-based Polyface protocol |
| **3DO** | 8-player daisy chain, Mouse | PIO-based PBUS protocol |
| **Casio Loopy** | 4 players | Experimental |
| **Xbox One** | USB passthrough | Internal console mod |

---

## Troubleshooting

### macOS Issues

#### Error: `nosys.specs: No such file or directory`

**Cause**: Using Homebrew's `arm-none-eabi-gcc` formula (incomplete)

**Solution**: Use official ARM toolchain cask instead
```bash
brew uninstall arm-none-eabi-gcc  # Remove formula version
brew install --cask gcc-arm-embedded  # Install official version
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
```

#### Build fails with SDK errors

**Cause**: Outdated or uninitialized submodules

**Solution**: Re-initialize submodules
```bash
make init
make clean
make usb2pce  # or your target
```

### Linux Issues

#### Error: `arm-none-eabi-gcc: not found`

**Solution**: Install toolchain
```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

### General Build Issues

#### Error: CMake configuration fails

**Solution**: Clean build directory
```bash
make clean
make <target>
```

### Runtime Issues

#### Board doesn't enumerate as USB host

**Possible causes**:
1. Wrong board flashed (e.g., Pico build on KB2040)
2. USB cable is charge-only (no data)
3. Hardware issue with USB power

#### Controllers not detected

**Check**:
1. Firmware matches board (KB2040 firmware on KB2040 board)
2. USB hub is powered (if using hub)
3. Controller is compatible (see [HARDWARE.md](HARDWARE.md))

---

## Advanced Topics

### Architecture Overview

USBRetro uses a modular architecture:

```
src/
├── main.c                    # Entry point, main loop
├── core/                     # Shared firmware infrastructure
│   ├── router/               # Input→Output routing
│   ├── services/             # Players, Profiles, LEDs, etc.
│   └── output_interface.h    # Console output abstraction
├── apps/                     # Product configurations
│   ├── usb2pce/              # USB→PCEngine app
│   ├── usb2gc/               # USB→GameCube app
│   └── ...
├── usb/                      # USB input layer
│   ├── hid/                  # HID device drivers
│   └── usbh/                 # USB host coordination
└── native/                   # Console-specific code
    ├── device/               # We emulate devices for consoles
    └── host/                 # We act as console for native controllers
```

### Dual-Core Architecture

- **Core 0**: USB polling, device processing, main loop
- **Core 1**: Console output protocol (timing-critical)

### PIO State Machines

Console protocols use RP2040 PIO for precise timing:
- **PCEngine**: `plex.pio`, `clock.pio`, `select.pio`
- **Nuon**: `polyface_read.pio`, `polyface_send.pio`
- **3DO**: `sampling.pio`, `output.pio`
- **Loopy**: `loopy.pio`
- **GameCube**: External `joybus-pio` library

### Adding a New Product

1. Create `src/apps/<productname>/` directory with:
   - `app.h` - App manifest (features, routing config)
   - `app.c` - App initialization
   - `profiles.h` - Button mapping profiles (optional)

2. Add CMake target in `src/CMakeLists.txt`

3. Add Makefile target

4. Test build:
   ```bash
   make <productname>
   ```

### Memory Considerations

- Functions marked `__not_in_flash_func` are kept in SRAM for performance
- Used for timing-critical code
- RP2040 has ~264KB SRAM total

---

## Getting Help

- **Documentation**: See [README.md](../README.md), [CLAUDE.md](../CLAUDE.md)
- **Issues**: https://github.com/RobertDaleSmith/USBRetro/issues
- **Discord**: https://discord.usbretro.com/
- **SDK Docs**: https://www.raspberrypi.com/documentation/pico-sdk/

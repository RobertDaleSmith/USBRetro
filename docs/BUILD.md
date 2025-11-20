# USBRetro Build Guide

Complete guide for building USBRetro firmware on macOS, Linux, and Windows.

## Table of Contents

- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Environment Setup](#environment-setup)
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
make usb2pce       # or: gcusb, nuonusb, xboxadapter
```

Output: `releases/USB2PCE_usbretro_pce.uf2` (ready to flash!)

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
   - **⚠️ Do NOT use** `brew install arm-none-eabi-gcc` (missing newlib/nosys.specs)

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

#### Required Tools

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 build-essential git python3
```

**Note**: Pico SDK and TinyUSB are included as submodules - no separate installation needed! Just run `make init` after cloning.

### Windows

#### Option 1: WSL2 (Recommended)

Use WSL2 with Ubuntu and follow Linux instructions above.

#### Option 2: Native Windows

1. Download and install [Raspberry Pi Pico Windows Installer](https://github.com/raspberrypi/pico-setup-windows/releases)
   - Includes: ARM toolchain, CMake, Python, VS Code

2. Clone USBRetro and initialize:
   ```powershell
   git clone https://github.com/RobertDaleSmith/USBRetro.git
   cd USBRetro
   make init
   ```

**Note**: Pico SDK and TinyUSB are included as submodules - no separate installation needed!

---

## Environment Setup

### Clone and Initialize

```bash
git clone https://github.com/RobertDaleSmith/USBRetro.git
cd USBRetro
make init  # Initializes pico-sdk and TinyUSB submodules
```

The `make init` command:
- Initializes all git submodules recursively
- Checks out pico-sdk 2.2.0
- Checks out TinyUSB 0.19.0

**No need to set `PICO_SDK_PATH`** - the Makefile automatically uses the submodule at `src/lib/pico-sdk`.

### Optional: ARM Toolchain Path (macOS only)

If the ARM toolchain isn't auto-detected, add to `~/.zshrc`:

```bash
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
source ~/.zshrc
```

Linux users: toolchain is usually auto-detected from `/usr/bin`.

### Verify Setup

```bash
# Check toolchain
arm-none-eabi-gcc --version  # Should show 14.2.1 or similar

# Check cmake
cmake --version  # Should be 3.13+

# Check submodules initialized
ls src/lib/pico-sdk/src/boards
ls src/lib/tinyusb/src/host
```

---

## Building Firmware

### Method 1: Makefile (Recommended - Easiest)

The top-level Makefile provides simple commands for all build scenarios.

#### Build Specific Products

```bash
make usb2pce       # USB2PCE (KB2040 + PCEngine)
make gcusb         # GCUSB (KB2040 + GameCube)
make nuonusb       # NUON USB (KB2040 + Nuon)
make xboxadapter   # Xbox Adapter (QT Py + Xbox One)
```

**Output**: `releases/<PRODUCT>_usbretro_<console>.uf2`

#### Build All Products

```bash
make all           # Build all 4 products
# or
make releases      # Alias for 'make all'
```

**Output**: All products in `releases/` directory

#### Build by Console (defaults to KB2040 board)

```bash
make pce      # PCEngine/TurboGrafx-16
make ngc      # GameCube/Wii
make nuon     # Nuon DVD Players
make loopy    # Casio Loopy
make xb1      # Xbox One (uses QT Py board)
```

**Output**: `src/build/usbretro_<console>.uf2`

#### Clean Build

```bash
make clean    # Remove build artifacts and releases
```

#### Show Available Commands

```bash
make help     # Display all available targets
```

### Console-Only Builds

Build for specific consoles (defaults to KB2040 board):

```bash
make pce      # PCEngine/TurboGrafx-16
make ngc      # GameCube/Wii
make xb1      # Xbox One (uses QT Py)
make nuon     # Nuon DVD Players
make loopy    # Casio Loopy
```

**Available Boards**: `pico`, `kb2040`, `qtpy`
**Available Consoles**: `pce`, `ngc`, `xb1`, `nuon`, `loopy`

See `Makefile` for the complete build matrix and board-specific configurations.

**Output**:
- `src/build/usbretro_<console>.uf2`
- `releases/<PRODUCT>_usbretro_<console>.uf2`

### Method 3: Manual Build (Advanced)

For full control over the build process.

#### Step 1: Configure for Board

```bash
cd src

# Choose one:
sh build_rpi_pico.sh        # Raspberry Pi Pico
sh build_ada_kb2040.sh      # Adafruit KB2040
sh build_ada_qtpy.sh        # Adafruit QT Py RP2040
sh build_rpi_pico_debug.sh  # Raspberry Pi Pico (debug build)
```

This creates `src/build/` directory with CMake configuration.

#### Step 2: Build Console Firmware

```bash
cd build
make usbretro_<console> -j4

# Available console targets:
make usbretro_pce      # PCEngine/TurboGrafx-16
make usbretro_ngc      # GameCube/Wii
make usbretro_xb1      # Xbox One
make usbretro_nuon     # Nuon DVD Players
make usbretro_loopy    # Casio Loopy
```

**Output**: `src/build/usbretro_<console>.uf2`

#### Step 3: Flash to Board

1. Hold BOOTSEL button on RP2040 board
2. Connect USB cable
3. Board appears as `RPI-RP2` drive
4. Copy `.uf2` file to the drive
5. Board automatically reboots with new firmware

---

## Product Build Matrix

USBRetro supports multiple **board × console** combinations. Here are the official products:

| Product | Board | Console | Command | Output File |
|---------|-------|---------|---------|-------------|
| **USB2PCE** | Adafruit KB2040 | PCEngine | `make usb2pce` | `USB2PCE_usbretro_pce.uf2` |
| **GCUSB** | Adafruit KB2040 | GameCube | `make gcusb` | `GCUSB_usbretro_ngc.uf2` |
| **NUON USB** | Adafruit KB2040 | Nuon | `make nuonusb` | `NUON-USB_usbretro_nuon.uf2` |
| **Xbox Adapter** | Adafruit QT Py RP2040 | Xbox One | `make xboxadapter` | `Xbox-Adapter_usbretro_xb1.uf2` |

### Board Variants

| Board | PICO_BOARD Value | Build Script | Notes |
|-------|------------------|--------------|-------|
| Raspberry Pi Pico | `pico` | `build_rpi_pico.sh` | Standard RP2040 board |
| Adafruit KB2040 | `adafruit_kb2040` | `build_ada_kb2040.sh` | **Default for most products** |
| Adafruit QT Py RP2040 | `adafruit_qtpy_rp2040` | `build_ada_qtpy.sh` | Used for Xbox adapter |

### Console Outputs

| Console | Target | GPIO Requirements | Special Notes |
|---------|--------|-------------------|---------------|
| **PCEngine** | `usbretro_pce` | Output: 26-29 (KB2040) or 4-7 (Pico)<br>Input: 18-19 | Supports multitap (5 players), mouse |
| **GameCube** | `usbretro_ngc` | Joybus PIO | **Requires 130MHz clock!** Rumble support |
| **Xbox One** | `usbretro_xb1` | I2C slave + DAC | Board-specific GPIO configs |
| **Nuon** | `usbretro_nuon` | Polyface serial PIO | Spinner support for Tempest 3000 |
| **Loopy** | `usbretro_loopy` | Row: 26-29,18-19<br>Bit: 2-9 | ⚠️ Experimental (has TODOs) |

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

#### Error: `board_init: implicit declaration`

**Cause**: Using old pico-sdk version (< 2.0)

**Solution**: Update pico-sdk
```bash
cd $PICO_SDK_PATH
git pull
git submodule update --init lib/tinyusb
```

This was fixed in USBRetro by replacing `board_init()` with `stdio_init_all()`.

#### Error: `xinputh_init` incompatible pointer type

**Cause**: TinyUSB API changed in 0.18.0 - driver init must return `bool`

**Solution**: Already fixed in current USBRetro code. If you see this, update your clone:
```bash
git pull
git submodule update
```

#### Build is slow or hangs on "Downloading Picotool"

**Cause**: CMake is building picotool from source

**Solution**: Pre-install picotool (optional, doesn't affect firmware):
```bash
brew install picotool
```

### Linux Issues

#### Error: `arm-none-eabi-gcc: not found`

**Solution**: Install toolchain
```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

#### Error: `PICO_SDK_PATH not set`

**Solution**: Export SDK path
```bash
export PICO_SDK_PATH=~/git/pico-sdk
echo 'export PICO_SDK_PATH=~/git/pico-sdk' >> ~/.bashrc
```

### General Build Issues

#### Error: CMake configuration fails after SDK update

**Solution**: Clean build directory
```bash
make clean
# or manually:
rm -rf src/build
```

#### Error: `make: *** No rule to make target 'usbretro_pce'`

**Cause**: Not in correct directory or build not configured

**Solution**:
```bash
# Use top-level Makefile (from repo root)
make usb2pce

# OR configure manually
cd src
sh build_ada_kb2040.sh
cd build
make usbretro_pce
```

#### Warning: Multiple definitions or linking errors

**Cause**: Mixing code from different pico-sdk versions

**Solution**: Clean everything and rebuild
```bash
make clean
cd $PICO_SDK_PATH && git pull && git submodule update
cd ~/git/USBRetro && git submodule update
make usb2pce  # or your target
```

### Runtime Issues

#### Board doesn't enumerate as USB host

**Possible causes**:
1. USB host not enabled in `tusb_config.h` (should be `#define CFG_TUH_ENABLED 1`)
2. Wrong board flashed (e.g., Pico build on KB2040)
3. Hardware issue with USB power

**Debug steps**:
1. Verify correct `.uf2` file flashed
2. Check USB-C cable is data-capable
3. Try different USB controller
4. Use `minicom` to check serial output (if enabled)

#### Controllers not detected

**Check**:
1. Firmware matches board (KB2040 firmware on KB2040 board)
2. USB hub is powered (if using hub)
3. Controller is compatible (see README.md compatibility list)
4. Try different USB port on hub

---

## Advanced Topics

### Build Optimization Levels

All builds use `-O3` (maximum optimization) by default. To change:

1. Edit `src/CMakeLists.txt`:
   ```cmake
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")  # Change from -O3
   ```

2. For debug builds:
   ```bash
   cd src
   sh build_rpi_pico_debug.sh  # Uses -Og for debugging
   ```

### Custom GPIO Pin Mappings

Board-specific GPIO pins are defined in console headers:

- **PCEngine**: `src/console/pcengine/pcengine.c`
- **GameCube**: `src/console/gamecube/gamecube.c`
- **Xbox One**: `src/console/xboxone/xboxone.c`

To create custom pin mapping:
1. Edit the appropriate console `.c` file
2. Modify pin definitions
3. Rebuild firmware

### Adding New Board Support

To add support for a new RP2040 board:

1. Check if board is in pico-sdk: `ls $PICO_SDK_PATH/src/boards/include/boards/`

2. Create build script `src/build_<boardname>.sh`:
   ```bash
   #!/bin/sh
   cmake -DFAMILY=rp2040 -DPICO_BOARD=<boardname> -B build
   ```

3. Add product definition to `Makefile`:
   ```makefile
   BOARD_SCRIPT_<boardname> := build_<boardname>.sh
   PRODUCT_myproduct := <boardname> pce MyProduct
   ```

4. Test build:
   ```bash
   make pce  # or make usb2pce for KB2040 + PCEngine
   ```

### PIO Program Development

Console protocols use RP2040 PIO (Programmable I/O):

**PIO Files Location**:
- `src/console/pcengine/*.pio`
- `src/console/nuon/*.pio`
- `src/console/loopy/*.pio`
- `src/lib/joybus-pio/src/joybus.pio`

**PIO Assembly**:
- PIO programs are compiled by `pioasm` tool (auto-built by CMake)
- Generated headers: `src/build/*_pio.h`

**PIO Constraints**:
- Maximum 32 instructions per program
- Tight timing requirements (cycle-accurate)

**Debugging PIO**:
1. Use logic analyzer on GPIO pins
2. Check generated `.pio.h` file for instruction encoding
3. Verify state machine clock divisor settings

### Multi-Core Architecture

USBRetro uses both RP2040 cores:

- **Core 0**: USB polling, device processing, main loop
- **Core 1**: Console output protocol (timing-critical)

See `src/main.c` for core 0 and individual console files for core 1 launch.

### Memory Optimization

Functions marked `__not_in_flash_func` are kept in SRAM (not XIP flash):

```c
void __not_in_flash_func(process_signals)(void) {
    // Fast, always in RAM
}
```

**When to use**:
- Timing-critical code
- Frequently called functions
- Code executed during flash operations

**Constraints**:
- RP2040 has ~264KB SRAM total
- Use sparingly - SRAM is limited

### Cross-Compilation for Windows

To build on Windows without official installer:

1. Install MSYS2: https://www.msys2.org/
2. Install packages:
   ```bash
   pacman -S mingw-w64-x86_64-arm-none-eabi-gcc \
             mingw-w64-x86_64-cmake \
             git make
   ```
3. Follow standard build process

---

## Build System Architecture

### Makefile Structure

```
Makefile (top-level)
├── Product targets (usb2pce, gcusb, etc.)
│   ├── Clean build directory
│   ├── Run board script
│   └── Build console target
├── Console targets (pce, ngc, etc.)
└── Utility targets (clean, help, all)
```

### Build Flow

```
make usb2pce
    ↓
1. Clean: rm -rf src/build
    ↓
2. Configure: sh src/build_ada_kb2040.sh
    ↓
3. Build: cd src/build && make usbretro_pce -j4
    ↓
4. Copy: cp to releases/USB2PCE_usbretro_pce.uf2
```

### CMake Configuration

Each console target is a separate executable sharing common sources:

```cmake
add_executable(usbretro_pce)
target_compile_definitions(usbretro_pce PRIVATE CONFIG_PCE=1)
target_sources(usbretro_pce PUBLIC ${COMMON_SOURCES} console/pcengine/pcengine.c)
```

Console-specific code is conditionally compiled:

```c
#ifdef CONFIG_PCE
#include "console/pcengine/pcengine.h"
#endif
```

---

## Build Performance

### Typical Build Times (Apple M1)

| Target | Clean Build | Incremental |
|--------|-------------|-------------|
| Single product | ~30-40 sec | ~5-10 sec |
| All products (4×) | ~2-3 min | N/A (always clean) |

### Parallel Builds

All builds use `-j4` (4 parallel jobs) by default. To change:

```bash
# Use all CPU cores
make usbretro_pce -j$(nproc)

# Or edit Makefile MAKEFLAGS
```

---

## Getting Help

- **Documentation**: See [README.md](../README.md), [CLAUDE.md](../CLAUDE.md)
- **Issues**: https://github.com/RobertDaleSmith/USBRetro/issues
- **Discord**: https://discord.usbretro.com/
- **SDK Docs**: https://www.raspberrypi.com/documentation/pico-sdk/

---

## Changelog

### 2025-11-16 - macOS Support & Build System Modernization

- ✅ Added macOS build support with official ARM toolchain
- ✅ Created Makefile-based build system for easy product builds
- ✅ Updated for pico-sdk 2.2.0+ and TinyUSB 0.18.0
- ✅ Fixed `board_init()` → `stdio_init_all()` for latest SDK
- ✅ Fixed `xinputh_init()` return type for TinyUSB 0.18+
- ✅ Added `releases/` directory with product-named outputs
- ✅ Makefile provides flexible build targets for all products and consoles
- ✅ Added comprehensive BUILD.md documentation

### Previous Versions

- See git history for Linux/Debian build system development

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

USBRetro is firmware for RP2040-based adapters that converts USB controllers, keyboards, and mice to retro console protocols (PCEngine, GameCube, Nuon, Xbox One, Loopy). It uses TinyUSB for USB host functionality and RP2040's PIO (Programmable I/O) for console-specific timing-critical protocols.

## Build Commands

### macOS Quick Start

```bash
# One-time setup (if not already done)
brew install --cask gcc-arm-embedded cmake git
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
export PICO_SDK_PATH=~/git/pico-sdk

# Build specific products (easiest)
make usb2pce       # USB2PCE (KB2040 + PCEngine)
make gcusb         # GCUSB (KB2040 + GameCube)
make nuonusb       # NUON USB (KB2040 + Nuon)
make xboxadapter   # Xbox Adapter (QT Py + Xbox One)

# Build all products for release
make all

# Clean everything
make clean
```

Output files are created in `releases/` directory with product names (e.g., `USB2PCE_usbretro_pce.uf2`).

### Product Build Matrix

| Product | Board | Console | Command |
|---------|-------|---------|---------|
| USB2PCE | KB2040 | PCEngine | `make usb2pce` |
| GCUSB | KB2040 | GameCube | `make gcusb` |
| NUON USB | KB2040 | Nuon | `make nuonusb` |
| Xbox Adapter | QT Py | Xbox One | `make xboxadapter` |

### Build by Console Only (defaults to KB2040)

```bash
make pce      # PCEngine/TurboGrafx-16
make ngc      # GameCube/Wii (requires 130MHz clock)
make xb1      # Xbox One (uses QT Py board)
make nuon     # Nuon DVD Players
make loopy    # Casio Loopy
```

### Advanced: Custom Board + Console Combinations

```bash
./build_firmware.sh <board> <console>

# Examples:
./build_firmware.sh kb2040 pce     # KB2040 + PCEngine
./build_firmware.sh qtpy xb1       # QT Py + Xbox One
./build_firmware.sh pico ngc       # Raspberry Pi Pico + GameCube
```

Available boards: `pico`, `kb2040`, `qtpy`
Available consoles: `pce`, `ngc`, `xb1`, `nuon`, `loopy`

### Initial Setup (One-Time)

1. Install ARM toolchain (macOS):
```bash
brew install --cask gcc-arm-embedded
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
```

2. Clone USBRetro and initialize submodules:
```bash
cd ~/git
git clone https://github.com/RobertDaleSmith/usbretro.git
cd usbretro
make init  # Initializes pico-sdk 2.2.0 and TinyUSB 0.19.0
```

**Note:** TinyUSB is now a top-level submodule (not nested in pico-sdk) to keep pico-sdk clean. The Makefile automatically sets `PICO_TINYUSB_PATH` to point to the external TinyUSB.

### Rebuilding After Code Changes

The Makefile automatically cleans and rebuilds when switching boards. Just run:
```bash
make <product>    # e.g., make usb2pce
```

For manual control:
```bash
make clean        # Remove all build artifacts
cd src && rm -rf build  # Deep clean if needed
```

## Architecture

### Multi-Console Build System

USBRetro uses compile-time configuration to build different firmware variants from a shared codebase:

- **Build Targets**: Each console has a separate CMake executable target (`usbretro_pce`, `usbretro_ngc`, etc.)
- **Compile Definitions**: Console-specific code is conditionally compiled using `CONFIG_PCE`, `CONFIG_NGC`, `CONFIG_XB1`, `CONFIG_NUON`, `CONFIG_LOOPY`
- **Common Sources**: All console variants share the same USB input processing code (in `common/` and `devices/`)
- **Console Modules**: Each console has its own directory under `console/` with protocol-specific implementation

### Core Data Flow

1. **USB Input** (Core 0): TinyUSB host stack polls USB devices via `tuh_task()` in `main.c:process_signals()`
2. **Device Processing**: Device-specific drivers (in `devices/`) parse reports and call `post_globals()`
3. **Player State**: `post_globals()` updates the global `players[]` array (defined in `common/players.c`)
4. **Console Output** (Core 1): Console-specific code reads `players[]` and outputs to the retro console protocol

### Device Driver System

USBRetro uses a registry pattern for controller support:

- **DeviceInterface**: Function pointer struct defined in `devices/device_interface.h`
- **Device Registry**: Array of device interfaces in `devices/device_registry.c`
- **Detection**: Devices are matched by VID/PID via `is_device()` or descriptor parsing via `check_descriptor()`
- **Processing**: Each device implements `process()` to parse USB reports and `task()` for periodic operations (e.g., rumble)

When adding a new controller:
1. Create `devices/new_controller.c` and `devices/new_controller.h`
2. Implement the `DeviceInterface` functions
3. Add controller to `device_registry.c` enum and registration
4. Add source file to `COMMON_SOURCES` in `CMakeLists.txt`

### PIO State Machines

Console protocols use RP2040 PIO for precise timing:

- **PCEngine**: 3 PIO programs (`plex.pio`, `clock.pio`, `select.pio`) handle multitap scanning
- **GameCube**: Uses external `joybus-pio` library for bidirectional joybus protocol
- **Nuon**: `polyface_read.pio` and `polyface_send.pio` handle Nuon's serial protocol
- **Loopy**: `loopy.pio` handles Casio Loopy protocol

PIO headers are auto-generated by CMake via `pico_generate_pio_header()`. The `.pio` assembly files define timing-critical bit-banging protocols.

### Dual-Core Architecture

- **Core 0**: Runs `process_signals()` infinite loop handling USB polling, LED updates, and console-specific tasks
- **Core 1**: Launched by console-specific code (e.g., `gamecube.c`, `pcengine.c`) to handle output protocol timing

Console modules call `multicore_launch_core1()` with their output function, which continuously reads the `players[]` array.

### Button Mapping

USBRetro uses an intermediate button representation:

1. **Input Stage**: Device drivers map controller-specific buttons → USBRetro buttons (defined as `USBR_BUTTON_*` in `globals.h`)
2. **Storage**: Stored in `players[].global_buttons` and `players[].output_*` fields
3. **Output Stage**: Console code maps USBRetro buttons → console-specific outputs

See README.md for complete input/output mapping tables.

### Critical Functions

- `post_globals()`: Console-specific implementation called by device drivers to update player state
- `post_mouse_globals()`: Console-specific mouse handling
- `find_player_index()`: Maps USB device address+instance to player number (in `players.c`)
- `add_player()`: Registers new controller in `players[]` array
- `remove_players_by_address()`: Cleans up disconnected devices

### Memory Considerations

- Functions marked `__not_in_flash_func` are kept in SRAM (not XIP flash) for performance
- Used for timing-critical code like `process_signals()` and `post_globals()`
- RP2040 has limited SRAM (~264KB), so use sparingly

### External Dependencies

- **TinyUSB**: USB host stack (from pico-sdk)
- **tusb_xinput** (src/lib/tusb_xinput): X-input controller support
- **joybus-pio** (src/lib/joybus-pio): GameCube/N64 joybus protocol (used by NGC builds)

Both are git submodules.

## Development Notes

- All console firmwares are compiled with `-O3` optimization
- USB supports up to 5 simultaneous devices via hubs (`MAX_PLAYERS=5`)
- When adding console support, follow the pattern in existing `console/*/` directories
- PIO programs have strict size limits (32 instructions max per program)

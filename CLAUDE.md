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

**Note:** TinyUSB is a top-level submodule (not nested in pico-sdk) to keep pico-sdk clean. The Makefile automatically sets `PICO_TINYUSB_PATH` to point to the external TinyUSB.

### Rebuilding After Code Changes

The Makefile automatically cleans and rebuilds when switching boards. Just run:
```bash
make <product>    # e.g., make usb2pce
```

For manual control:
```bash
make clean        # Remove all build artifacts
make fullclean    # Reset to fresh clone state (removes all untracked files)
```

## Architecture

### Multi-Console Build System

USBRetro uses compile-time configuration to build different firmware variants from a shared codebase:

- **Build Targets**: Each console has a separate CMake executable target (`usbretro_pce`, `usbretro_ngc`, etc.) defined in `src/CMakeLists.txt`
- **Compile Definitions**: Console-specific code is conditionally compiled using `CONFIG_PCE`, `CONFIG_NGC`, `CONFIG_XB1`, `CONFIG_NUON`, `CONFIG_LOOPY`
- **Common Sources**: All console variants share the same USB input processing code (in `common/` and `devices/`)
- **Console Modules**: Each console has its own directory under `console/` with protocol-specific implementation
- **Board Selection**: Board-specific scripts (`src/build_*.sh`) set `PICO_BOARD` environment variable before CMake runs

### Core Data Flow

1. **USB Input** (Core 0): TinyUSB host stack polls USB devices via `tuh_task()` in `main.c:process_signals()`
2. **Device Processing**: Device-specific drivers (in `devices/`) parse reports and call `post_globals()`
3. **Player State**: `post_globals()` updates the global `players[]` array (defined in `common/players.c`)
4. **Console Output** (Core 1): Console-specific code reads `players[]` and outputs to the retro console protocol

**Key Point:** The `players[]` array is the bridge between USB input (Core 0) and console output (Core 1). Device drivers write to it, console code reads from it.

### Player Management System

**Fixed-Slot Assignment** (no shifting on disconnect):

- `players[]` array has `MAX_PLAYERS` slots (default 5)
- Empty slots marked with `dev_addr == -1`
- When a controller connects, it fills the first available slot
- When a controller disconnects, its slot is marked empty but not shifted
- Remaining players stay in their original slots (prevents mid-game position changes)

**Functions:**
- `add_player()`: Finds first empty slot, assigns USB device to it
- `find_player_index()`: Looks up player by `dev_addr` + `instance`
- `remove_players_by_address()`: Marks slot as empty, resets to neutral state
- `playersCount`: Tracks highest occupied slot + 1 (not necessarily active player count)

**Example:**
```
Initial:
players[0] = Xbox (dev_addr=1)
players[1] = PS4 (dev_addr=2)
players[2] = Switch (dev_addr=3)

Xbox disconnects:
players[0] = (empty, dev_addr=-1)  ← neutral state
players[1] = PS4 (dev_addr=2)      ← stays in slot 1
players[2] = Switch (dev_addr=3)   ← stays in slot 2

New controller reconnects:
players[0] = New controller        ← fills first empty slot
```

This behavior is critical for multi-port outputs (e.g., 4-port GameCube implementation).

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

**PIO Resource Limits:**
- RP2040 has 2 PIO blocks (PIO0, PIO1)
- 4 state machines per PIO block (8 total)
- 32 instruction memory slots per PIO block
- Programs can share instruction memory if loaded once

### Dual-Core Architecture

- **Core 0**: Runs `process_signals()` infinite loop handling USB polling, LED updates, and console-specific tasks
- **Core 1**: Launched by console-specific code (e.g., `gamecube.c`, `pcengine.c`) to handle output protocol timing

Console modules call `multicore_launch_core1()` with their output function, which continuously reads the `players[]` array.

**Synchronization:**
- No mutexes used (would introduce latency)
- Atomic reads/writes via proper memory barriers
- Some consoles use `output_exclude` flag for atomic multi-field updates

### Button Mapping

USBRetro uses an intermediate button representation:

1. **Input Stage**: Device drivers map controller-specific buttons → USBRetro buttons (defined as `USBR_BUTTON_*` in `globals.h`)
2. **Storage**: Stored in `players[].global_buttons` and `players[].output_*` fields
3. **Output Stage**: Console code maps USBRetro buttons → console-specific outputs

**Button State Fields:**
- `global_buttons`: Raw button state from USB device
- `altern_buttons`: Alternative/secondary button state (e.g., Joy-Con Grip's second controller)
- `output_buttons`: Combined/processed buttons (`global_buttons & altern_buttons`)

See README.md for complete input/output mapping tables.

### Console-Specific Post Functions

Each console implements these critical functions:

```c
void post_globals(uint8_t dev_addr, int8_t instance, uint32_t buttons,
                  uint8_t analog_1x, uint8_t analog_1y,
                  uint8_t analog_2x, uint8_t analog_2y,
                  uint8_t analog_l, uint8_t analog_r,
                  uint32_t keys, uint8_t quad_x);

void post_mouse_globals(uint8_t dev_addr, int8_t instance, uint16_t buttons,
                        uint8_t delta_x, uint8_t delta_y, uint8_t quad_x);
```

These functions:
- Update the `players[]` array with input data
- Handle device registration (calls `add_player()` on first button press)
- Apply console-specific button mappings
- Trigger output updates (e.g., PCEngine calls `update_output()`)

**Important:** Device drivers are console-agnostic. They call `post_globals()` which is implemented differently for each console variant.

### Memory Considerations

- Functions marked `__not_in_flash_func` are kept in SRAM (not XIP flash) for performance
- Used for timing-critical code like `process_signals()` and `post_globals()`
- RP2040 has limited SRAM (~264KB), so use sparingly
- Flash read latency can cause timing jitter in console protocols

### External Dependencies

Submodules are managed in `.gitmodules`:

- **pico-sdk** (`src/lib/pico-sdk`): Raspberry Pi Pico SDK (pinned to 2.2.0)
- **TinyUSB** (`src/lib/tinyusb`): USB host stack (pinned to 0.19.0, external to pico-sdk)
- **tusb_xinput** (`src/lib/tusb_xinput`): X-input controller support
- **joybus-pio** (`src/lib/joybus-pio`): GameCube/N64 joybus protocol (used by NGC builds)

**External TinyUSB Setup:**
- TinyUSB is a top-level submodule (not nested in pico-sdk)
- `PICO_TINYUSB_PATH` environment variable points to `src/lib/tinyusb`
- Keeps pico-sdk clean (no modified content in submodule)

## Development Workflow

### Adding a New Console

1. Create `src/console/<consolename>/` directory
2. Implement console protocol (PIO programs if timing-critical)
3. Implement `post_globals()` and `post_mouse_globals()` functions
4. Add console-specific `core1_entry()` for output protocol
5. Add CMake target in `src/CMakeLists.txt`:
   - Define executable: `add_executable(usbretro_<consolename>)`
   - Add compile definition: `target_compile_definitions(usbretro_<consolename> PRIVATE CONFIG_<CONSOLENAME>=1)`
   - Link sources and libraries
6. Create board-specific build script if needed
7. Add Makefile target for easy building

### Adding a New USB Device

1. Create `src/devices/<devicename>.c` and `.h`
2. Implement `DeviceInterface` functions:
   - `is_device()`: VID/PID matching
   - `check_descriptor()`: Descriptor parsing (if needed)
   - `init()`: Device initialization
   - `process()`: Parse USB reports, call `post_globals()`
   - `disconnect()`: Cleanup
   - `task()`: Periodic operations (optional)
3. Add to `device_registry.c` enum and registration
4. Add source file to `COMMON_SOURCES` in `CMakeLists.txt`
5. Test with real hardware

### Debugging

**UART Debug Output** (if enabled):
- UART pins: 12=TX, 13=RX (configurable per board)
- Set `CFG_TUSB_DEBUG` level in code
- Use `printf()` statements (avoid in timing-critical sections)

**LED Feedback:**
- WS2812 RGB LED support via `ws2812.c`
- Player number indicators via `PLAYER_LEDS[]` array
- Console-specific LED patterns

## Common Pitfalls

- **Don't shift players array on disconnect** - Use fixed slots to prevent mid-game position changes
- **Avoid mutexes in timing-critical paths** - Use lock-free synchronization where possible
- **GameCube requires 130MHz overclock** - Set in `ngc_init()` via `set_sys_clock_khz(130000, true)`
- **PIO programs have 32 instruction limit** - Optimize carefully or split into multiple programs
- **Flash XIP adds latency** - Use `__not_in_flash_func` for timing-critical code
- **USB device addresses change on reconnect** - Don't rely on `dev_addr` for persistent identification
- **Empty player slots must be checked** - Always check `players[i].dev_addr == -1` before accessing

## CI/CD

**GitHub Actions** (`.github/workflows/build.yml`):
- Builds firmware for all boards on push to `main`
- Uses Docker for consistent build environment
- Matrix builds: `rpi_pico`, `ada_qtpy`, `ada_kb2040`
- Creates releases on version bump (manual trigger via workflow_dispatch)
- Artifacts named with commit hash for dev builds
- Release artifacts named with version number

**Docker** (`Dockerfile`):
- Debian Bookworm base image
- Pre-installs ARM toolchain and build dependencies
- Runs `make init` to pin pico-sdk and TinyUSB versions
- Used by both CI and local development

## Repository Structure

```
USBRetro/
├── src/
│   ├── main.c                    # Entry point, USB polling loop
│   ├── CMakeLists.txt            # Build configuration for all console variants
│   ├── common/                   # Shared code across all consoles
│   │   ├── players.c/h          # Player management (fixed-slot array)
│   │   ├── globals.h            # USBRetro button definitions
│   │   ├── codes.c/h            # Cheat code detection
│   │   └── ws2812.c/h/.pio      # RGB LED support
│   ├── devices/                  # USB device drivers
│   │   ├── device_registry.c    # Device registration and routing
│   │   ├── device_interface.h   # Device driver interface
│   │   ├── hid_*.c              # Generic HID support
│   │   └── <vendor>_<device>.c  # Vendor-specific drivers
│   ├── console/                  # Console-specific implementations
│   │   ├── pcengine/            # PCEngine protocol + PIO programs
│   │   ├── gamecube/            # GameCube protocol (uses joybus-pio)
│   │   ├── nuon/                # Nuon protocol + PIO programs
│   │   ├── xboxone/             # Xbox One I2C passthrough
│   │   └── loopy/               # Casio Loopy protocol + PIO programs
│   ├── lib/                      # External libraries (submodules)
│   │   ├── pico-sdk/            # Raspberry Pi Pico SDK (2.2.0)
│   │   ├── tinyusb/             # TinyUSB (0.19.0, external)
│   │   ├── tusb_xinput/         # X-input controller support
│   │   └── joybus-pio/          # GameCube/N64 joybus protocol
│   └── build_*.sh               # Board-specific build scripts
├── Makefile                      # Top-level build system
├── Dockerfile                    # Docker build environment
├── .github/workflows/build.yml  # CI/CD pipeline
├── images/                       # Project images
├── docs/                         # Documentation
│   ├── BUILD.md                 # Developer build guide
│   ├── INSTALLATION.md          # User flashing guide
│   ├── HARDWARE.md              # Hardware compatibility
│   └── consoles/                # Console-specific docs
│       ├── GAMECUBE.md
│       ├── PCENGINE.md
│       ├── NUON.md
│       └── XBOXONE.md
└── README.md                     # Project overview
```

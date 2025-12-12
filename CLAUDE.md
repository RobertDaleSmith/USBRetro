# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Joypad is firmware for RP2040-based adapters that converts USB controllers, keyboards, and mice to retro console protocols (PCEngine, GameCube, Nuon, Xbox One, Loopy, 3DO). It uses TinyUSB for USB host functionality and RP2040's PIO (Programmable I/O) for console-specific timing-critical protocols.

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

Output files are created in `releases/` directory with product names (e.g., `usb2pce_joypad_pce.uf2`).

### Product Build Matrix

| Product | Board | Console | Command |
|---------|-------|---------|---------|
| USB2PCE | KB2040 | PCEngine | `make usb2pce` |
| GCUSB | KB2040 | GameCube | `make gcusb` |
| NUON USB | KB2040 | Nuon | `make nuonusb` |
| Xbox Adapter | QT Py | Xbox One | `make xboxadapter` |
| USB23DO | KB2040 | 3DO | `make usb23do` |
| SNES23DO | KB2040 | SNES→3DO | `make snes23do` |
| USB2Loopy | KB2040 | Casio Loopy | `make usb2loopy` |

### Initial Setup (One-Time)

```bash
brew install --cask gcc-arm-embedded
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
cd ~/git
git clone https://github.com/RobertDaleSmith/joypad.git
cd joypad
make init  # Initializes pico-sdk 2.2.0 and TinyUSB 0.19.0
```

## Architecture

### Repository Structure

```
src/
├── main.c                      # Entry point, main loop
├── CMakeLists.txt              # Build configuration
├── core/                       # Shared firmware infrastructure
│   ├── buttons.h               # Canonical JP_BUTTON_* definitions
│   ├── input_event.h           # Unified input event structure
│   ├── output_interface.h      # Console output abstraction
│   ├── router/                 # Input→Output routing system
│   │   ├── router.c/h          # SIMPLE/MERGE/BROADCAST modes
│   └── services/
│       ├── players/            # Player slot management + feedback
│       │   ├── manager.c/h     # Slot assignment (SHIFT/FIXED modes)
│       │   └── feedback.c/h    # Rumble/LED feedback patterns
│       ├── profile/            # Button remapping profiles
│       │   └── profile.c/h     # Profile switching, flash persistence
│       ├── codes/              # Sequence detection (Konami code, etc.)
│       ├── hotkeys/            # Button combo detection (hold/tap)
│       ├── leds/               # NeoPixel LED control
│       │   └── ws2812.c/h      # WS2812 driver
│       └── storage/            # Flash persistence
│           └── flash.c/h       # Settings storage
├── apps/                       # Product configurations
│   ├── usb2pce/               # USB→PCEngine
│   ├── usb2gc/                # USB→GameCube
│   ├── usb2nuon/              # USB→Nuon
│   ├── usb23do/               # USB→3DO
│   ├── usb2loopy/             # USB→Casio Loopy
│   ├── usb2xb1/               # USB→Xbox One
│   └── snes23do/              # SNES→3DO (native host input)
├── usb/
│   ├── usbh/                   # USB Host layer
│   │   ├── usbh.c/h           # Unified USB host (HID + X-input)
│   │   ├── hid/               # HID protocol stack
│   │   │   ├── hid.c          # HID report handling
│   │   │   ├── hid_registry.c # Device driver registry
│   │   │   └── devices/       # Device drivers
│   │   │       ├── generic/   # Generic HID (gamepad, mouse, keyboard)
│   │   │       └── vendors/   # Vendor-specific (Sony, Nintendo, etc.)
│   │   └── xinput/            # X-input protocol
│   └── usbd/                   # USB Device (placeholder)
├── native/
│   ├── device/                 # Console output protocols (we emulate devices)
│   │   ├── pcengine/          # PCEngine multitap (PIO)
│   │   ├── gamecube/          # GameCube joybus (PIO)
│   │   ├── nuon/              # Nuon polyface (PIO)
│   │   ├── 3do/               # 3DO controller (PIO)
│   │   ├── loopy/             # Casio Loopy (PIO)
│   │   └── xboxone/           # Xbox One I2C passthrough
│   └── host/                   # Native controller input (we emulate console)
│       └── snes/              # SNES controller reading
└── lib/                        # External libraries (submodules)
    ├── pico-sdk/              # Raspberry Pi Pico SDK (2.2.0)
    ├── tinyusb/               # TinyUSB (0.19.0)
    ├── tusb_xinput/           # X-input support
    └── joybus-pio/            # GameCube/N64 joybus
```

### Core Data Flow

```
Input Sources                    Router                      Output Targets
─────────────                    ──────                      ──────────────
USB HID ──────┐                                              ┌──→ PCEngine
USB X-input ──┼──→ router_submit_input() ──→ router ──→ ────┼──→ GameCube
Native SNES ──┘                              │               └──→ 3DO, etc.
                                             │
                                    profile_apply()
                                    (button remapping)
```

1. **Input**: USB drivers or native host drivers submit events via `router_submit_input()`
2. **Routing**: Router distributes to outputs based on mode (SIMPLE, MERGE, BROADCAST)
3. **Profile**: Console output code calls `profile_apply()` for button remapping
4. **Output**: Console-specific code reads via `router_get_output()` and outputs to hardware

### Key Abstractions

#### input_event_t (`core/input_event.h`)
Unified input structure for all device types:
```c
typedef struct {
    uint8_t dev_addr;           // USB device address
    int8_t instance;            // Instance number
    input_device_type_t type;   // GAMEPAD, MOUSE, KEYBOARD, etc.
    uint32_t buttons;           // Button bitmap (JP_BUTTON_*)
    uint8_t analog[8];          // Analog axes (0-255, centered at 128)
    int8_t delta_x, delta_y;    // Mouse deltas
    // ... more fields
} input_event_t;
```

#### OutputInterface (`core/output_interface.h`)
Console output abstraction - each console implements:
```c
typedef struct {
    const char* name;
    void (*init)(void);
    void (*core1_entry)(void);      // Runs on Core 1
    void (*task)(void);             // Periodic task on Core 0
    uint8_t (*get_rumble)(void);
    uint8_t (*get_player_led)(void);
    // Profile system accessors
    uint8_t (*get_profile_count)(void);
    uint8_t (*get_active_profile)(void);
    void (*set_active_profile)(uint8_t index);
    const char* (*get_profile_name)(uint8_t index);
} OutputInterface;
```

#### Router Modes (`core/router/router.h`)
- **SIMPLE**: 1:1 mapping (USB device N → output slot N)
- **MERGE**: All inputs merged to single output (Nuon single-port)
- **BROADCAST**: All inputs sent to all outputs

### Apps Layer

Each product has an app in `apps/<product>/` that configures:
- Router mode and routing
- Player slot management (SHIFT vs FIXED mode)
- Profile definitions (button remapping)

Example app structure:
```
apps/usb2gc/
├── app.c           # app_init() - configures router, players, profiles
├── app.h           # Version, config constants
├── app_config.h    # Build-time configuration
└── profiles.h      # Profile definitions (button maps)
```

### Profile System

Profiles provide button remapping with:
- Multiple named profiles per console
- SELECT + D-pad Up/Down to cycle (after 2s hold)
- Visual feedback (NeoPixel blinks)
- Haptic feedback (rumble pulses)
- Flash persistence

Consoles without `profiles.h` pass buttons through unchanged.

### Services

| Service | Purpose |
|---------|---------|
| `players/` | Player slot assignment, feedback patterns |
| `profile/` | Button remapping, profile switching |
| `codes/` | Button sequence detection (cheat codes) |
| `hotkeys/` | Button combo detection (hold/tap/release) |
| `leds/` | NeoPixel LED control |
| `storage/` | Flash persistence for settings |

### Dual-Core Architecture

- **Core 0**: Main loop (`process_signals()`) - USB polling, tasks, LED updates
- **Core 1**: Console output protocol timing (launched via `multicore_launch_core1()`)

### PIO State Machines

Console protocols use RP2040 PIO for precise timing:
- **PCEngine**: `plex.pio`, `clock.pio`, `select.pio` (multitap scanning)
- **GameCube**: `joybus.pio` (bidirectional joybus)
- **Nuon**: `polyface_read.pio`, `polyface_send.pio`
- **3DO**: `sampling.pio`, `output.pio`
- **Loopy**: `loopy.pio`

## Development Workflow

### Adding a New Console

1. Create `src/native/device/<console>/` with:
   - `<console>_device.c/h` - Protocol implementation
   - `<console>_buttons.h` - Button aliases for readability
   - `.pio` files if timing-critical

2. Implement OutputInterface:
   ```c
   const OutputInterface <console>_output_interface = {
       .name = "<Console>",
       .init = <console>_init,
       .core1_entry = core1_entry,  // or NULL
       .task = <console>_task,      // or NULL
       // ... profile accessors if using profiles
   };
   ```

3. Create `src/apps/usb2<console>/` with:
   - `app.c` - Router/player/profile configuration
   - `app.h`, `app_config.h`
   - `profiles.h` (optional)

4. Add to `CMakeLists.txt`:
   - `add_executable(joypad_<console>)`
   - `target_compile_definitions(...PRIVATE CONFIG_<CONSOLE>=1)`
   - Source files and includes

5. Add Makefile target

### Adding a New USB Device Driver

1. Create `src/usb/usbh/hid/devices/vendors/<vendor>/<device>.c/h`

2. Implement HID device interface:
   ```c
   bool <device>_is_device(uint16_t vid, uint16_t pid);
   void <device>_init(uint8_t dev_addr, uint8_t instance);
   void <device>_process(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
   void <device>_disconnect(uint8_t dev_addr, uint8_t instance);
   void <device>_task(uint8_t dev_addr, uint8_t instance, uint8_t rumble, uint8_t leds);
   ```

3. Register in `hid_registry.c`

4. Add to `COMMON_SOURCES` in `CMakeLists.txt`

### Button Mapping

Joypad uses canonical button definitions (`core/buttons.h`):
```c
#define JP_BUTTON_B1 0x00020  // A/Cross
#define JP_BUTTON_B2 0x00010  // B/Circle
#define JP_BUTTON_B3 0x02000  // X/Square
#define JP_BUTTON_B4 0x01000  // Y/Triangle
#define JP_BUTTON_L1 0x04000  // LB/L1
#define JP_BUTTON_R1 0x08000  // RB/R1
#define JP_BUTTON_S1 0x00040  // Back/Select
#define JP_BUTTON_S2 0x00080  // Start
// ... etc
```

Buttons are **active-low** (0 = pressed, 1 = released).

## Common Pitfalls

- **Buttons are active-low** - Check with `(buttons & JP_BUTTON_X) == 0` for pressed
- **GameCube requires 130MHz** - Set via `set_sys_clock_khz(130000, true)`
- **PIO has 32 instruction limit** - Optimize or split programs
- **Use `__not_in_flash_func`** - For timing-critical code to avoid XIP latency
- **Player slots** - SHIFT mode shifts on disconnect, FIXED mode preserves positions
- **Profile passthrough** - NULL profile or empty button_map passes through unchanged

## External Dependencies

Submodules in `src/lib/`:
- **pico-sdk** (2.2.0): Raspberry Pi Pico SDK
- **tinyusb** (0.19.0): USB host stack (external to pico-sdk)
- **tusb_xinput**: X-input controller support
- **joybus-pio**: GameCube/N64 joybus protocol
- **SNESpad**: SNES controller reading (for native host)

## CI/CD

GitHub Actions (`.github/workflows/build.yml`):
- Builds all products on push to `main`
- Docker-based for consistency
- Artifacts in `releases/` directory

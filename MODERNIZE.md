# USBRetro Modernization Plan

This document tracks the modernization of USBRetro to support macOS development, update dependencies, improve build system, and ensure all console outputs work correctly.

**Target Platform**: macOS (primary), Linux (secondary)
**Target SDK**: pico-sdk 2.1.1+
**Priority Console**: PCEngine (easiest to test)
**Board Variants**: Raspberry Pi Pico, Adafruit KB2040, Adafruit QT Py RP2040

---

## Current Status Assessment

### Working
- ✅ Linux (Debian) builds via Docker
- ✅ GitHub Actions CI/CD pipeline
- ✅ Multi-console build system (PCE, NGC, XB1, Nuon, Loopy)
- ✅ Device driver registry pattern
- ✅ Git submodules for dependencies

### Known Issues
- ❌ No macOS build documentation or testing
- ❌ TinyUSB version not pinned (requires specific commit or master)
- ❌ pico-sdk version not specified
- ❌ Submodule commits not tagged (hard to track versions)
- ❌ Build scripts require manual setup steps
- ❌ Board-specific GPIO pins hardcoded in headers
- ❌ Some console outputs incomplete (Loopy has multiple TODOs)

---

## Phase 1: macOS Build Support ✅ COMPLETED

**Goal**: Get USBRetro building on macOS with minimal changes

### 1.1 Install ARM Toolchain on macOS ✅
- [x] Document ARM toolchain installation (prefer official ARM release over Homebrew)
- [x] Test both Intel and Apple Silicon Macs if possible
- [x] Document PATH and environment variable setup
- [x] Create macOS-specific setup script/documentation

**Recommended Setup** (Tested on Apple Silicon):
```bash
# Install official ARM toolchain via Homebrew cask (easiest)
brew install --cask gcc-arm-embedded

# This installs to: /Applications/ArmGNUToolchain/<version>/arm-none-eabi/
# Example: /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/

# Set toolchain path for pico-sdk
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi

# Required tools
brew install cmake
brew install git
```

**⚠️ Important**: Do NOT use Homebrew's `arm-none-eabi-gcc` formula - it's missing newlib (nosys.specs)!

### 1.2 Setup Pico SDK on macOS ✅
- [x] Clone pico-sdk to standard location (e.g., `~/git/pico-sdk`)
- [x] Initialize TinyUSB submodule: `git submodule update --init lib/tinyusb`
- [x] Verified TinyUSB on latest (86ad6e56c / 0.18.0)
- [x] Set PICO_SDK_PATH environment variable
- [x] Tested with pico-sdk 2.2.0+ (latest master)

**Setup Commands**:
```bash
cd ~/git
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init lib/tinyusb
export PICO_SDK_PATH=~/git/pico-sdk
# Add to ~/.zshrc for persistence
echo 'export PICO_SDK_PATH=~/git/pico-sdk' >> ~/.zshrc
```

### 1.3 Test Initial PCEngine Build ✅
- [x] Run `src/build_rpi_pico.sh` on macOS
- [x] Run `cmake ..` in build directory
- [x] Run `make usbretro_pce`
- [x] Verify .uf2 file is generated (143K usbretro_pce.uf2)
- [x] Document macOS-specific build issues encountered
- [x] Fix all build errors

**Build Issues Encountered & Fixed**:

1. **Missing nosys.specs** (Homebrew arm-none-eabi-gcc)
   - **Problem**: Homebrew's `arm-none-eabi-gcc 15.1.0` missing newlib library
   - **Solution**: Use official ARM toolchain via cask (`gcc-arm-embedded`)

2. **board_init() removed in latest pico-sdk/TinyUSB**
   - **Problem**: `src/main.c:89` - implicit declaration of `board_init()`
   - **Solution**: Replaced with `stdio_init_all()` (pico stdlib function)
   - **File Modified**: `src/main.c` line 89

3. **xinputh_init() signature changed in TinyUSB 0.18+**
   - **Problem**: Driver init function must return `bool` instead of `void`
   - **Solution**: Changed `void xinputh_init(void)` to `bool xinputh_init(void)` with `return true;`
   - **File Modified**: `src/lib/tusb_xinput/xinput_host.c` line 253-256

**Build Command**:
```bash
export PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
cd ~/git/usbretro/src
rm -rf build
sh build_rpi_pico.sh
cd build
make usbretro_pce -j4
```

**Build Time**: ~30 seconds on Apple M1/M2

### 1.4 Validate Build Output
- [ ] Flash usbretro_pce.uf2 to Raspberry Pi Pico
- [ ] Test with USB controller on PCEngine hardware
- [ ] Verify basic functionality (button mapping, D-pad, analog)
- [ ] Test with USB hub (multi-player)
- [ ] Document any runtime issues

**Next Step**: Hardware testing with PCEngine console

---

## Phase 2: Dependency Management 📦

**Goal**: Pin dependency versions and modernize dependency handling

### 2.1 Update Pico SDK
- [ ] Research latest stable pico-sdk version (2.1.1 as of Feb 2025)
- [ ] Test build with pico-sdk 2.1.1
- [ ] Document required pico-sdk version in README.md
- [ ] Consider FetchContent approach vs. manual SDK setup
- [ ] Update CLAUDE.md with new SDK version requirements

### 2.2 Pin TinyUSB Version
- [ ] Verify current TinyUSB commit works (67cd834...)
- [ ] Test with latest TinyUSB master
- [ ] Update .gitmodules or documentation to specify TinyUSB version
- [ ] Document TinyUSB version in README.md
- [ ] Create fallback instructions if specific commit needed

### 2.3 Update Submodules
- [ ] Check tusb_xinput for updates (current: bc98a93)
- [ ] Check joybus-pio for updates (current: 39296ff on branch 'c')
- [ ] Create tags/releases for submodules if they're your forks
- [ ] Pin to specific tags instead of commits
- [ ] Test all console builds after updates

### 2.4 Document Dependencies
- [ ] Create DEPENDENCIES.md listing all external dependencies
- [ ] Include minimum versions for each dependency
- [ ] Document why specific versions are required
- [ ] Add version check script if needed

---

## Phase 3: Build System Modernization 🏗️

**Goal**: Improve CMake structure, reduce duplication, support easier multi-board builds

### 3.1 CMake Consolidation
- [ ] Create CMake utility functions for common patterns
- [ ] Extract common target configuration into function
- [ ] Reduce duplication in console target definitions
- [ ] Consider using target_link_libraries with INTERFACE libraries
- [ ] Update to CMake 3.17+ features if beneficial

### 3.2 Board Configuration System
- [ ] Move GPIO pin definitions from headers to CMake
- [ ] Create board-specific configuration files
- [ ] Generate config.h from CMake variables
- [ ] Support runtime board detection where possible
- [ ] Document board variant differences

### 3.3 Multi-Board Build Automation
- [ ] Research superbuild pattern for multiple PICO_BOARD values
- [ ] Create top-level CMakeLists.txt for multi-board builds
- [ ] Use ExternalProject_Add for each board variant
- [ ] Create `build_all.sh` script to build all variants
- [ ] Update CI/CD to use new build system

### 3.4 Build Scripts Modernization
- [ ] Combine build_*.sh scripts into single parameterized script
- [ ] Add validation checks (SDK path exists, toolchain available)
- [ ] Add clean build option
- [ ] Add verbose/debug build options
- [ ] Create cross-platform build script (macOS/Linux)

---

## Phase 4: Console Output Testing 🎮

**Goal**: Validate all console outputs work correctly with updated dependencies

### 4.1 PCEngine Testing (Priority 1)
- [ ] Build usbretro_pce for all board variants
- [ ] Test 2-button controller mode
- [ ] Test 6-button controller mode
- [ ] Test multitap (5 players)
- [ ] Test PCEngine Mouse output
- [ ] Test with various USB controllers (Xbox, PS4, Switch Pro)
- [ ] Document any issues found

### 4.2 GameCube Testing
- [ ] Build usbretro_ngc for all board variants
- [ ] Verify 130MHz clock requirement still valid
- [ ] Test standard controller output
- [ ] Test rumble functionality
- [ ] Test GameCube Keyboard mode (scroll lock)
- [ ] Test Copilot mode (4 controllers combined)
- [ ] Validate joybus-pio library compatibility

### 4.3 Xbox One Testing
- [ ] Build usbretro_xb1 for supported boards
- [ ] Test I2C communication with Xbox One controller mod
- [ ] Verify button/analog passthrough
- [ ] Note rumble passthrough status (currently incomplete)
- [ ] Test board-specific GPIO configurations

### 4.4 Nuon Testing
- [ ] Build usbretro_nuon
- [ ] Test standard controller output
- [ ] Test spinner controller (Tempest 3000)
- [ ] Review TODO at nuon.c:332 (checksum calculation)
- [ ] Document protocol quirks

### 4.5 Loopy Testing
- [ ] Build usbretro_loopy
- [ ] Review extensive TODOs in loopy.c
- [ ] Determine completion status of Loopy implementation
- [ ] Test if hardware available, document if not
- [ ] Decide whether to mark as experimental

---

## Phase 5: Code Cleanup & Organization 🧹

**Goal**: Address TODOs, refactor globals, improve code quality

### 5.1 Global Variable Refactoring
- [ ] Review recent global variable refactoring (commits 3a09309, 0206553)
- [ ] Complete button code standardization (globals.h:38 TODO)
- [ ] Add A2 button support to HID devices
- [ ] Ensure consistent button representation across all devices

### 5.2 Device Driver Improvements
- [ ] Complete DragonRise device support or remove deprecated code
- [ ] Finish 8bitdo_neo.c implementation (marked incomplete)
- [ ] Add L2/R2/L3/R3 mapping to hid_keyboard.c
- [ ] Implement A1/A2 button support for generic HID
- [ ] Improve analog change detection (hid_gamepad.c:440)

### 5.3 Console Implementation Completion
- [ ] Address PCEngine protocol TODOs (pcengine.c:397-400)
- [ ] Complete Nuon checksum calculation (nuon.c:332)
- [ ] Review and complete Loopy implementation TODOs
- [ ] Document incomplete features clearly
- [ ] Add feature flags for experimental consoles

### 5.4 Code Quality
- [ ] Run static analysis (cppcheck, clang-tidy)
- [ ] Fix any warnings with -Wall -Wextra
- [ ] Document critical functions
- [ ] Add runtime assertions for safety-critical code
- [ ] Review SRAM usage (`__not_in_flash_func` placement)

---

## Phase 6: Documentation & Developer Experience 📚

**Goal**: Make project accessible to new contributors and future you

### 6.1 README Updates
- [ ] Add macOS build instructions
- [ ] Update dependency versions
- [ ] Add troubleshooting section
- [ ] Include board variant comparison table
- [ ] Add contributor guidelines

### 6.2 CLAUDE.md Updates
- [ ] Document new build system structure
- [ ] Update dependency information
- [ ] Add macOS-specific notes
- [ ] Document board configuration system
- [ ] Update commands for new build scripts

### 6.3 Developer Documentation
- [ ] Create CONTRIBUTING.md
- [ ] Document device driver interface with examples
- [ ] Create console implementation guide
- [ ] Add PIO programming tips
- [ ] Document testing procedures

### 6.4 Automated Setup
- [ ] Create setup.sh for macOS/Linux
- [ ] Validate environment (SDK, toolchain, cmake)
- [ ] Auto-download dependencies if possible
- [ ] Interactive board variant selection
- [ ] First-build success validation

---

## Phase 7: CI/CD & Release Process 🚀

**Goal**: Improve automation and release workflow

### 7.1 GitHub Actions Updates
- [ ] Add macOS build job to CI
- [ ] Test on both Intel and ARM (M1/M2) GitHub runners
- [ ] Add build validation (file size checks, symbol checks)
- [ ] Add artifact naming with board variant and console type
- [ ] Cache dependencies for faster builds

### 7.2 Docker Improvements
- [ ] Update Dockerfile to use pinned pico-sdk version
- [ ] Pin Debian base image version
- [ ] Add multi-stage build for smaller image
- [ ] Document Docker build for local development
- [ ] Create docker-compose.yml for dev environment

### 7.3 Release Automation
- [ ] Create release tagging strategy
- [ ] Auto-generate release notes from commits
- [ ] Build all board×console combinations for releases
- [ ] Generate checksums for .uf2 files
- [ ] Update release instructions in README

---

## Success Criteria ✅

### Phase 1 Complete When:
- [ ] PCEngine firmware builds successfully on macOS
- [ ] .uf2 file flashes and runs on real hardware
- [ ] macOS build instructions documented

### Phase 2 Complete When:
- [ ] All dependencies pinned to specific versions
- [ ] Builds work with documented dependency versions
- [ ] DEPENDENCIES.md created and accurate

### Phase 3 Complete When:
- [ ] CMake files consolidated and maintainable
- [ ] Single command builds all board variants
- [ ] Board-specific configs externalized from code

### Phase 4 Complete When:
- [ ] All console outputs tested on real hardware
- [ ] Issues documented or fixed
- [ ] Experimental features clearly marked

### Phase 5 Complete When:
- [ ] All critical TODOs addressed or documented
- [ ] Code passes static analysis
- [ ] Global variable usage documented

### Phase 6 Complete When:
- [ ] New contributor can build firmware following docs
- [ ] README accurate for both macOS and Linux
- [ ] CLAUDE.md reflects new architecture

### Phase 7 Complete When:
- [ ] CI runs on macOS and Linux
- [ ] Release process documented and tested
- [ ] Docker build uses modern practices

---

## Notes & Decisions

### Design Decisions
- **Why keep multi-console builds?** Single codebase reduces maintenance, compile-time configuration is efficient for embedded
- **FetchContent vs Submodules?** TBD - test both approaches, consider ease of offline development
- **Board config in CMake vs runtime?** CMake preferred for embedded - smaller binary, compile-time optimization

### Platform-Specific Considerations
- **macOS**: May need code signing for USB device access in future macOS versions
- **Apple Silicon**: ARM-to-ARM cross-compile should be fast
- **Linux**: Keep Docker build working for CI/CD and users without SDK setup

### Future Considerations
- [ ] Add clangd/LSP configuration for better IDE support
- [ ] Consider platformio support for easier hobbyist builds
- [ ] Add automated testing with USB device emulation
- [ ] Create web-based configurator for button mapping
- [ ] Support for additional consoles (3DO, Dreamcast, CD-i as per README)

---

## Timeline Estimate

- **Phase 1**: 1-2 days (initial macOS build)
- **Phase 2**: 1-2 days (dependency updates and testing)
- **Phase 3**: 2-3 days (build system refactor)
- **Phase 4**: 3-5 days (comprehensive testing, depends on hardware availability)
- **Phase 5**: 2-4 days (code cleanup)
- **Phase 6**: 1-2 days (documentation)
- **Phase 7**: 1-2 days (CI/CD updates)

**Total**: ~2-3 weeks of focused work

---

## Getting Started

**Immediate Next Steps**:
1. Install ARM toolchain on macOS
2. Clone pico-sdk and set PICO_SDK_PATH
3. Initialize TinyUSB submodule
4. Attempt PCEngine build
5. Document any issues encountered

**First Build Command**:
```bash
# Setup (once)
export PICO_SDK_PATH=~/pico/pico-sdk

# Build
cd ~/git/USBRetro/src
sh build_rpi_pico.sh
cd build
cmake ..
make usbretro_pce
```

Expected output: `usbretro_pce.uf2` in `src/build/` directory

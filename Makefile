# USBRetro Top-Level Makefile
# Builds firmware for all product variants

# Default target
.DEFAULT_GOAL := help

# Ensure PICO_TOOLCHAIN_PATH is set
ifndef PICO_TOOLCHAIN_PATH
    # Try macOS default location
    TOOLCHAIN_PATH_MACOS := /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi
    # Try Linux/CI location (toolchain in PATH)
    TOOLCHAIN_IN_PATH := $(shell which arm-none-eabi-gcc 2>/dev/null)

    ifneq ($(wildcard $(TOOLCHAIN_PATH_MACOS)),)
        export PICO_TOOLCHAIN_PATH := $(TOOLCHAIN_PATH_MACOS)
    else ifneq ($(TOOLCHAIN_IN_PATH),)
        # Toolchain is in PATH (Linux/Docker/CI) - pico-sdk will find it automatically
        export PICO_TOOLCHAIN_PATH :=
    else
        $(error PICO_TOOLCHAIN_PATH not set and toolchain not found in PATH or at $(TOOLCHAIN_PATH_MACOS))
    endif
endif

# Use local pico-sdk submodule by default
ifndef PICO_SDK_PATH
    export PICO_SDK_PATH := $(CURDIR)/src/lib/pico-sdk
else
    # If PICO_SDK_PATH is set but doesn't exist, use submodule instead
    ifeq ($(wildcard $(PICO_SDK_PATH)),)
        $(warning PICO_SDK_PATH is set to '$(PICO_SDK_PATH)' but directory not found, using submodule instead)
        override PICO_SDK_PATH := $(CURDIR)/src/lib/pico-sdk
    endif
endif

# Use external TinyUSB (keeps pico-sdk clean)
export PICO_TINYUSB_PATH := $(CURDIR)/src/lib/tinyusb

# Board-specific build scripts
BOARD_SCRIPT_pico := boards/build_rpi_pico.sh
BOARD_SCRIPT_kb2040 := boards/build_ada_kb2040.sh
BOARD_SCRIPT_qtpy := boards/build_ada_qtpy.sh
BOARD_SCRIPT_rp2040zero := boards/build_waveshare_rp2040_zero.sh

# Console targets
CONSOLE_pce := usbretro_pce
CONSOLE_ngc := usbretro_ngc
CONSOLE_xb1 := usbretro_xb1
CONSOLE_nuon := usbretro_nuon
CONSOLE_loopy := usbretro_loopy

# Product definitions: PRODUCT_name = board console output_name
PRODUCT_usb2pce := kb2040 pce USB2PCE
PRODUCT_gcusb := kb2040 ngc GCUSB
PRODUCT_nuonusb := kb2040 nuon NUONUSB
PRODUCT_xboxadapter := qtpy xb1 Xbox-Adapter

# All products
PRODUCTS := usb2pce gcusb nuonusb xboxadapter

# Release directory
RELEASE_DIR := releases

# ANSI color codes
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m

# Help target
.PHONY: help
help:
	@echo ""
	@echo "$(BLUE)USBRetro Firmware Build System$(NC)"
	@echo "$(BLUE)==============================$(NC)"
	@echo ""
	@echo "$(GREEN)Quick Start:$(NC)"
	@echo "  make init          - Initialize submodules (run once after clone)"
	@echo "  make build         - Build all products (alias for 'make all')"
	@echo ""
	@echo "$(GREEN)Product Targets:$(NC)"
	@echo "  make usb2pce       - Build USB2PCE (KB2040 + PCEngine)"
	@echo "  make gcusb         - Build GCUSB (KB2040 + GameCube)"
	@echo "  make nuonusb       - Build NUON USB (KB2040 + Nuon)"
	@echo "  make xboxadapter   - Build Xbox Adapter (QT Py + Xbox One)"
	@echo ""
	@echo "$(GREEN)Convenience Targets:$(NC)"
	@echo "  make all           - Build all products"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make fullclean     - Reset to fresh clone state (removes all untracked files)"
	@echo "  make releases      - Build all products for release"
	@echo ""
	@echo "$(GREEN)Flash Targets:$(NC)"
	@echo "  make flash         - Flash most recently built firmware"
	@echo "  make flash-usb2pce - Flash USB2PCE firmware"
	@echo "  make flash-gcusb   - Flash GCUSB firmware"
	@echo "  make flash-nuonusb - Flash NUON USB firmware"
	@echo "  make flash-xboxadapter - Flash Xbox Adapter firmware"
	@echo ""
	@echo "$(GREEN)Console-Only Targets (uses KB2040):$(NC)"
	@echo "  make pce           - Build PCEngine firmware"
	@echo "  make ngc           - Build GameCube firmware"
	@echo "  make xb1           - Build Xbox One firmware"
	@echo "  make nuon          - Build Nuon firmware"
	@echo "  make loopy         - Build Loopy firmware"
	@echo ""
	@echo "$(GREEN)Environment:$(NC)"
	@echo "  PICO_SDK_PATH:       $(PICO_SDK_PATH)"
	@echo "  PICO_TOOLCHAIN_PATH: $(PICO_TOOLCHAIN_PATH)"
	@echo ""

# Initialize submodules (run once after cloning)
.PHONY: init
init:
	@echo "$(YELLOW)Initializing submodules...$(NC)"
	@git submodule update --init --recursive
	@echo "$(YELLOW)Checking out pico-sdk 2.2.0...$(NC)"
	@cd src/lib/pico-sdk && git checkout 2.2.0
	@echo "$(YELLOW)Checking out TinyUSB 0.19.0...$(NC)"
	@cd src/lib/tinyusb && git fetch --tags && git checkout 0.19.0
	@echo "$(GREEN)✓ Initialization complete!$(NC)"
	@echo "$(GREEN)  You can now run 'make build' or 'make all'$(NC)"
	@echo ""

# Alias for all
.PHONY: build
build: all

# Generic product build function
define build_product
	@echo "$(YELLOW)Building $1...$(NC)"
	@echo "  Board:   $(word 1,$(PRODUCT_$1))"
	@echo "  Console: $(word 2,$(PRODUCT_$1))"
	@cd src && rm -rf build
	@cd src && sh $(BOARD_SCRIPT_$(word 1,$(PRODUCT_$1)))
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_$(word 2,$(PRODUCT_$1))) -j4
	@mkdir -p $(RELEASE_DIR)
	@cp src/build/$(CONSOLE_$(word 2,$(PRODUCT_$1))).uf2 \
	    $(RELEASE_DIR)/$(word 3,$(PRODUCT_$1))_$(CONSOLE_$(word 2,$(PRODUCT_$1))).uf2
	@echo "$(GREEN)✓ $1 built successfully$(NC)"
	@echo "  Output: $(RELEASE_DIR)/$(word 3,$(PRODUCT_$1))_$(CONSOLE_$(word 2,$(PRODUCT_$1))).uf2"
	@echo ""
endef

# Product-specific targets
.PHONY: usb2pce
usb2pce:
	$(call build_product,usb2pce)

.PHONY: gcusb
gcusb:
	$(call build_product,gcusb)

.PHONY: nuonusb
nuonusb:
	$(call build_product,nuonusb)

.PHONY: xboxadapter
xboxadapter:
	$(call build_product,xboxadapter)

# Console-only targets (defaults to KB2040)
.PHONY: pce
pce:
	@echo "$(YELLOW)Building PCEngine (KB2040)...$(NC)"
	@cd src && rm -rf build && sh $(BOARD_SCRIPT_kb2040)
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_pce) -j4
	@echo "$(GREEN)✓ PCEngine built successfully$(NC)"
	@echo "  Output: src/build/$(CONSOLE_pce).uf2"
	@echo ""

.PHONY: ngc
ngc:
	@echo "$(YELLOW)Building GameCube (KB2040)...$(NC)"
	@cd src && rm -rf build && sh $(BOARD_SCRIPT_kb2040)
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_ngc) -j4
	@echo "$(GREEN)✓ GameCube built successfully$(NC)"
	@echo "  Output: src/build/$(CONSOLE_ngc).uf2"
	@echo ""

.PHONY: ngc_rp2040zero
ngc_rp2040zero:
	@echo "$(YELLOW)Building GameCube (RP2040-Zero)...$(NC)"
	@cd src && rm -rf build && sh $(BOARD_SCRIPT_rp2040zero)
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_ngc) -j4
	@echo "$(GREEN)✓ GameCube built successfully$(NC)"
	@echo "  Output: src/build/$(CONSOLE_ngc).uf2"
	@echo ""

.PHONY: xb1
xb1:
	@echo "$(YELLOW)Building Xbox One (QT Py)...$(NC)"
	@cd src && rm -rf build && sh $(BOARD_SCRIPT_qtpy)
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_xb1) -j4
	@echo "$(GREEN)✓ Xbox One built successfully$(NC)"
	@echo "  Output: src/build/$(CONSOLE_xb1).uf2"
	@echo ""

.PHONY: nuon
nuon:
	@echo "$(YELLOW)Building Nuon (KB2040)...$(NC)"
	@cd src && rm -rf build && sh $(BOARD_SCRIPT_kb2040)
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_nuon) -j4
	@echo "$(GREEN)✓ Nuon built successfully$(NC)"
	@echo "  Output: src/build/$(CONSOLE_nuon).uf2"
	@echo ""

.PHONY: loopy
loopy:
	@echo "$(YELLOW)Building Loopy (KB2040)...$(NC)"
	@cd src && rm -rf build && sh $(BOARD_SCRIPT_kb2040)
	@cd src/build && $(MAKE) --no-print-directory $(CONSOLE_loopy) -j4
	@echo "$(GREEN)✓ Loopy built successfully$(NC)"
	@echo "  Output: src/build/$(CONSOLE_loopy).uf2"
	@echo ""

# Build all products
.PHONY: all
all: $(PRODUCTS)
	@echo "$(BLUE)==============================$(NC)"
	@echo "$(GREEN)All products built!$(NC)"
	@echo "$(BLUE)==============================$(NC)"
	@ls -lh $(RELEASE_DIR)/*.uf2
	@echo ""

# Alias for all
.PHONY: releases
releases: all

# Flash target - flashes most recently built firmware
.PHONY: flash
flash:
	@if [ ! -d "/Volumes/RPI-RP2" ]; then \
		echo "$(YELLOW)⚠ RPI-RP2 drive not found at /Volumes/RPI-RP2$(NC)"; \
		echo "$(YELLOW)  Please put device in bootloader mode:$(NC)"; \
		echo "$(YELLOW)  - USB2PCE/GCUSB: Hold BOOT button while plugging in USB-C$(NC)"; \
		echo "$(YELLOW)  - Or unplug all USB devices and plug in USB-C$(NC)"; \
		exit 1; \
	fi
	@LATEST_UF2=$$(ls -t $(RELEASE_DIR)/*.uf2 2>/dev/null | head -1); \
	if [ -z "$$LATEST_UF2" ]; then \
		echo "$(YELLOW)⚠ No UF2 files found in $(RELEASE_DIR)$(NC)"; \
		echo "$(YELLOW)  Run 'make usb2pce' or another build target first$(NC)"; \
		exit 1; \
	fi; \
	echo "$(YELLOW)Flashing $$(basename $$LATEST_UF2)...$(NC)"; \
	cp "$$LATEST_UF2" /Volumes/RPI-RP2/ && \
	echo "$(GREEN)✓ Firmware flashed successfully!$(NC)" && \
	echo "$(GREEN)  Device will reboot automatically$(NC)"

# Flash specific products
.PHONY: flash-usb2pce
flash-usb2pce:
	@$(MAKE) --no-print-directory _flash FLASH_FILE=$(RELEASE_DIR)/$(word 3,$(PRODUCT_usb2pce))_$(CONSOLE_$(word 2,$(PRODUCT_usb2pce))).uf2

.PHONY: flash-gcusb
flash-gcusb:
	@$(MAKE) --no-print-directory _flash FLASH_FILE=$(RELEASE_DIR)/$(word 3,$(PRODUCT_gcusb))_$(CONSOLE_$(word 2,$(PRODUCT_gcusb))).uf2

.PHONY: flash-nuonusb
flash-nuonusb:
	@$(MAKE) --no-print-directory _flash FLASH_FILE=$(RELEASE_DIR)/$(word 3,$(PRODUCT_nuonusb))_$(CONSOLE_$(word 2,$(PRODUCT_nuonusb))).uf2

.PHONY: flash-xboxadapter
flash-xboxadapter:
	@$(MAKE) --no-print-directory _flash FLASH_FILE=$(RELEASE_DIR)/$(word 3,$(PRODUCT_xboxadapter))_$(CONSOLE_$(word 2,$(PRODUCT_xboxadapter))).uf2

# Internal flash helper
.PHONY: _flash
_flash:
	@if [ ! -d "/Volumes/RPI-RP2" ]; then \
		echo "$(YELLOW)⚠ RPI-RP2 drive not found at /Volumes/RPI-RP2$(NC)"; \
		echo "$(YELLOW)  Please put device in bootloader mode$(NC)"; \
		exit 1; \
	fi
	@if [ ! -f "$(FLASH_FILE)" ]; then \
		echo "$(YELLOW)⚠ File not found: $(FLASH_FILE)$(NC)"; \
		echo "$(YELLOW)  Build the firmware first$(NC)"; \
		exit 1; \
	fi
	@echo "$(YELLOW)Flashing $$(basename $(FLASH_FILE))...$(NC)"
	@cp "$(FLASH_FILE)" /Volumes/RPI-RP2/
	@echo "$(GREEN)✓ Firmware flashed successfully!$(NC)"
	@echo "$(GREEN)  Device will reboot automatically$(NC)"

# Clean target
.PHONY: clean
clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	@rm -rf src/build
	@rm -rf $(RELEASE_DIR)
	@echo "$(GREEN)✓ Clean complete$(NC)"
	@echo ""

# Full clean - reset to fresh clone state
.PHONY: fullclean
fullclean:
	@echo "$(YELLOW)⚠️  Full clean - resetting to fresh clone state...$(NC)"
	@echo "$(YELLOW)  This will remove all untracked files and deinitialize submodules!$(NC)"
	@rm -rf src/build
	@rm -rf $(RELEASE_DIR)
	@git clean -fdx
	@git submodule deinit -f --all
	@echo "$(GREEN)✓ Full clean complete - repository reset to fresh clone state$(NC)"
	@echo "$(GREEN)  Run 'make init' to initialize submodules and start building$(NC)"
	@echo ""

# Show current configuration
.PHONY: config
config:
	@echo "$(BLUE)Current Configuration:$(NC)"
	@echo "  PICO_SDK_PATH:       $(PICO_SDK_PATH)"
	@echo "  PICO_TOOLCHAIN_PATH: $(PICO_TOOLCHAIN_PATH)"
	@echo ""

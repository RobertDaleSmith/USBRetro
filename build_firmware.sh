#!/bin/bash
# USBRetro Firmware Build Script
# Usage: ./build_firmware.sh <board> <console>
#
# Boards: pico, kb2040, qtpy
# Consoles: pce, ngc, xb1, nuon, loopy
#
# Product shortcuts: usb2pce, gcusb, nuonusb, xboxadapter

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if PICO_TOOLCHAIN_PATH is set
if [ -z "$PICO_TOOLCHAIN_PATH" ]; then
    echo -e "${YELLOW}Warning: PICO_TOOLCHAIN_PATH not set. Attempting to find toolchain...${NC}"
    if [ -d "/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi" ]; then
        export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi"
        echo -e "${GREEN}Found toolchain at: $PICO_TOOLCHAIN_PATH${NC}"
    else
        echo -e "${RED}Error: ARM toolchain not found. Please set PICO_TOOLCHAIN_PATH${NC}"
        exit 1
    fi
fi

# Check if PICO_SDK_PATH is set
if [ -z "$PICO_SDK_PATH" ]; then
    if [ -d "$HOME/git/pico-sdk" ]; then
        export PICO_SDK_PATH="$HOME/git/pico-sdk"
    else
        echo -e "${RED}Error: PICO_SDK_PATH not set and ~/git/pico-sdk not found${NC}"
        exit 1
    fi
fi

# Function to show usage
show_usage() {
    echo "Usage: $0 <board> <console>"
    echo ""
    echo "Boards:"
    echo "  pico        - Raspberry Pi Pico"
    echo "  kb2040      - Adafruit KB2040"
    echo "  qtpy        - Adafruit QT Py RP2040"
    echo ""
    echo "Consoles:"
    echo "  pce         - PCEngine/TurboGrafx-16"
    echo "  ngc         - GameCube/Wii"
    echo "  xb1         - Xbox One"
    echo "  nuon        - Nuon DVD Players"
    echo "  loopy       - Casio Loopy"
    echo ""
    echo "Product Shortcuts:"
    echo "  usb2pce     - USB2PCE (KB2040 + PCEngine)"
    echo "  gcusb       - GCUSB (KB2040 + GameCube)"
    echo "  nuonusb     - NUON USB (KB2040 + Nuon)"
    echo "  xboxadapter - Xbox Adapter (QT Py + Xbox One)"
    echo ""
    echo "Examples:"
    echo "  $0 kb2040 pce       # Build KB2040 + PCEngine"
    echo "  $0 usb2pce          # Same as above (product shortcut)"
    echo "  $0 qtpy xb1         # Build QT Py + Xbox One"
}

# Parse product shortcuts
BOARD=""
CONSOLE=""

case "$1" in
    usb2pce)
        BOARD="kb2040"
        CONSOLE="pce"
        PRODUCT_NAME="USB2PCE"
        ;;
    gcusb)
        BOARD="kb2040"
        CONSOLE="ngc"
        PRODUCT_NAME="GCUSB"
        ;;
    nuonusb)
        BOARD="kb2040"
        CONSOLE="nuon"
        PRODUCT_NAME="NUON-USB"
        ;;
    xboxadapter)
        BOARD="qtpy"
        CONSOLE="xb1"
        PRODUCT_NAME="Xbox-Adapter"
        ;;
    pico|kb2040|qtpy)
        BOARD="$1"
        CONSOLE="$2"
        ;;
    *)
        echo -e "${RED}Error: Invalid board or product name${NC}"
        show_usage
        exit 1
        ;;
esac

# Validate console if not set by product
if [ -z "$CONSOLE" ]; then
    echo -e "${RED}Error: Console not specified${NC}"
    show_usage
    exit 1
fi

# Validate console
case "$CONSOLE" in
    pce|ngc|xb1|nuon|loopy)
        ;;
    *)
        echo -e "${RED}Error: Invalid console '${CONSOLE}'${NC}"
        show_usage
        exit 1
        ;;
esac

# Map board names to build scripts
case "$BOARD" in
    pico)
        BUILD_SCRIPT="build_rpi_pico.sh"
        BOARD_NAME="Pico"
        ;;
    kb2040)
        BUILD_SCRIPT="build_ada_kb2040.sh"
        BOARD_NAME="KB2040"
        ;;
    qtpy)
        BUILD_SCRIPT="build_ada_qtpy.sh"
        BOARD_NAME="QTPy"
        ;;
    *)
        echo -e "${RED}Error: Invalid board '${BOARD}'${NC}"
        show_usage
        exit 1
        ;;
esac

# Map console names
case "$CONSOLE" in
    pce)
        CONSOLE_NAME="PCEngine"
        ;;
    ngc)
        CONSOLE_NAME="GameCube"
        ;;
    xb1)
        CONSOLE_NAME="XboxOne"
        ;;
    nuon)
        CONSOLE_NAME="Nuon"
        ;;
    loopy)
        CONSOLE_NAME="Loopy"
        ;;
esac

# Set product name if not already set
if [ -z "$PRODUCT_NAME" ]; then
    PRODUCT_NAME="${BOARD_NAME}-${CONSOLE_NAME}"
fi

echo -e "${GREEN}================================================${NC}"
echo -e "${GREEN}Building USBRetro Firmware${NC}"
echo -e "${GREEN}================================================${NC}"
echo -e "Product:  ${YELLOW}${PRODUCT_NAME}${NC}"
echo -e "Board:    ${BOARD_NAME}"
echo -e "Console:  ${CONSOLE_NAME}"
echo -e "Target:   usbretro_${CONSOLE}"
echo -e "${GREEN}================================================${NC}"
echo ""

# Change to src directory
cd "$(dirname "$0")/src"

# Clean build directory
echo -e "${YELLOW}Cleaning build directory...${NC}"
rm -rf build

# Run board-specific build script
echo -e "${YELLOW}Configuring for ${BOARD_NAME}...${NC}"
sh "$BUILD_SCRIPT"

# Build the firmware
echo -e "${YELLOW}Building firmware...${NC}"
cd build
make usbretro_${CONSOLE} -j4

# Check if build succeeded
if [ -f "usbretro_${CONSOLE}.uf2" ]; then
    FILESIZE=$(ls -lh "usbretro_${CONSOLE}.uf2" | awk '{print $5}')
    echo ""
    echo -e "${GREEN}================================================${NC}"
    echo -e "${GREEN}Build Successful!${NC}"
    echo -e "${GREEN}================================================${NC}"
    echo -e "Output: ${YELLOW}src/build/usbretro_${CONSOLE}.uf2${NC} (${FILESIZE})"

    # Optionally copy to release directory with product name
    if [ -n "$PRODUCT_NAME" ]; then
        mkdir -p ../../releases
        cp "usbretro_${CONSOLE}.uf2" "../../releases/${PRODUCT_NAME}_usbretro_${CONSOLE}.uf2"
        echo -e "Copied: ${YELLOW}releases/${PRODUCT_NAME}_usbretro_${CONSOLE}.uf2${NC}"
    fi

    echo -e "${GREEN}================================================${NC}"
    echo ""
    echo "Ready to flash to your ${BOARD_NAME}!"
    echo ""
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

#!/bin/bash
# Build all USBRetro product variants
# This script builds firmware for all official USBRetro products

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Building All USBRetro Products${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Array of products to build
# Format: "product_name:board:console"
PRODUCTS=(
    "usb2pce:kb2040:pce"
    "gcusb:kb2040:ngc"
    "nuonusb:kb2040:nuon"
    "xboxadapter:qtpy:xb1"
)

# Build each product
SUCCESS_COUNT=0
FAIL_COUNT=0

for product_spec in "${PRODUCTS[@]}"; do
    IFS=':' read -r product board console <<< "$product_spec"

    echo -e "${YELLOW}Building ${product}...${NC}"

    if ./build_firmware.sh "$product"; then
        ((SUCCESS_COUNT++))
        echo -e "${GREEN}✓ ${product} built successfully${NC}"
    else
        ((FAIL_COUNT++))
        echo -e "${RED}✗ ${product} build failed${NC}"
    fi

    echo ""
done

# Summary
echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Build Summary${NC}"
echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}Successful: ${SUCCESS_COUNT}${NC}"
if [ $FAIL_COUNT -gt 0 ]; then
    echo -e "${RED}Failed: ${FAIL_COUNT}${NC}"
fi
echo ""

if [ -d "releases" ]; then
    echo "Release files created:"
    ls -lh releases/*.uf2
fi

echo -e "${BLUE}================================================${NC}"

# Xbox One S USB Host Mod

USB controller passthrough for Xbox One S consoles via internal USB host mod.

## Overview

This firmware enables Xbox One S to accept any USB controller by replacing the internal wireless board with an RP2040 adapter. The adapter acts as both USB host (for external controllers) and Xbox One controller (to the console).

**⚠️ Hardware Modification Required** - This requires opening the Xbox One S console and replacing the wireless module.

## Features

- ✅ Full button and analog passthrough
- ✅ All standard USB controllers supported
- ✅ Multiple controller support via USB hub
- ⚠️ Rumble passthrough not yet implemented

## Button Mappings

Direct 1:1 passthrough for Xbox controllers:

| USB Input | Xbox One Output |
|-----------|-----------------|
| B1 (A) | A |
| B2 (B) | B |
| B3 (X) | X |
| B4 (Y) | Y |
| L1 (LB) | LB |
| R1 (RB) | RB |
| L2 (LT) | LT (analog) |
| R2 (RT) | RT (analog) |
| S1 (Back/View) | View |
| S2 (Start/Menu) | Menu |
| L3 (LS) | LS |
| R3 (RS) | RS |
| A1 (Guide) | Xbox |
| D-Pad | D-Pad |
| Left Stick | Left Stick |
| Right Stick | Right Stick |

### Non-Xbox Controllers

Controllers are remapped to Xbox layout:
- PlayStation → Xbox button mapping
- Switch Pro → Xbox button mapping
- Generic HID → Best-effort mapping

## Hardware Requirements

- **Board**: Adafruit QT Py RP2040 (small form factor required)
- **Target**: Xbox One S internal USB header
- **Power**: 5V from Xbox One S internal USB
- **Protocol**: I2C passthrough to Xbox One controller logic

## Installation

**⚠️ WARNING**: This modification requires:
- Opening Xbox One S console (voids warranty)
- Soldering to internal USB header
- Basic electronics knowledge
- Risk of console damage if done incorrectly

**Not recommended for beginners**

### Steps:
1. Open Xbox One S console
2. Locate wireless board and internal USB header
3. Remove wireless board
4. Install QT Py RP2040 with adapter PCB
5. Connect to internal USB header
6. Flash firmware
7. Reassemble and test

## Protocol Details

- **Communication**: I2C between RP2040 and Xbox One controller chip
- **Update Rate**: 125Hz (8ms latency)
- **Analog Resolution**: 16-bit for triggers and sticks
- **Rumble**: Protocol reverse-engineered but not yet implemented

## Limitations

- **No wireless** - Wired USB controllers only
- **No rumble** - Rumble passthrough not yet implemented
- **Single console** - Each mod requires dedicated adapter
- **Warranty void** - Hardware modification voids console warranty

## Troubleshooting

**Console doesn't detect controller:**
- Verify I2C connections
- Check 5V power supply
- Reflash firmware
- Test adapter outside console first

**Buttons not working:**
- Check button mapping
- Verify USB controller is detected
- Check USB data lines

**Analog sticks drifting:**
- Calibrate USB controller first
- Check for loose connections
- Verify USB controller works on PC

**Console won't boot:**
- Check all connections
- Verify no shorts on I2C lines
- Restore original wireless board

## Future Development

- [ ] Rumble passthrough
- [ ] Wireless controller support via Bluetooth module
- [ ] Multiple controller support via USB hub
- [ ] Adapter PCB design for easier installation

## Safety Warnings

- ⚠️ Opening console voids warranty
- ⚠️ Risk of damaging console if done incorrectly
- ⚠️ Work in static-safe environment
- ⚠️ Disconnect power before working on console
- ⚠️ Not responsible for damaged consoles

## Product Links

- **Pre-built hardware**: Not currently available (DIY only)
- [GitHub Releases](https://github.com/RobertDaleSmith/USBRetro/releases) - Latest firmware
- [Discord](https://discord.usbretro.com/) - Community support for mod

---

**Note**: This mod is experimental and requires advanced technical skills. Proceed at your own risk.

# SNES to USB Adapter (KB2040)

SNES/NES controllers to USB HID gamepad via Adafruit KB2040.

## Parts Needed

- [Adafruit KB2040](https://www.adafruit.com/product/5302) (~$10)
- SNES controller extension cable (cut to expose wires)
- Hookup wire (22-26 AWG), soldering iron

## SNES Controller Connector

The SNES controller port is a 7-pin connector. Cut an extension cable and wire the console-end plug to the KB2040.

> 🚨 **The connector pin numbering below is UNVERIFIED — do not solder against it yet.**
> The KB2040 GPIO assignments in the wiring table *are* verified against firmware
> (`src/apps/snes2usb/app.h:37-41`), but nothing in this repository states which **physical pin of
> the SNES connector** each signal lands on — `docs/input/snes.md` has no pinout section, and the
> firmware only knows GPIO numbers. This table was not checked against hardware.
> Verify continuity with a multimeter against a known-good SNES pinout before connecting +5V.
> See `a15980f` for why this repo treats unverified connector diagrams as a safety issue.

```
  ___________
 /  1 2 3 4  \
|  5 6 7      |
 \____________/

 1 = +5V        5 = DATA (serial data from controller)     [UNVERIFIED]
 2 = CLOCK      6 = LATCH (active-high strobe)              [UNVERIFIED]
 3 = IOBIT      7 = GND                                     [UNVERIFIED]
 4 = DATA1
```

## Wiring

| KB2040 GPIO | SNES Pin | Signal | Direction |
|-------------|----------|--------|-----------|
| GPIO 5 | 2 | CLOCK | Output to controller |
| GPIO 6 | 6 | LATCH | Output to controller |
| GPIO 7 | 5 | DATA0 | Input from controller |
| GPIO 8 | 4 | DATA1 | Input (multitap/keyboard) |
| GPIO 9 | 3 | IOBIT | Output (mouse/keyboard) |
| 5V | 1 | +5V | Power to controller |
| GND | 7 | GND | Ground |

## Build and Flash

```bash
# Build
make snes2usb_kb2040

# Flash: hold BOOTSEL while connecting USB, or double-tap reset
make flash-snes2usb_kb2040
```

Output file: `releases/joypad_<commit>_snes2usb_kb2040.uf2`

## Testing

1. Connect the SNES extension cable between the KB2040 and a SNES controller
2. Plug the KB2040 into a PC via USB
3. The controller appears as a USB HID gamepad
4. Open a gamepad tester and verify D-pad, A/B/X/Y, L/R, Start/Select all register
5. If using a SNES mouse, verify mouse movement and button clicks

## Supported Devices

| Device | Support |
|--------|---------|
| SNES controller | Full (12 buttons + D-pad) |
| NES controller | Full (auto-detected, 8 buttons + D-pad) |
| SNES mouse | Supported (requires IOBIT wiring) |
| Xband keyboard | Supported (requires DATA1 + IOBIT wiring) |

## Notes

- No CPU overclock needed (standard 125 MHz)
- The NeoPixel LED indicates connection status (solid = connected, blink = idle)
- See [SNES input docs](../../input/snes.md) for protocol details and button mapping
- See [snes2usb app docs](../../apps/snes2usb.md) for feature details

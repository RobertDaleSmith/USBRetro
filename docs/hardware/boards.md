# Supported Boards

Joypad OS runs on RP2040, ESP32-S3, and nRF52840 microcontrollers.

## RP2040 Boards

### Adafruit KB2040 (Recommended)

**Default board for most console adapters**

- **Features**: USB-C, 21 GPIO pins, boot button, WS2812 RGB LED
- **Form Factor**: Pro Micro compatible (1.3" x 0.7")
- **Apps**: USB2PCE, USB2GC, USB2Nuon, USB2DC, USB2Loopy, USB2Neo
- **Purchase**: [Adafruit](https://www.adafruit.com/product/5302)

**Why KB2040?**
- USB-C connector (modern, reversible)
- Built-in RGB LED (status indicator)
- Pro Micro footprint (fits existing designs)
- Widely available
- Good GPIO breakout

### Raspberry Pi Pico / Pico W / Pico 2 W

- **Features**: Micro-USB (Pico) or USB-C (Pico 2 W), 26 GPIO pins, boot button
- **Form Factor**: Unique Pico layout (2.1" x 0.8")
- **Apps**: All console adapters, bt2usb (Pico W/2 W), wifi2usb (Pico W/2 W)
- **Purchase**: [Raspberry Pi](https://www.raspberrypi.com/products/raspberry-pi-pico/)

**Pico W / Pico 2 W** have built-in Bluetooth (Classic BT + BLE) and WiFi — no dongle needed for wireless controllers or WiFi apps.

### Adafruit Feather RP2040 USB Host

- **Features**: USB-C (device) + USB-A (host), 21 GPIO pins, boot button, RGB LED
- **Form Factor**: Feather (2.0" x 0.9")
- **Apps**: usb2usb (USB passthrough)
- **Purchase**: [Adafruit](https://www.adafruit.com/product/5723)

**Use Cases:**
- USB passthrough applications
- Built-in USB-A host port (no wiring needed)
- USB → USB conversion

### Waveshare RP2040-Zero

- **Features**: USB-C, 20 GPIO pins, boot/reset buttons, WS2812 RGB LED
- **Form Factor**: Ultra-compact (0.9" x 0.7")
- **Apps**: USB23DO, USB2DC, USB2Neo, usb2usb
- **Purchase**: [Waveshare](https://www.waveshare.com/rp2040-zero.htm)

**Features:**
- Smallest RP2040 board
- USB-C connector
- Built-in RGB LED
- Castellated edges for embedding

### Adafruit MacroPad RP2040

- **Features**: 12 mechanical key switches, rotary encoder, OLED display, RGB LEDs
- **Form Factor**: MacroPad (3.4" x 2.8")
- **Apps**: controller_macropad
- **Purchase**: [Adafruit](https://www.adafruit.com/product/5128)

**Use Cases:**
- Custom macro pad controller
- Stream deck alternative
- Fighting game button box

## ESP32-S3 Boards

### Seeed XIAO ESP32-S3

- **Features**: USB-C, BLE, WiFi, 11 GPIO pins, boot button, user LED
- **Form Factor**: Ultra-compact (0.84" x 0.70")
- **Apps**: bt2usb (BLE to USB adapter)
- **Purchase**: [Seeed Studio](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)

**Notes:**
- User LED on GPIO 21 (active low)
- BLE only (no Classic Bluetooth)
- Requires ESP-IDF to build (see [ESP32-S3 platform docs](../platforms/esp32.md))

## nRF52840 Boards

### Seeed XIAO nRF52840 (BLE Sense)

- **Features**: USB-C, BLE, 11 GPIO pins, user LED, IMU, microphone
- **Form Factor**: Ultra-compact (0.84" x 0.70")
- **Apps**: bt2usb (BLE to USB adapter), usb2usb (with MAX3421E FeatherWing)
- **Purchase**: [Seeed Studio](https://www.seeedstudio.com/Seeed-XIAO-BLE-nRF52840-p-5201.html)

**Notes:**
- BLE only (no Classic Bluetooth)
- Requires nRF Connect SDK to build (see [nRF52840 platform docs](../platforms/nrf52840.md))
- UF2 bootloader for drag-and-drop flashing

### Adafruit Feather nRF52840 Express

- **Features**: USB-C, BLE, 21 GPIO pins, LiPo charging, RGB LED
- **Form Factor**: Feather (2.0" x 0.9")
- **Apps**: bt2usb, usb2usb (with MAX3421E FeatherWing)
- **Purchase**: [Adafruit](https://www.adafruit.com/product/4062)

**Notes:**
- Compatible with Feather-format accessories (FeatherWings)
- UF2 bootloader for drag-and-drop flashing
- BLE only

### nice!nano / nRF52840 Pro Micro / SuperMini

- **Features**: USB-C, BLE, Pro Micro pinout, LiPo charging, single status LED (P0.15)
- **Form Factor**: Pro Micro (1.3" x 0.7")
- **Apps**: bt2usb, universal (GPIO pad + BLE + USB)
- **Clones**: [nrfmicro Alternatives](https://github.com/joric/nrfmicro/wiki/Alternatives)

**Notes:**
- Requires the Adafruit nRF52 UF2 bootloader with SoftDevice s140 6.1.1 (the nice!nano bootloader, drive `NICENANO`)
- **Untested on hardware.** See [nRF52840 platform docs](../platforms/nrf52840.md#nicenano-nrf52840-pro-micro-supermini)
- BLE only

## Board Comparison

### RP2040

| Board | USB | GPIO | LED | Size | Cost | Best For |
|-------|-----|------|-----|------|------|----------|
| KB2040 | USB-C | 21 | RGB | Medium | $10 | **General use (recommended)** |
| Pico | Micro | 26 | No | Large | $4 | Budget builds |
| Pico W | Micro | 26 | No | Large | $6 | Bluetooth/WiFi (built-in) |
| Feather USB Host | USB-C+A | 21 | RGB | Medium | $12 | USB passthrough |
| MacroPad | USB-C | 12 keys | RGB | Large | $30 | Custom controllers |
| RP2040-Zero | USB-C | 20 | RGB | Smallest | $6 | Compact/embedded |

### Cross-Platform

| Platform | Board | BT | WiFi | USB Host | Best For |
|----------|-------|-----|------|----------|----------|
| RP2040 | KB2040 | Via dongle | No | PIO-USB | Console adapters |
| RP2040 | Pico W | Classic+BLE | Yes | PIO-USB | BT/WiFi adapters |
| ESP32-S3 | XIAO ESP32-S3 | BLE only | Yes | No | Compact BLE adapter |
| nRF52840 | XIAO nRF52840 | BLE only | No | Via MAX3421E | Compact BLE adapter |
| nRF52840 | Feather nRF52840 | BLE only | No | Via MAX3421E | BLE + USB host |
| nRF52840 | nice!nano / Pro Micro | BLE only | No | No | Cheap DIY controller |

## Power Requirements

### USB Power Budget

- **USB 2.0 Port**: 500mA max
- **RP2040 Board**: ~50-100mA
- **Per Controller**: 50-500mA (varies)
- **Rumble**: +100-300mA per controller

### Recommendations

**1-2 Controllers:**
- Bus-powered USB hub OK
- No external power needed

**3+ Controllers:**
- Use powered USB hub
- Especially if using rumble
- Prevents brownouts

**High-Power Devices:**
- Xbox controllers with rumble
- PlayStation controllers with rumble
- RGB gaming peripherals
- Use powered hub

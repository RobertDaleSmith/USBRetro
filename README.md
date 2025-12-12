# Joypad Core

**Universal controller firmware foundation for adapters, controllers, and input systems.**

Joypad Core is a modular, high-performance firmware platform for building controller adapters, custom controllers, and input/output bridges across USB, Bluetooth, and native game console protocols.

Formerly known as **USBRetro**, this project now serves as the foundational firmware layer of the **Joypad** ecosystem — a universal platform for extending how controllers work, connect, and evolve.

Joypad Core focuses on real-time controller I/O, protocol translation, and flexible routing, making it easy to build everything from classic console adapters to next-generation, assistive, and AI-augmented input devices.

---

## What Joypad Core Enables

- **Universal input/output translation**
  Convert USB HID devices into native console protocols and vice versa.

- **Modular firmware apps**
  Build specific bridges like `usb2usb`, `usb2gc`, `snes2usb`, passthrough adapters, merged inputs, and hybrid devices — all on a shared core.

- **Flexible routing & passthrough**
  Support multi-output controllers, input merging, chaining devices, and advanced mods.

- **Hardware-agnostic foundation**
  Designed to run across RP2040 today, with future portability to ESP32 and nRF platforms.

- **Foundation for accessibility & assistive play**
  Enables custom controllers and input extensions for gamers with diverse needs.

Joypad Core is the real-time nervous system of the Joypad platform.

---

## Supported Outputs

Joypad Core currently supports output to a wide range of systems, including:

- **PC Engine / TurboGrafx-16**
- **Nintendo GameCube / Wii**
- **Nuon DVD Players**
- **3DO Interactive Multiplayer**
- **Casio Loopy** *(experimental)*
- **Xbox One S (USB host mod)**

---

## Supported Inputs

Joypad Core supports a wide variety of USB input devices, including:

- Xbox controllers (OG, 360, One, Series X|S)
- PlayStation controllers (DualShock 3/4, DualSense)
- Nintendo Switch Pro Controller & Joy-Cons
- 8BitDo controllers and adapters
- Generic USB HID gamepads
- USB keyboards and mice

---

## Quick Start

### Flashing Prebuilt Firmware

1. Download the latest `.uf2` file from **Releases**.
2. Hold the `BOOT` button while connecting the RP2040 board via USB.
3. Drag the `.uf2` file onto the `RPI-RP2` drive.
4. The device will automatically flash and reboot.

---

## License

Joypad Core is licensed under the **Apache-2.0 License**.

The **Joypad** name and branding are trademarks of Joypad Inc.

# USBRetro

<p align="center"><img src="static/PNGs/USBRetro_Outline.png" width="300"/></p>
<p align="center">
  <img src="https://img.shields.io/github/license/RobertDaleSmith/USBRetro" />
  <img src="https://img.shields.io/github/actions/workflow/status/RobertDaleSmith/USBRetro/.github/workflows/build.yml" />
</p>
<p align="justify">USBRetro is an open source controller adapter firmware for converting USB controllers, keyboards, and mice to various retro consoles' native controller protocols.</p>

### Update Instructions

**USB-2-PCE** [controlleradapter.com/products/usb-2-pce](https://controlleradapter.com/products/usb-2-pce)<br>
**NUON USB** [controlleradapter.com/products/nuon-usb](https://controlleradapter.com/products/nuon-usb)

1. Download the latest release usbretro_[console].uf2 file.
2. Disconnected adapter from the console and all connected USB devices.
3. While holding the boot(+) button, connect the USB-C port to a computer.
4. A virual drive called RPI-RP2 will appear, drag-n-drop the UF2 update file onto the drive.
5. The virtual drive will automatically unmount when the update is completed. 🚀

**GC USB** [controlleradapter.com/products/gc-usb](https://controlleradapter.com/products/gc-usb)

1. Download the latest release usbretro_ngc.uf2 file.
2. Disconnected adapter from the console and all connected USB devices.
3. Connect the USB-C port to a computer.
4. A virual drive called RPI-RP2 will appear, drag-n-drop the UF2 update file onto the drive.
5. The virtual drive will automatically unmount and remount when the update is completed. 🚀

## Compatibility

### USB Host Input
- [x] USB Hubs (up to 5 devices)
- [x] USB HID Keyboards/Mice (maps to controller)
- [x] X-input Controllers (Xbox OG/360/One/SeriesX|S)
- [x] D-input Controllers (generic HID gamepad|joystick)
- [x] PlayStation (PSClassic/DS3/DS4/DualSense)
- [x] Switch (SwitchPro/JoyConGrip)
- [x] 8BitDo Controllers/Adapters
    - [x] PCEngine 2.4g
    - [x] M30 2.4g/BT
    - [ ] NeoGeo 2.4g
    - [x] Wireless Adapter 1 (Grey/Red)
    - [ ] Wireless Adapter 2 (Black/Red)
- [x] Logitech Wingman Action Pad
- [x] Sega Astrocity controller/joystick
- [x] Hori Pokken and Horipad controllers
- [x] DragonRise Generic USB controllers

### Retro Console Output
- [x] PCEngine/TurboGrafx-16
    - [x] Multitap (1-5 players)
    - [x] PCEngine Mouse
    - [x] 2-button Controller
    - [x] 3-button Controller
    - [x] 6-button Controller
- [x] Nuon DVD Players
    - [x] Standard controller output
    - [x] Spinner controller output for Tempest 3000
- [x] GameCube/Wii
    - [x] Standard Controller (with rumble)
    - [x] GameCube Keyboard (scroll lock to enable)
    - [x] Copilot (combine up to 4 controllers into one)
- [x] Xbox One S (USB host controller mod)
    - [x] Full button and analog value passthrough
    - [ ] Rumble passthrough
- [ ] CD-i
    - [ ] 2-player Splitter
    - [ ] Standard Controller
    - [ ] CD-i Mouse Output
- [ ] 3DO
    - [ ] Daisy-chaining (1-8 controllers)
    - [ ] 3DO Mouse Output
    - [ ] 3DO Arcade Start/Coin (Orbatak/Shootout at Old Tucson)
- [ ] Sega Dreamcast
    - [ ] Standard Controller
    - [ ] Dreamcast Keyboard
    - [ ] Virtual VMU/Rumble Pack
- [ ] Any retro console or computer..

## Bugs and Feature Requests

If you run into any issues, then please submit a bug report on the issues tab of this repo. If you would like to see support for specific USB controllers or think of something I missed, then you are welcome to open a feature request under the issues tab. Don't be shy. 👂

## Button Mapping

### Input Map

| USBRetro    | X-input     | Switch      | PlayStation | DirectInput |
| ----------- | ----------- | ----------- | ----------- | ----------- |
| B1          | A           | B           | Cross       | 2           |
| B2          | B           | A           | Circle      | 3           |
| B3          | X           | Y           | Square      | 1           |
| B4          | Y           | X           | Triangle    | 4           |
| L1          | LB          | L           | L1          | 5           |
| R1          | RB          | R           | R1          | 6           |
| L2          | LT          | ZL          | L2          | 7           |
| R2          | RT          | ZR          | R2          | 8           |
| S1          | Back        | Minus       | Select/Share| 9           |
| S2          | Start       | Options     | Start/Option| 10          |
| L3          | LS          | LS          | L3          | 11          |
| R3          | RS          | RS          | R3          | 12          |
| A1          | Guide       | Home        | PS          | 13          |
| A2          |             | Capture     | Touchpad    | 14          |

### Output Map
| USBRetro    | PCEngine      | Nuon        | GameCube    | Xbox One    |
| ----------- | ------------- | ----------- | ----------- | ----------- |
| B1          | II            | A           | B           | A           |
| B2          | I             | C-Down      | A           | B           |
| B3          | IV (turbo II) | B           | Y           | X           |
| B4          | III (turbo I) | C-Left      | X           | Y           |
| L1          | VI            | L           | L           | LB          |
| R1          | V             | R           | R           | RB          |
| L2          |               | C-Up        | L (switch Z)| LT          |
| R2          |               | C-Right     | R (switch Z)| RT          |
| S1          | Select        | Nuon        | Z           | Back        |
| S2          | Run           | Start       | Start       | Start       |
| L3          |               |             |             | LS          |
| R3          |               |             |             | RS          |
| A1          |               |             |             | Guide       |
| A2          |               | Nuon        | Z           |             |

## Compiling
### Setup
#### 1.) Raspberry Pi Pico SDK
First, clone the [pico-sdk](https://github.com/raspberrypi/pico-sdk) repo to your local dev environment. Then point the `PICO_SDK_PATH` environment variable to it.
```cmd
cd ~/git
git clone https://github.com/raspberrypi/pico-sdk.git

cd ~/git/pico-sdk
git submodule init
git submodule update

export PICO_SDK_PATH=~/git/pico-sdk
```

#### 2.) TinyUSB
Then the TinyUSB library within pico-sdk should be on the latest `master` branch. Change to that directory and checkout `master` if not already on it.
```cmd
cd ~/git/pico-sdk/lib/tinyusb
git checkout master
```

#### 3.) Clone USBRetro and Submodules
Once you have cloned this repo to your local environment. The external submodule dependencies must be initialized and updated.
```cmd
cd ~/git
git clone https://github.com/RobertDaleSmith/usbretro.git

cd ~/git/usbretro
git submodule init
git submodule update
```

#### 4.) RP2040 Board Setup
Finally, run the specific microcontroller build script to create a `src/build` directory.
```cmd
cd ~/git/usbretro/src
sh build_ada_kb2040.sh
```

### Build Firmwares
#### Build All
To build all the various console specific firmwares:

```cmd
cd ~/git/usbretro/src/build
cmake ..
make
```

#### Build Individual
If you ever want to build firmware for only a single output console, then you can use `make usbretro_[console]`.
```cmd
make usbretro_pce
make usbretro_ngc
make usbretro_nuon
make usbretro_xb1
```

## Discord Server

Join 👉 [discord.usbretro.com](https://discord.usbretro.com/)

## Acknowledgements

- [Ha Thach](https://github.com/hathach/)'s excellent [TinyUSB library](https://github.com/hathach/tinyusb) and controller example
- [David Shadoff](https://github.com/dshadoff) for building [PCEMouse](https://github.com/dshadoff/PC_Engine_RP2040_Projects/tree/main/PCEMouse) which laid the foundation
- [Ryzee119](https://github.com/Ryzee119) for the wonderful [tusb_xinput](https://github.com/Ryzee119/tusb_xinput/) library for X-input support
- [SelvinPL](https://github.com/SelvinPL/) for the robust [lufa-hid-parser](https://gist.github.com/SelvinPL/99fd9af4566e759b6553e912b6a163f9) example for generic HID gamepad support
- [JonnyHaystack](https://github.com/JonnyHaystack/) for the awesome [joybus-pio](https://github.com/JonnyHaystack/joybus-pio) for GameCube controller output

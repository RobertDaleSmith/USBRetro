# USBRetro

<p align="center"><img src="static/PNGs/USBRetro_Outline.png" width="300"/></p>
<p align="justify">USBRetro is an open source controller adapter firmware for converting USB controllers, keyboards, and mice to various retro consoles' native controller protocols.</p>

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
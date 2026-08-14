# usb2pce

USB/BT controllers to PCEngine/TurboGrafx-16 console.

## Overview

Connects USB and Bluetooth controllers to a PCEngine or TurboGrafx-16 via the multitap protocol. Emulates up to 5 players simultaneously. Supports USB mouse as a PCEngine mouse for compatible games. The controller **button mode** (2-button, 3-button, 6-button) is selected through the [profile system](../core/profiles.md) -- switch it live with a controller hotkey or the [web config](../core/web-config.md), and the choice persists across power cycles.

## Input

- [USB HID](../input/usb-hid.md), [XInput](../input/xinput.md), [Bluetooth](../input/bluetooth.md) controllers
- USB mouse (mapped to PCEngine mouse protocol)

## Output

[PCEngine Output](../output/pcengine.md) -- PIO-based multitap emulation (plex.pio, clock.pio, select.pio).

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE (1:1 controller to multitap slot) |
| Player slots | 5 (shift on disconnect) |
| Max USB devices | 6 |
| Profile system | 4 built-in profiles (button mode + turbo) |

## Button Modes (Profiles)

The button mode is no longer auto-negotiated from the game -- it is an explicit choice, exposed as four built-in profiles applied through the normal `profile_apply()` pipeline. The PCEngine driver reads the active profile's `output_mode` for the byte format and applies its button map (remaps + turbo) each scan.

| Profile | PCE format | B3 | B4 |
|---------|-----------|----|----|
| **2-Button** (default) | 2-button | turbo of **II** (~15 Hz) | turbo of **I** (~15 Hz) |
| **6-Button** | 6-button (Avenue Pad 6) | III/IV hole | III/IV hole |
| **3-Button (Sel)** | 3-button | **Select** | -- |
| **3-Button (Run)** | 3-button | **Run** | -- |

> PCE main buttons map to the JoyPad model as **I = B2**, **II = B1**. In 2-button
> mode the III/IV turbo holes are driven by B3 (turbo II) and B4 (turbo I),
> matching a classic PCE turbo pad. See [PCEngine Output](../output/pcengine.md#button-mapping)
> for the full button table.

### Switching profiles

On the **source controller** (the USB/BT pad you're playing with):

- **SELECT + D-pad Up** -- previous profile
- **SELECT + D-pad Down** -- next profile

Switching is instant and clamps at the ends (no wrap). The selection is saved to flash and restored on boot. Profiles can also be selected from the web config. See [Profiles](../core/profiles.md) for the full hotkey scheme.

## Turbo / Rapid-Fire

- **Built-in** -- the 2-button profile turbos the III/IV holes (~15 Hz), as above.
- **On the fly** -- hold **SELECT + B3** for ~2 s to enter rapid-fire set, then tap any button to cycle its rate (off / 30 / 20 / 15 / 12 / 10 / 7.5 Hz); press **SELECT** to exit. This overlays the active profile and is not persisted. See [Runtime Profile](../core/runtime_profile.md).

## Web Config Mode

When the adapter is plugged into a **PC with no PCEngine attached**, it comes up as a USB **CDC device** for the [web config](../core/web-config.md) (edit/switch profiles, view status) instead of a controller host.

Mode is chosen at boot by watching the console's SEL/CLR control lines: a running console **toggles** them every frame (→ play mode), a PC leaves them at the board's idle level (→ config mode). Because a console powered on **with the adapter already attached** isn't scanning controllers yet at boot, config mode also keeps watching those lines and **reboots into play mode** the instant the console starts scanning -- so a cold boot settles into play mode on its own. A PC never toggles the lines, so it stays a config device.

The web config's **USB Host** page appears read-only for this app: usb2pce hosts controllers on the RP2040's native USB (silicon-fixed pins), so there is nothing to configure.

## Key Features

- **5-player multitap** -- Controllers assigned in connection order. Works with Bomberman '93/'94, Dungeon Explorer, Moto Roader, etc.
- **Selectable button modes** -- 2/3/6-button via profiles (see above).
- **Turbo** -- built-in per profile plus on-the-fly rapid-fire.
- **Mouse support** -- USB mouse outputs native PCEngine mouse protocol for Afterburner II, Darius Plus, Lemmings.
- **Player shifting** -- When a controller disconnects, remaining players shift up to fill gaps.
- **Web config over USB** -- off-console, plug into a PC to configure.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make usb2pce_kb2040` |

## Build and Flash

```bash
make usb2pce_kb2040
make flash-usb2pce_kb2040
```

## Compatible Games

### Mouse-Compatible
- Afterburner II
- Darius Plus
- Lemmings

### Multitap-Compatible (5 players)
- Bomberman '93
- Bomberman '94
- Dungeon Explorer
- Moto Roader

## Troubleshooting

**Controller not responding:**
- Check PCEngine port connections, especially 5V power and ground.
- Verify data and select pin assignments match your board.

**Wrong buttons / mode:**
- Select the matching button-mode profile with **SELECT + D-pad Up/Down** (or the web config). Some games expect a specific 2/3/6-button layout.

**Adapter shows up as a USB serial device on the console:**
- This is config mode triggering on a cold boot; it self-reboots into play mode once the console starts scanning. If it persists, the console isn't polling the controller port -- check the connection.

**Multitap not working:**
- Ensure the USB hub provides enough power for all controllers.
- Some games do not support 5-player mode.

**Mouse not working:**
- Verify the game supports the PCEngine mouse.
- Check that the USB mouse is detected by the adapter.
- Try a different USB mouse model.

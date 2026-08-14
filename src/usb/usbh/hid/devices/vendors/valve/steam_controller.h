// steam_controller.h - Valve Steam Controller 1 (original, "D0G")
// VID: 0x28DE  PIDs: 0x1102 (wired) / 0x1142 (wireless dongle)
//
// Ships in "lizard mode" (keyboard+mouse). This driver disables it and parses
// the native 64-byte vendor state report. Layout/protocol: Linux hid-steam.c
// and SDL SDL_hidapi_steam.c, verified against on-hardware raw logging.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef STEAM_CONTROLLER_H
#define STEAM_CONTROLLER_H

#include "../../../hid_device.h"

#define SC1_VID        0x28DE
#define SC1_PID_WIRED  0x1102
#define SC1_PID_DONGLE 0x1142

extern DeviceInterface steam_controller_interface;

#endif // STEAM_CONTROLLER_H

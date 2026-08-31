// remoteplay_output.h - PS Remote Play OutputInterface (usb2wifi)
// SPDX-License-Identifier: Apache-2.0
#ifndef REMOTEPLAY_OUTPUT_H
#define REMOTEPLAY_OUTPUT_H

#include "core/output_interface.h"

// OutputInterface for the PS Remote Play (WiFi) output. Brings up WiFi station
// mode, drives the session engine, and forwards router input to the console.
// Config (WiFi creds + PSN account + PS5 IP + RP-Key) is provisioned over the
// web config via get_native_config / set_native_config (OUTPUT.NATIVE.*).
extern const OutputInterface remoteplay_output_interface;

#endif // REMOTEPLAY_OUTPUT_H

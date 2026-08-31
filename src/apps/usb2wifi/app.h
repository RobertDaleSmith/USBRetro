// app.h - USB2WiFi App Manifest
// USB controllers -> PS Remote Play (WiFi) output, Pico W
//
// Input:  USB HID/XInput controllers via PIO-USB host.
// Output: PS Remote Play over WiFi (station mode) to a PS5.
// Config: native USB device = CDC (web config) to provision WiFi creds + the
//         PSN account / PS5 IP / RP-Key (from remote-play-lab/rp.py).
//
// NOTE: the Remote Play *session engine* is a staged chiaki port and is stubbed
// in this build (rp_session_stub.c). This app currently brings up WiFi + the
// provisioning + input plumbing; a real session needs a Pico 2 W. See
// .dev/docs/ps5-remoteplay-output.md.

#ifndef APP_USB2WIFI_H
#define APP_USB2WIFI_H

#define APP_NAME "USB2WiFi"
#define APP_DESCRIPTION "USB controller -> PS Remote Play (Pico W)"
#define APP_AUTHOR "RobertDaleSmith"

#ifndef JOYPAD_VERSION
#define JOYPAD_VERSION "0.1.0"
#endif

#endif // APP_USB2WIFI_H

// p5general_mode.c - PlayStation 5 native output via a P5General auth dongle
// SPDX-License-Identifier: MIT
//
// Ported from GP2040-CE P5GeneralDriver (MIT, (c) 2024 OpenStickCommunity).
// Presents to the PS5 as the licensed "Activtor / P5General" controller. A real
// P5General dongle on the USB host port (p5general_host.c) answers the F0/F1/F2
// auth AND signs every 64-byte report (fills hash[8]). We build the controller-
// state portion; the dongle finishes it; we forward the signed report to the PS5.
//
// This is a SIBLING of dualsense_mode.c (which is a PC/DS5Dongle mode) — same
// joypad-os machinery (state mapping, usbd_mode_t), different wire identity+auth.
// Single-chip model: this file owns the shared p5general_auth_data instance; the
// host relay fills the signed report + auth buffers in RAM. See ps5-p5general-*.md.

#include "usbd_mode.h"
#include "descriptors/p5general_descriptors.h"
#include "usb/usbh/hid/devices/vendors/sony/p5general_host.h"  // shared auth data
#include "core/buttons.h"
#include "tusb.h"
#include <string.h>

// Shared with the host relay (defined here so device-only builds still link).
p5general_auth_data_t p5general_auth_data = {0};

static p5general_report_t p5g_report;
static p5general_report_t p5g_report_last;
static uint8_t p5g_diff_repeat;

static void p5general_mode_init(void)
{
    memset(&p5g_report, 0, sizeof(p5g_report));
    p5g_report.report_id      = P5GENERAL_REPORT_ID_INPUT;
    p5g_report.left_stick_x   = P5GENERAL_JOYSTICK_MID;
    p5g_report.left_stick_y   = P5GENERAL_JOYSTICK_MID;
    p5g_report.right_stick_x  = P5GENERAL_JOYSTICK_MID;
    p5g_report.right_stick_y  = P5GENERAL_JOYSTICK_MID;
    p5g_report.dpad           = P5GENERAL_HAT_NOTHING;
    p5g_report.data_30_31_0x001a = 0x001a;
    p5g_report.touchpad_data.p1.unpressed = 1;
    p5g_report.touchpad_data.p2.unpressed = 1;
    memcpy(&p5g_report_last, &p5g_report, sizeof(p5g_report));
    p5g_diff_repeat = 0;

    // A fresh session: nothing signed yet.
    p5general_auth_data.hash_pending = false;
    p5general_auth_data.hash_ready = false;
    p5general_auth_data.passthrough_state = P5G_AUTH_IDLE;
}

// The device is "ready" only once the dongle has mounted — without it there is
// nothing to sign reports, so the PS5 would get nothing valid.
static bool p5general_mode_is_ready(void)
{
    return tud_hid_ready() && p5general_auth_data.dongle_ready;
}

static uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

// GP2040 process(): forward a signed report when ready; otherwise build the
// current report and, if changed, hand it to the dongle for signing.
static bool p5general_mode_send_report(uint8_t player_index,
                                       const input_event_t* event,
                                       const profile_output_t* profile_out,
                                       uint32_t buttons)
{
    (void)player_index;
    p5general_auth_data_t* a = &p5general_auth_data;

    if (!a->dongle_ready) return false;

    // 1) A completed (signed) report waiting? send it to the PS5.
    if (a->hash_ready) {
        if (tud_hid_ready() &&
            tud_hid_report(0, a->hash_finish_buffer, sizeof(a->hash_finish_buffer))) {
            a->hash_ready = false;
        }
        return true;
    }
    // 2) Still waiting for the dongle to sign the last one.
    if (a->hash_pending) return false;

    // 3) Build the current controller state into p5g_report.
    p5general_report_t* r = &p5g_report;

    // D-pad hat
    uint8_t up    = (buttons & JP_BUTTON_DU) ? 1 : 0;
    uint8_t down  = (buttons & JP_BUTTON_DD) ? 1 : 0;
    uint8_t left  = (buttons & JP_BUTTON_DL) ? 1 : 0;
    uint8_t right = (buttons & JP_BUTTON_DR) ? 1 : 0;
    if      (up && right)   r->dpad = P5GENERAL_HAT_UPRIGHT;
    else if (down && right) r->dpad = P5GENERAL_HAT_DOWNRIGHT;
    else if (down && left)  r->dpad = P5GENERAL_HAT_DOWNLEFT;
    else if (up && left)    r->dpad = P5GENERAL_HAT_UPLEFT;
    else if (up)            r->dpad = P5GENERAL_HAT_UP;
    else if (right)         r->dpad = P5GENERAL_HAT_RIGHT;
    else if (down)          r->dpad = P5GENERAL_HAT_DOWN;
    else if (left)          r->dpad = P5GENERAL_HAT_LEFT;
    else                    r->dpad = P5GENERAL_HAT_NOTHING;

    r->button_south = (buttons & JP_BUTTON_B1) ? 1 : 0;  // cross
    r->button_east  = (buttons & JP_BUTTON_B2) ? 1 : 0;  // circle
    r->button_west  = (buttons & JP_BUTTON_B3) ? 1 : 0;  // square
    r->button_north = (buttons & JP_BUTTON_B4) ? 1 : 0;  // triangle
    r->button_l1 = (buttons & JP_BUTTON_L1) ? 1 : 0;
    r->button_r1 = (buttons & JP_BUTTON_R1) ? 1 : 0;
    r->button_l2 = usbd_l2_digital(profile_out, buttons) ? 1 : 0;
    r->button_r2 = usbd_r2_digital(profile_out, buttons) ? 1 : 0;
    r->button_select = (buttons & JP_BUTTON_S1) ? 1 : 0;
    r->button_start  = (buttons & JP_BUTTON_S2) ? 1 : 0;
    r->button_l3 = (buttons & JP_BUTTON_L3) ? 1 : 0;
    r->button_r3 = (buttons & JP_BUTTON_R3) ? 1 : 0;
    r->button_home = (buttons & JP_BUTTON_A1) ? 1 : 0;
    r->button_touchpad = (buttons & JP_BUTTON_A2) ? 1 : 0;

    r->left_stick_x  = profile_out->left_x;
    r->left_stick_y  = profile_out->left_y;
    r->right_stick_x = profile_out->right_x;
    r->right_stick_y = profile_out->right_y;
    r->left_trigger  = profile_out->l2_analog;
    r->right_trigger = profile_out->r2_analog;

    // Motion — pass gyro/accel through byte-swapped (GP2040 stores big-endian).
    // NOTE: units are not yet scaled to P5General's ranges (GYRO_RES 1024 /
    // ACCEL_RES 8192); axis/scale tuning is a hardware follow-up (Phase 8).
    if (event->has_motion) {
        r->gyroscope.x  = (int16_t)bswap16((uint16_t)event->gyro[0]);
        r->gyroscope.y  = (int16_t)bswap16((uint16_t)event->gyro[1]);
        r->gyroscope.z  = (int16_t)bswap16((uint16_t)event->gyro[2]);
        r->accelerometer.x = (int16_t)bswap16((uint16_t)event->accel[0]);
        r->accelerometer.y = (int16_t)bswap16((uint16_t)event->accel[1]);
        r->accelerometer.z = (int16_t)bswap16((uint16_t)event->accel[2]);
    }

    // Touchpad — two fingers, scaled to 1920x943, packed 12-bit X/Y.
    for (int f = 0; f < 2; f++) {
        p5general_touch_xy_t* t = (f == 0) ? &r->touchpad_data.p1 : &r->touchpad_data.p2;
        bool active = event->has_touch && event->touch[f].active;
        t->unpressed = active ? 0 : 1;
        if (active) {
            uint16_t x = (uint16_t)((uint32_t)event->touch[f].x * (P5GENERAL_TP_X_MAX - 1) / 65535);
            uint16_t y = (uint16_t)((uint32_t)event->touch[f].y * (P5GENERAL_TP_Y_MAX - 1) / 65535);
            p5general_pack_touch(t, x, y);
        }
    }

    // 4) On change (or during the 4-frame repeat), queue for signing.
    if (memcmp(&p5g_report_last, r, sizeof(p5g_report)) != 0) {
        memcpy(&p5g_report_last, r, sizeof(p5g_report));
        memcpy(a->hash_pending_buffer, r, sizeof(p5g_report));
        a->hash_pending = true;
        p5g_diff_repeat = 4;
        return true;
    } else if (p5g_diff_repeat) {
        p5g_diff_repeat--;
        memcpy(a->hash_pending_buffer, r, sizeof(p5g_report));
        a->hash_pending = true;
        return true;
    }
    return false;
}

// GET_REPORT feature dispatch (PS5 -> device).
static uint16_t p5general_mode_get_report(uint8_t report_id, hid_report_type_t report_type,
                                          uint8_t* buffer, uint16_t reqlen)
{
    if (report_type != HID_REPORT_TYPE_FEATURE) return 0;
    p5general_auth_data_t* a = &p5general_auth_data;

    switch (report_id) {
        case P5GENERAL_REPORT_DEFINITION: {  // 0x03 device definition
            uint16_t n = reqlen < sizeof(p5general_definition_0x03)
                             ? reqlen : sizeof(p5general_definition_0x03);
            memcpy(buffer, p5general_definition_0x03, n);
            return n;
        }
        case P5GENERAL_GET_SIGNATURE_NONCE:  // 0xF1
            memcpy(buffer, a->auth_buffer + 1, 63);
            if (a->passthrough_state == P5G_AUTH_IDLE) a->passthrough_state = P5G_AUTH_RECV_F1;
            return 63;
        case P5GENERAL_GET_SIGNING_STATE:    // 0xF2
            memcpy(buffer, a->auth_buffer + 1, 15);
            if (a->passthrough_state == P5G_AUTH_IDLE) a->passthrough_state = P5G_AUTH_RECV_F1;
            return 15;
        default:
            return 0;
    }
}

// SET_REPORT feature dispatch (PS5 -> device); called from usbd.c set_report_cb.
void p5general_mode_set_feature_report(uint8_t report_id, const uint8_t* buffer, uint16_t bufsize)
{
    p5general_auth_data_t* a = &p5general_auth_data;
    if (report_id == P5GENERAL_SET_AUTH_PAYLOAD) {  // 0xF0
        if (bufsize != 63) return;
        if (a->passthrough_state == P5G_AUTH_IDLE) {
            a->auth_buffer[0] = report_id;
            memcpy(a->auth_buffer + 1, buffer, bufsize);
            a->passthrough_state = P5G_AUTH_SEND_F0;
        }
    }
}

static const uint8_t* p5general_mode_get_device_descriptor(void) {
    return (const uint8_t*)&p5general_device_descriptor;
}
static const uint8_t* p5general_mode_get_config_descriptor(void) {
    return p5general_config_descriptor;
}
static const uint8_t* p5general_mode_get_report_descriptor(void) {
    return p5general_report_descriptor;
}

const usbd_mode_t p5general_mode = {
    .name = "PlayStation 5 (P5General)",
    .mode = USB_OUTPUT_MODE_PS5,
    .get_device_descriptor = p5general_mode_get_device_descriptor,
    .get_config_descriptor = p5general_mode_get_config_descriptor,
    .get_report_descriptor = p5general_mode_get_report_descriptor,
    .init = p5general_mode_init,
    .send_report = p5general_mode_send_report,
    .is_ready = p5general_mode_is_ready,
    .handle_output = NULL,   // no rumble in the GP2040 P5General path (yet)
    .get_rumble = NULL,
    .get_feedback = NULL,
    .get_report = p5general_mode_get_report,
    .get_class_driver = NULL,
    .task = NULL,
};

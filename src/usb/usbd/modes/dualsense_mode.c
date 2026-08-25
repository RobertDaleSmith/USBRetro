// dualsense_mode.c - PlayStation 5 DualSense output mode (auth passthrough)
// SPDX-License-Identifier: Apache-2.0
//
// Presents as a DualSense (054c:0ce6) and relays the PS5's controller-auth
// challenge to a REAL DualSense on the USB host port via ds5_auth (the
// "controller in the loop" model). Modeled on ps4_mode.c.
//
// HARDWARE-SNIFF-GATED / EXPERIMENTAL: descriptors are reconstructed (not
// byte-exact) and the DualSense auth sequence (page counts, 0xf4/0xf5) is
// unverified. NOT confirmed on a real PS5. See
// .dev/docs/ds5-auth-passthrough-plan.md.

#include "usbd_mode.h"
#include "descriptors/dualsense_descriptors.h"
#ifndef DISABLE_USB_HOST
#include "usb/usbh/hid/devices/vendors/sony/ds5_auth.h"  // host-side relay (USB host only)
#endif
#include "core/buttons.h"
#include "platform/platform.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

static uint8_t ds5_report_buffer[64];
static uint8_t ds5_report_counter = 0;

static void ds5_mode_init(void)
{
    ds5_init_report((ds5_in_report_t*)ds5_report_buffer);
    ds5_report_counter = 0;
}

static bool ds5_mode_is_ready(void)
{
    return tud_hid_ready();
}

// Build the DualSense 0x01 input report (byte order: sticks, triggers, counter,
// hat+face, buttons, ps/tp/mute; vendor tail zeroed).
static bool ds5_mode_send_report(uint8_t player_index,
                                 const input_event_t* event,
                                 const profile_output_t* profile_out,
                                 uint32_t buttons)
{
    (void)player_index; (void)event;

    memset(ds5_report_buffer, 0, sizeof(ds5_report_buffer));
    ds5_report_buffer[0] = 0x01;

    // Sticks (bytes 1-4)
    ds5_report_buffer[1] = profile_out->left_x;
    ds5_report_buffer[2] = profile_out->left_y;
    ds5_report_buffer[3] = profile_out->right_x;
    ds5_report_buffer[4] = profile_out->right_y;

    // Triggers (bytes 5-6)
    ds5_report_buffer[5] = profile_out->l2_analog;
    ds5_report_buffer[6] = profile_out->r2_analog;

    // Counter (byte 7)
    ds5_report_buffer[7] = ds5_report_counter++;

    // Byte 8: hat (low nibble) + face buttons (high nibble)
    uint8_t up = (buttons & JP_BUTTON_DU) ? 1 : 0;
    uint8_t down = (buttons & JP_BUTTON_DD) ? 1 : 0;
    uint8_t left = (buttons & JP_BUTTON_DL) ? 1 : 0;
    uint8_t right = (buttons & JP_BUTTON_DR) ? 1 : 0;
    uint8_t hat;
    if (up && right)        hat = 0x01;
    else if (down && right) hat = 0x03;
    else if (down && left)  hat = 0x05;
    else if (up && left)    hat = 0x07;
    else if (up)            hat = 0x00;
    else if (right)         hat = 0x02;
    else if (down)          hat = 0x04;
    else if (left)          hat = 0x06;
    else                    hat = DS5_HAT_NOTHING;

    uint8_t face = 0;
    if (buttons & JP_BUTTON_B3) face |= 0x10;  // Square
    if (buttons & JP_BUTTON_B1) face |= 0x20;  // Cross
    if (buttons & JP_BUTTON_B2) face |= 0x40;  // Circle
    if (buttons & JP_BUTTON_B4) face |= 0x80;  // Triangle
    ds5_report_buffer[8] = hat | face;

    // Byte 9: shoulders / create / options / stick clicks
    uint8_t b9 = 0;
    if (buttons & JP_BUTTON_L1) b9 |= 0x01;
    if (buttons & JP_BUTTON_R1) b9 |= 0x02;
    if (usbd_l2_digital(profile_out, buttons)) b9 |= 0x04;
    if (usbd_r2_digital(profile_out, buttons)) b9 |= 0x08;
    if (buttons & JP_BUTTON_S1) b9 |= 0x10;  // Create/Share
    if (buttons & JP_BUTTON_S2) b9 |= 0x20;  // Options
    if (buttons & JP_BUTTON_L3) b9 |= 0x40;
    if (buttons & JP_BUTTON_R3) b9 |= 0x80;
    ds5_report_buffer[9] = b9;

    // Byte 10: PS / touchpad-click / mute + counter
    uint8_t b10 = 0;
    if (buttons & JP_BUTTON_A1) b10 |= 0x01;  // PS
    if (buttons & JP_BUTTON_A2) b10 |= 0x02;  // Touchpad click
    b10 |= ((ds5_report_counter & 0x1F) << 3);
    ds5_report_buffer[10] = b10;

    // Bytes 11-63: vendor (IMU/touchpad/battery) left zeroed for now.

    return tud_hid_report(0x01, ds5_report_buffer, sizeof(ds5_report_buffer));
}

// GET_REPORT feature dispatch — auth signature/status/reset via ds5_auth.
static uint16_t ds5_mode_get_report(uint8_t report_id, hid_report_type_t report_type,
                                    uint8_t* buffer, uint16_t reqlen)
{
    if (report_type != HID_REPORT_TYPE_FEATURE) return 0;

    switch (report_id) {
        case DS5_REPORT_ID_AUTH_RESPONSE:  // 0xF1
#ifndef DISABLE_USB_HOST
            if (ds5_auth_is_available()) return ds5_auth_get_next_signature(buffer, reqlen);
#endif
            memset(buffer, 0, reqlen); return reqlen;

        case DS5_REPORT_ID_AUTH_STATUS:    // 0xF2
#ifndef DISABLE_USB_HOST
            if (ds5_auth_is_available()) return ds5_auth_get_status(buffer, reqlen);
#endif
            memset(buffer, 0, reqlen); buffer[1] = 0x10; return reqlen;  // "signing"

        case DS5_REPORT_ID_AUTH_RESET:     // 0xF3
#ifndef DISABLE_USB_HOST
            ds5_auth_reset();
#endif
            memset(buffer, 0, reqlen);
            memcpy(buffer, ds5_feature_f3,
                   reqlen < sizeof(ds5_feature_f3) ? reqlen : sizeof(ds5_feature_f3));
            return reqlen;

        default:
            return 0;
    }
}

// SET_REPORT feature dispatch — called from usbd.c tud_hid_set_report_cb.
void ds5_mode_set_feature_report(uint8_t report_id, const uint8_t* buffer, uint16_t bufsize)
{
    switch (report_id) {
        case DS5_REPORT_ID_AUTH_NONCE:  // 0xF0
#ifndef DISABLE_USB_HOST
            if (ds5_auth_is_available()) ds5_auth_send_nonce(buffer, bufsize);
#else
            (void)buffer; (void)bufsize;
#endif
            break;
        case DS5_REPORT_ID_AUTH_RESET:  // 0xF3
#ifndef DISABLE_USB_HOST
            ds5_auth_reset();
#endif
            break;
        default:
            break;
    }
}

static const uint8_t* ds5_mode_get_device_descriptor(void) { return (const uint8_t*)&ds5_device_descriptor; }
static const uint8_t* ds5_mode_get_config_descriptor(void) { return ds5_config_descriptor; }
static const uint8_t* ds5_mode_get_report_descriptor(void) { return ds5_report_descriptor; }

const usbd_mode_t dualsense_mode = {
    .name = "PS5 (DualSense passthrough)",
    .mode = USB_OUTPUT_MODE_DUALSENSE,
    .get_device_descriptor = ds5_mode_get_device_descriptor,
    .get_config_descriptor = ds5_mode_get_config_descriptor,
    .get_report_descriptor = ds5_mode_get_report_descriptor,
    .init = ds5_mode_init,
    .send_report = ds5_mode_send_report,
    .is_ready = ds5_mode_is_ready,
    .handle_output = NULL,
    .get_rumble = NULL,
    .get_feedback = NULL,
    .get_report = ds5_mode_get_report,
    .get_class_driver = NULL,
    .task = NULL,
};

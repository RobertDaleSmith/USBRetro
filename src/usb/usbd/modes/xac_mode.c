// xac_mode.c - Xbox Adaptive Controller compatible USB device mode
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "tusb.h"
#include "../usbd_mode.h"
#include "../usbd.h"
#include "descriptors/xac_descriptors.h"
#include "core/buttons.h"
#include <string.h>

// ============================================================================
// STATE
// ============================================================================

static xac_in_report_t xac_report;

// ============================================================================
// CONVERSION HELPER
// ============================================================================

// Convert input_event dpad to HID hat switch
static uint8_t convert_dpad_to_hat(uint32_t buttons)
{
    uint8_t up = (buttons & JP_BUTTON_DU) ? 1 : 0;
    uint8_t down = (buttons & JP_BUTTON_DD) ? 1 : 0;
    uint8_t left = (buttons & JP_BUTTON_DL) ? 1 : 0;
    uint8_t right = (buttons & JP_BUTTON_DR) ? 1 : 0;

    if (up && right) return XAC_HAT_UP_RIGHT;
    if (up && left) return XAC_HAT_UP_LEFT;
    if (down && right) return XAC_HAT_DOWN_RIGHT;
    if (down && left) return XAC_HAT_DOWN_LEFT;
    if (up) return XAC_HAT_UP;
    if (down) return XAC_HAT_DOWN;
    if (left) return XAC_HAT_LEFT;
    if (right) return XAC_HAT_RIGHT;

    return XAC_HAT_CENTER;
}

// ============================================================================
// MODE INTERFACE IMPLEMENTATION
// ============================================================================

static void xac_mode_init(void)
{
    xac_init_report(&xac_report);
}

static bool xac_mode_is_ready(void)
{
    return tud_hid_ready();
}

static bool xac_mode_send_report(uint8_t player_index,
                                  const input_event_t* event,
                                  const profile_output_t* profile_out,
                                  uint32_t buttons)
{
    (void)player_index;
    (void)event;

    // Analog sticks (HID convention: 0=up, 255=down - no inversion needed)
    xac_report.lx = profile_out->left_x;
    xac_report.ly = profile_out->left_y;
    xac_report.rx = profile_out->right_x;
    xac_report.ry = profile_out->right_y;

    // D-pad as hat switch
    xac_report.hat = convert_dpad_to_hat(buttons);

    // Buttons (12 total, split into low 4 bits and high 8 bits)
    uint16_t xac_buttons = 0;
    if (buttons & JP_BUTTON_B1) xac_buttons |= XAC_MASK_B1;  // A
    if (buttons & JP_BUTTON_B2) xac_buttons |= XAC_MASK_B2;  // B
    if (buttons & JP_BUTTON_B3) xac_buttons |= XAC_MASK_B3;  // X
    if (buttons & JP_BUTTON_B4) xac_buttons |= XAC_MASK_B4;  // Y
    if (buttons & JP_BUTTON_L1) xac_buttons |= XAC_MASK_L1;  // LB
    if (buttons & JP_BUTTON_R1) xac_buttons |= XAC_MASK_R1;  // RB
    // xac_in_report_t is digital-only for LT/RT - see usbd_mode.h.
    if (usbd_l2_digital(profile_out, buttons)) xac_buttons |= XAC_MASK_L2;  // LT (digital)
    if (usbd_r2_digital(profile_out, buttons)) xac_buttons |= XAC_MASK_R2;  // RT (digital)
    if (buttons & JP_BUTTON_S1) xac_buttons |= XAC_MASK_S1;  // Back
    if (buttons & JP_BUTTON_S2) xac_buttons |= XAC_MASK_S2;  // Start
    if (buttons & JP_BUTTON_L3) xac_buttons |= XAC_MASK_L3;  // LS
    if (buttons & JP_BUTTON_R3) xac_buttons |= XAC_MASK_R3;  // RS

    xac_report.buttons_lo = xac_buttons & 0x0F;
    xac_report.buttons_hi = (xac_buttons >> 4) & 0xFF;

    return tud_hid_report(0, &xac_report, sizeof(xac_report));
}

static const uint8_t* xac_mode_get_device_descriptor(void)
{
    return (const uint8_t*)&xac_device_descriptor;
}

static const uint8_t* xac_mode_get_config_descriptor(void)
{
    return xac_config_descriptor;
}

static const uint8_t* xac_mode_get_report_descriptor(void)
{
    return xac_report_descriptor;
}

// ============================================================================
// MODE EXPORT
// ============================================================================

const usbd_mode_t xac_mode = {
    .name = "XAC Compat",
    .mode = USB_OUTPUT_MODE_XAC,

    .get_device_descriptor = xac_mode_get_device_descriptor,
    .get_config_descriptor = xac_mode_get_config_descriptor,
    .get_report_descriptor = xac_mode_get_report_descriptor,

    .init = xac_mode_init,
    .send_report = xac_mode_send_report,
    .is_ready = xac_mode_is_ready,

    // XAC mode has no rumble or feedback
    .handle_output = NULL,
    .get_rumble = NULL,
    .get_feedback = NULL,
    .get_report = NULL,

    .get_class_driver = NULL,  // Uses built-in HID class driver
    .task = NULL,
};

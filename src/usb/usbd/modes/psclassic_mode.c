// psclassic_mode.c - PlayStation Classic USB device mode
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "tusb.h"
#include "../usbd_mode.h"
#include "../usbd.h"
#include "descriptors/psclassic_descriptors.h"
#include "core/buttons.h"
#include <string.h>

// ============================================================================
// STATE
// ============================================================================

static psclassic_in_report_t psclassic_report;

// ============================================================================
// MODE INTERFACE IMPLEMENTATION
// ============================================================================

static void psclassic_mode_init(void)
{
    psclassic_init_report(&psclassic_report);
}

static bool psclassic_mode_is_ready(void)
{
    return tud_hid_ready();
}

static bool psclassic_mode_send_report(uint8_t player_index,
                                        const input_event_t* event,
                                        const profile_output_t* profile_out,
                                        uint32_t buttons)
{
    (void)player_index;
    (void)event;

    // Start with D-pad centered
    psclassic_report.buttons = PSCLASSIC_DPAD_CENTER;

    // D-pad encoding (bits 10-13)
    uint8_t up = (buttons & JP_BUTTON_DU) ? 1 : 0;
    uint8_t down = (buttons & JP_BUTTON_DD) ? 1 : 0;
    uint8_t left = (buttons & JP_BUTTON_DL) ? 1 : 0;
    uint8_t right = (buttons & JP_BUTTON_DR) ? 1 : 0;

    if (up && right)        psclassic_report.buttons = PSCLASSIC_DPAD_UP_RIGHT;
    else if (up && left)    psclassic_report.buttons = PSCLASSIC_DPAD_UP_LEFT;
    else if (down && right) psclassic_report.buttons = PSCLASSIC_DPAD_DOWN_RIGHT;
    else if (down && left)  psclassic_report.buttons = PSCLASSIC_DPAD_DOWN_LEFT;
    else if (up)            psclassic_report.buttons = PSCLASSIC_DPAD_UP;
    else if (down)          psclassic_report.buttons = PSCLASSIC_DPAD_DOWN;
    else if (left)          psclassic_report.buttons = PSCLASSIC_DPAD_LEFT;
    else if (right)         psclassic_report.buttons = PSCLASSIC_DPAD_RIGHT;

    // Face buttons and shoulders (bits 0-9)
    psclassic_report.buttons |=
          (buttons & JP_BUTTON_B4 ? PSCLASSIC_MASK_TRIANGLE : 0)
        | (buttons & JP_BUTTON_B2 ? PSCLASSIC_MASK_CIRCLE   : 0)
        | (buttons & JP_BUTTON_B1 ? PSCLASSIC_MASK_CROSS    : 0)
        | (buttons & JP_BUTTON_B3 ? PSCLASSIC_MASK_SQUARE   : 0)
        | (buttons & JP_BUTTON_L1 ? PSCLASSIC_MASK_L1       : 0)
        | (buttons & JP_BUTTON_R1 ? PSCLASSIC_MASK_R1       : 0)
        // psclassic_in_report_t is buttons-only - no analog trigger field to
        // fall back on for an analog-only input source. See usbd_mode.h.
        | (usbd_l2_digital(profile_out, buttons) ? PSCLASSIC_MASK_L2 : 0)
        | (usbd_r2_digital(profile_out, buttons) ? PSCLASSIC_MASK_R2 : 0)
        | (buttons & JP_BUTTON_S1 ? PSCLASSIC_MASK_SELECT   : 0)
        | (buttons & JP_BUTTON_S2 ? PSCLASSIC_MASK_START    : 0);

    return tud_hid_report(0, &psclassic_report, sizeof(psclassic_report));
}

static const uint8_t* psclassic_mode_get_device_descriptor(void)
{
    return (const uint8_t*)&psclassic_device_descriptor;
}

static const uint8_t* psclassic_mode_get_config_descriptor(void)
{
    return psclassic_config_descriptor;
}

static const uint8_t* psclassic_mode_get_report_descriptor(void)
{
    return psclassic_report_descriptor;
}

// ============================================================================
// MODE EXPORT
// ============================================================================

const usbd_mode_t psclassic_mode = {
    .name = "PSClassic",
    .mode = USB_OUTPUT_MODE_PSCLASSIC,

    .get_device_descriptor = psclassic_mode_get_device_descriptor,
    .get_config_descriptor = psclassic_mode_get_config_descriptor,
    .get_report_descriptor = psclassic_mode_get_report_descriptor,

    .init = psclassic_mode_init,
    .send_report = psclassic_mode_send_report,
    .is_ready = psclassic_mode_is_ready,

    // No feedback support for PS Classic
    .handle_output = NULL,
    .get_rumble = NULL,
    .get_feedback = NULL,
    .get_report = NULL,
    .get_class_driver = NULL,
    .task = NULL,
};

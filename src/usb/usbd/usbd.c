// usbd.c - USB device output
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Implements USB device mode for USBRetro, enabling the adapter to emulate
// a gamepad for USB-capable consoles. Uses TinyUSB device stack.
//
// Supports multiple output modes:
// - HID (DInput/PS3-compatible) - default
// - Xbox Original (XID protocol)
// - Future: XInput, PS4, Switch, etc.
//
// Mode is stored in flash and can be changed via CDC commands.
// Mode changes require USB re-enumeration (device reset).

#include "usbd.h"
#include "descriptors/hid_descriptors.h"
#include "descriptors/xbox_og_descriptors.h"
#include "descriptors/xinput_descriptors.h"
#include "descriptors/switch_descriptors.h"
#include "descriptors/ps3_descriptors.h"
#include "tud_xid.h"
#include "tud_xinput.h"
#include "cdc/cdc.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/buttons.h"
#include "core/services/storage/flash.h"
#include "core/services/button/button.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// STATE
// ============================================================================

// Current HID report (for HID mode)
static usbretro_hid_report_t hid_report;

// Current XID report (for Xbox OG mode)
static xbox_og_in_report_t xid_report;
static xbox_og_out_report_t xid_rumble;
static bool xid_rumble_available = false;

// Current XInput report (for Xbox 360 mode)
static xinput_in_report_t xinput_report;
static xinput_out_report_t xinput_output;
static bool xinput_output_available = false;

// Current Switch report (for Nintendo Switch mode)
static switch_in_report_t switch_report;

// Current PS3 report (for PlayStation 3 mode)
static ps3_in_report_t ps3_report;
static ps3_out_report_t ps3_output;
static bool ps3_output_available = false;

// Serial number from board unique ID (12 hex chars + null)
#define USB_SERIAL_LEN 12
static char usb_serial_str[USB_SERIAL_LEN + 1];

// Current output mode (persisted to flash)
static usb_output_mode_t output_mode = USB_OUTPUT_MODE_HID;
static flash_t flash_settings;

// Mode names for display
static const char* mode_names[] = {
    [USB_OUTPUT_MODE_HID] = "HID (DInput)",
    [USB_OUTPUT_MODE_XBOX_ORIGINAL] = "Xbox Original",
    [USB_OUTPUT_MODE_XINPUT] = "XInput",
    [USB_OUTPUT_MODE_PS3] = "PS3",
    [USB_OUTPUT_MODE_PS4] = "PS4",
    [USB_OUTPUT_MODE_SWITCH] = "Switch",
};

// ============================================================================
// CONVERSION HELPERS
// ============================================================================

// Convert input_event buttons to HID gamepad buttons
static uint16_t convert_buttons(uint32_t buttons)
{
    uint16_t hid_buttons = 0;

    // USBRetro uses active-high (1 = pressed), HID uses active-high (1 = pressed)
    // No inversion needed

    if (buttons & USBR_BUTTON_B1) hid_buttons |= USB_GAMEPAD_MASK_B1;
    if (buttons & USBR_BUTTON_B2) hid_buttons |= USB_GAMEPAD_MASK_B2;
    if (buttons & USBR_BUTTON_B3) hid_buttons |= USB_GAMEPAD_MASK_B3;
    if (buttons & USBR_BUTTON_B4) hid_buttons |= USB_GAMEPAD_MASK_B4;
    if (buttons & USBR_BUTTON_L1) hid_buttons |= USB_GAMEPAD_MASK_L1;
    if (buttons & USBR_BUTTON_R1) hid_buttons |= USB_GAMEPAD_MASK_R1;
    if (buttons & USBR_BUTTON_L2) hid_buttons |= USB_GAMEPAD_MASK_L2;
    if (buttons & USBR_BUTTON_R2) hid_buttons |= USB_GAMEPAD_MASK_R2;
    if (buttons & USBR_BUTTON_S1) hid_buttons |= USB_GAMEPAD_MASK_S1;
    if (buttons & USBR_BUTTON_S2) hid_buttons |= USB_GAMEPAD_MASK_S2;
    if (buttons & USBR_BUTTON_L3) hid_buttons |= USB_GAMEPAD_MASK_L3;
    if (buttons & USBR_BUTTON_R3) hid_buttons |= USB_GAMEPAD_MASK_R3;
    if (buttons & USBR_BUTTON_A1) hid_buttons |= USB_GAMEPAD_MASK_A1;
    if (buttons & USBR_BUTTON_A2) hid_buttons |= USB_GAMEPAD_MASK_A2;

    return hid_buttons;
}

// Convert input_event dpad to HID hat switch
static uint8_t convert_dpad_to_hat(uint32_t buttons)
{
    // USBRetro uses active-high (1 = pressed)
    uint8_t up = (buttons & USBR_BUTTON_DU) ? 1 : 0;
    uint8_t down = (buttons & USBR_BUTTON_DD) ? 1 : 0;
    uint8_t left = (buttons & USBR_BUTTON_DL) ? 1 : 0;
    uint8_t right = (buttons & USBR_BUTTON_DR) ? 1 : 0;

    if (up && right) return HID_HAT_UP_RIGHT;
    if (up && left) return HID_HAT_UP_LEFT;
    if (down && right) return HID_HAT_DOWN_RIGHT;
    if (down && left) return HID_HAT_DOWN_LEFT;
    if (up) return HID_HAT_UP;
    if (down) return HID_HAT_DOWN;
    if (left) return HID_HAT_LEFT;
    if (right) return HID_HAT_RIGHT;

    return HID_HAT_CENTER;
}

// ============================================================================
// XID CONVERSION HELPERS (Xbox Original mode)
// ============================================================================

// Convert USBRetro buttons to Xbox OG digital buttons (byte 2)
static uint8_t convert_xid_digital_buttons(uint32_t buttons)
{
    uint8_t xog_buttons = 0;

    if (buttons & USBR_BUTTON_DU) xog_buttons |= XBOX_OG_BTN_DPAD_UP;
    if (buttons & USBR_BUTTON_DD) xog_buttons |= XBOX_OG_BTN_DPAD_DOWN;
    if (buttons & USBR_BUTTON_DL) xog_buttons |= XBOX_OG_BTN_DPAD_LEFT;
    if (buttons & USBR_BUTTON_DR) xog_buttons |= XBOX_OG_BTN_DPAD_RIGHT;
    if (buttons & USBR_BUTTON_S2) xog_buttons |= XBOX_OG_BTN_START;
    if (buttons & USBR_BUTTON_S1) xog_buttons |= XBOX_OG_BTN_BACK;
    if (buttons & USBR_BUTTON_L3) xog_buttons |= XBOX_OG_BTN_L3;
    if (buttons & USBR_BUTTON_R3) xog_buttons |= XBOX_OG_BTN_R3;

    return xog_buttons;
}

// Convert analog value from USBRetro (0-255, center 128) to Xbox OG signed 16-bit
static int16_t convert_axis_to_s16(uint8_t value)
{
    int32_t scaled = ((int32_t)value - 128) * 256;
    if (scaled < -32768) scaled = -32768;
    if (scaled > 32767) scaled = 32767;
    return (int16_t)scaled;
}

// ============================================================================
// MODE SELECTION API
// ============================================================================

usb_output_mode_t usbd_get_mode(void)
{
    return output_mode;
}

// Helper to flush debug output over CDC
static void flush_debug_output(void)
{
    tud_task();
    sleep_ms(20);
    tud_task();
}

bool usbd_set_mode(usb_output_mode_t mode)
{
    if (mode >= USB_OUTPUT_MODE_COUNT) {
        return false;
    }

    // Supported modes: HID, Xbox OG, XInput, PS3, Switch
    if (mode != USB_OUTPUT_MODE_HID &&
        mode != USB_OUTPUT_MODE_XBOX_ORIGINAL &&
        mode != USB_OUTPUT_MODE_XINPUT &&
        mode != USB_OUTPUT_MODE_PS3 &&
        mode != USB_OUTPUT_MODE_SWITCH) {
        printf("[usbd] Mode %d not yet supported\n", mode);
        return false;
    }

    if (mode == output_mode) {
        return false;  // Same mode, no change needed
    }

    printf("[usbd] Changing mode from %s to %s\n",
           mode_names[output_mode], mode_names[mode]);
    flush_debug_output();

    // Save mode to flash immediately (we're about to reset)
    printf("[usbd] Setting flash_settings.usb_output_mode = %d\n", mode);
    flush_debug_output();
    flash_settings.usb_output_mode = (uint8_t)mode;
    printf("[usbd] Calling flash_save_now...\n");
    flush_debug_output();
    flash_save_now(&flash_settings);
    printf("[usbd] Mode saved to flash (mode=%d)\n", flash_settings.usb_output_mode);
    flush_debug_output();

    // Verify the write by reading it back
    flash_t verify_settings;
    if (flash_load(&verify_settings)) {
        printf("[usbd] Verify: mode=%d (expected %d)\n",
               verify_settings.usb_output_mode, mode);
    } else {
        printf("[usbd] Verify FAILED: flash_load returned false!\n");
    }
    flush_debug_output();

    output_mode = mode;

    // Wait for button release before resetting
    // (holding boot button during reset enters bootloader)
    printf("[usbd] Release button to reset...\n");
    flush_debug_output();
    #ifdef BUTTON_USER_GPIO
    while (!gpio_get(BUTTON_USER_GPIO)) {
        sleep_ms(10);
        tud_task();  // Keep USB alive while waiting
    }
    sleep_ms(100);  // Brief debounce after release
    #endif

    // Trigger device reset to re-enumerate with new descriptors
    printf("[usbd] Resetting device for re-enumeration...\n");
    flush_debug_output();
    watchdog_enable(100, false);  // Reset in 100ms
    while(1);  // Wait for watchdog reset

    return true;  // Never reached
}

const char* usbd_get_mode_name(usb_output_mode_t mode)
{
    if (mode < USB_OUTPUT_MODE_COUNT) {
        return mode_names[mode];
    }
    return "Unknown";
}

// ============================================================================
// PUBLIC API
// ============================================================================

void usbd_init(void)
{
    printf("[usbd] Initializing USB device output\n");

    // Initialize and load settings from flash
    flash_init();
    printf("[usbd] Loading settings from flash...\n");
    if (flash_load(&flash_settings)) {
        printf("[usbd] Flash load success! usb_output_mode=%d, active_profile=%d\n",
               flash_settings.usb_output_mode, flash_settings.active_profile_index);
        // Validate loaded mode
        if (flash_settings.usb_output_mode < USB_OUTPUT_MODE_COUNT) {
            // Only accept supported modes
            if (flash_settings.usb_output_mode == USB_OUTPUT_MODE_HID ||
                flash_settings.usb_output_mode == USB_OUTPUT_MODE_XBOX_ORIGINAL ||
                flash_settings.usb_output_mode == USB_OUTPUT_MODE_XINPUT ||
                flash_settings.usb_output_mode == USB_OUTPUT_MODE_PS3 ||
                flash_settings.usb_output_mode == USB_OUTPUT_MODE_SWITCH) {
                output_mode = (usb_output_mode_t)flash_settings.usb_output_mode;
                printf("[usbd] Loaded mode from flash: %s\n", mode_names[output_mode]);
            } else {
                printf("[usbd] Unsupported mode %d in flash, using default\n",
                       flash_settings.usb_output_mode);
            }
        }
    } else {
        printf("[usbd] No valid flash settings (magic mismatch), using defaults\n");
        memset(&flash_settings, 0, sizeof(flash_settings));
    }

    printf("[usbd] Mode: %s\n", mode_names[output_mode]);

    // Get unique board ID for USB serial number (first 12 chars)
    char full_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(full_id, sizeof(full_id));
    memcpy(usb_serial_str, full_id, USB_SERIAL_LEN);
    usb_serial_str[USB_SERIAL_LEN] = '\0';
    printf("[usbd] Serial: %s\n", usb_serial_str);

    // Initialize TinyUSB device stack
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = (output_mode == USB_OUTPUT_MODE_XBOX_ORIGINAL)
                 ? TUSB_SPEED_FULL  // Xbox OG is USB 1.1
                 : TUSB_SPEED_AUTO
    };
    tusb_init(0, &dev_init);

    // Initialize reports based on mode
    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL:
            // Initialize XID report to neutral state
            memset(&xid_report, 0, sizeof(xbox_og_in_report_t));
            xid_report.reserved1 = 0x00;
            xid_report.report_len = sizeof(xbox_og_in_report_t);
            memset(&xid_rumble, 0, sizeof(xbox_og_out_report_t));
            break;

        case USB_OUTPUT_MODE_XINPUT:
            // Initialize XInput report to neutral state
            memset(&xinput_report, 0, sizeof(xinput_in_report_t));
            xinput_report.report_id = 0x00;
            xinput_report.report_size = sizeof(xinput_in_report_t);
            memset(&xinput_output, 0, sizeof(xinput_out_report_t));
            break;

        case USB_OUTPUT_MODE_SWITCH:
            // Initialize Switch report to neutral state
            memset(&switch_report, 0, sizeof(switch_in_report_t));
            switch_report.hat = SWITCH_HAT_CENTER;
            switch_report.lx = SWITCH_JOYSTICK_MID;
            switch_report.ly = SWITCH_JOYSTICK_MID;
            switch_report.rx = SWITCH_JOYSTICK_MID;
            switch_report.ry = SWITCH_JOYSTICK_MID;
            break;

        case USB_OUTPUT_MODE_PS3:
            // Initialize PS3 report to neutral state
            ps3_init_report(&ps3_report);
            memset(&ps3_output, 0, sizeof(ps3_out_report_t));
            break;

        case USB_OUTPUT_MODE_HID:
        default:
            // Initialize HID report to neutral state
            memset(&hid_report, 0, sizeof(usbretro_hid_report_t));
            hid_report.lx = 128;  // Center
            hid_report.ly = 128;
            hid_report.rx = 128;
            hid_report.ry = 128;
            hid_report.hat = HID_HAT_CENTER;
            break;
    }

    // Initialize CDC subsystem (only for HID and Switch modes)
    if (output_mode == USB_OUTPUT_MODE_HID || output_mode == USB_OUTPUT_MODE_SWITCH) {
        cdc_init();
    }

    printf("[usbd] Initialization complete\n");
}

void usbd_task(void)
{
    // TinyUSB device task - runs from core0 main loop
    tud_task();

    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL:
            // Xbox OG mode: check for rumble updates
            if (tud_xid_get_rumble(&xid_rumble)) {
                xid_rumble_available = true;
            }
            // Send XID report if ready
            if (tud_xid_ready()) {
                usbd_send_report(0);
            }
            break;

        case USB_OUTPUT_MODE_XINPUT:
            // XInput mode: check for rumble/LED updates
            if (tud_xinput_get_output(&xinput_output)) {
                xinput_output_available = true;
            }
            // Send XInput report if ready
            if (tud_xinput_ready()) {
                usbd_send_report(0);
            }
            break;

        case USB_OUTPUT_MODE_SWITCH:
            // Switch mode: process CDC tasks, send HID report
            cdc_task();
            if (tud_hid_ready()) {
                usbd_send_report(0);
            }
            break;

        case USB_OUTPUT_MODE_PS3:
            // PS3 mode: send HID report (no CDC - PS3 doesn't use it)
            if (tud_hid_ready()) {
                usbd_send_report(0);
            }
            break;

        case USB_OUTPUT_MODE_HID:
        default:
            // HID mode: process CDC tasks
            cdc_task();
            // Send HID report if device is ready
            if (tud_hid_ready()) {
                usbd_send_report(0);
            }
            break;
    }
}

// Send XID report (Xbox Original mode)
static bool usbd_send_xid_report(uint8_t player_index)
{
    if (!tud_xid_ready()) {
        return false;
    }

    const input_event_t* event = router_get_output(OUTPUT_TARGET_USB_DEVICE, player_index);

    if (event) {
        // Digital buttons (DPAD, Start, Back, L3, R3)
        xid_report.buttons = convert_xid_digital_buttons(event->buttons);

        // Analog face buttons (0 = not pressed, 255 = fully pressed)
        xid_report.a     = (event->buttons & USBR_BUTTON_B1) ? 0xFF : 0x00;
        xid_report.b     = (event->buttons & USBR_BUTTON_B2) ? 0xFF : 0x00;
        xid_report.x     = (event->buttons & USBR_BUTTON_B3) ? 0xFF : 0x00;
        xid_report.y     = (event->buttons & USBR_BUTTON_B4) ? 0xFF : 0x00;
        xid_report.black = (event->buttons & USBR_BUTTON_L1) ? 0xFF : 0x00;  // L1 -> Black
        xid_report.white = (event->buttons & USBR_BUTTON_R1) ? 0xFF : 0x00;  // R1 -> White

        // Analog triggers (use digital for now)
        xid_report.trigger_l = (event->buttons & USBR_BUTTON_L2) ? 0xFF : 0x00;
        xid_report.trigger_r = (event->buttons & USBR_BUTTON_R2) ? 0xFF : 0x00;

        // Analog sticks (signed 16-bit, -32768 to +32767)
        xid_report.stick_lx = convert_axis_to_s16(event->analog[ANALOG_X]);
        xid_report.stick_ly = convert_axis_to_s16(event->analog[ANALOG_Y]);
        xid_report.stick_rx = convert_axis_to_s16(event->analog[ANALOG_Z]);
        xid_report.stick_ry = convert_axis_to_s16(event->analog[ANALOG_RX]);
    } else {
        // No input - send neutral state
        xid_report.buttons = 0;
        xid_report.a = 0;
        xid_report.b = 0;
        xid_report.x = 0;
        xid_report.y = 0;
        xid_report.black = 0;
        xid_report.white = 0;
        xid_report.trigger_l = 0;
        xid_report.trigger_r = 0;
        xid_report.stick_lx = 0;
        xid_report.stick_ly = 0;
        xid_report.stick_rx = 0;
        xid_report.stick_ry = 0;
    }

    return tud_xid_send_report(&xid_report);
}

// Send HID report (DInput mode)
static bool usbd_send_hid_report(uint8_t player_index)
{
    if (!tud_hid_ready()) {
        return false;
    }

    const input_event_t* event = router_get_output(OUTPUT_TARGET_USB_DEVICE, player_index);

    if (event) {
        // Convert input event to HID report
        uint16_t buttons = convert_buttons(event->buttons);
        hid_report.buttons = buttons;
        hid_report.hat = convert_dpad_to_hat(event->buttons);

        // Analog sticks (input_event uses 0-255, HID Y-axis is inverted: 0=up, 255=down)
        hid_report.lx = event->analog[ANALOG_X];
        hid_report.ly = 255 - event->analog[ANALOG_Y];
        hid_report.rx = event->analog[ANALOG_Z];
        hid_report.ry = 255 - event->analog[ANALOG_RX];  // ANALOG_RX = Right stick Y

        // PS3 pressure axes (0x00 = released, 0xFF = fully pressed)
        hid_report.pressure_dpad_right = (event->buttons & USBR_BUTTON_DR) ? 0xFF : 0x00;
        hid_report.pressure_dpad_left  = (event->buttons & USBR_BUTTON_DL) ? 0xFF : 0x00;
        hid_report.pressure_dpad_up    = (event->buttons & USBR_BUTTON_DU) ? 0xFF : 0x00;
        hid_report.pressure_dpad_down  = (event->buttons & USBR_BUTTON_DD) ? 0xFF : 0x00;
        hid_report.pressure_triangle   = (buttons & USB_GAMEPAD_MASK_B4) ? 0xFF : 0x00;
        hid_report.pressure_circle     = (buttons & USB_GAMEPAD_MASK_B2) ? 0xFF : 0x00;
        hid_report.pressure_cross      = (buttons & USB_GAMEPAD_MASK_B1) ? 0xFF : 0x00;
        hid_report.pressure_square     = (buttons & USB_GAMEPAD_MASK_B3) ? 0xFF : 0x00;
        hid_report.pressure_l1         = (buttons & USB_GAMEPAD_MASK_L1) ? 0xFF : 0x00;
        hid_report.pressure_r1         = (buttons & USB_GAMEPAD_MASK_R1) ? 0xFF : 0x00;
        // Use analog values for L2/R2 triggers (from DS3 pressure or other analog input)
        hid_report.pressure_l2         = event->analog[ANALOG_RZ];      // Left trigger analog
        hid_report.pressure_r2         = event->analog[ANALOG_SLIDER];  // Right trigger analog
    } else {
        // No input - send neutral state
        memset(&hid_report, 0, sizeof(hid_report));
        hid_report.hat = HID_HAT_CENTER;
        hid_report.lx = 128;
        hid_report.ly = 128;
        hid_report.rx = 128;
        hid_report.ry = 128;
    }

    return tud_hid_report(0, &hid_report, sizeof(hid_report));
}

// Send XInput report (Xbox 360 mode)
static bool usbd_send_xinput_report(uint8_t player_index)
{
    if (!tud_xinput_ready()) {
        return false;
    }

    const input_event_t* event = router_get_output(OUTPUT_TARGET_USB_DEVICE, player_index);

    if (event) {
        // Digital buttons byte 0 (DPAD, Start, Back, L3, R3)
        xinput_report.buttons0 = 0;
        if (event->buttons & USBR_BUTTON_DU) xinput_report.buttons0 |= XINPUT_BTN_DPAD_UP;
        if (event->buttons & USBR_BUTTON_DD) xinput_report.buttons0 |= XINPUT_BTN_DPAD_DOWN;
        if (event->buttons & USBR_BUTTON_DL) xinput_report.buttons0 |= XINPUT_BTN_DPAD_LEFT;
        if (event->buttons & USBR_BUTTON_DR) xinput_report.buttons0 |= XINPUT_BTN_DPAD_RIGHT;
        if (event->buttons & USBR_BUTTON_S2) xinput_report.buttons0 |= XINPUT_BTN_START;
        if (event->buttons & USBR_BUTTON_S1) xinput_report.buttons0 |= XINPUT_BTN_BACK;
        if (event->buttons & USBR_BUTTON_L3) xinput_report.buttons0 |= XINPUT_BTN_L3;
        if (event->buttons & USBR_BUTTON_R3) xinput_report.buttons0 |= XINPUT_BTN_R3;

        // Digital buttons byte 1 (LB, RB, Guide, A, B, X, Y)
        xinput_report.buttons1 = 0;
        if (event->buttons & USBR_BUTTON_L1) xinput_report.buttons1 |= XINPUT_BTN_LB;
        if (event->buttons & USBR_BUTTON_R1) xinput_report.buttons1 |= XINPUT_BTN_RB;
        if (event->buttons & USBR_BUTTON_A1) xinput_report.buttons1 |= XINPUT_BTN_GUIDE;
        if (event->buttons & USBR_BUTTON_B1) xinput_report.buttons1 |= XINPUT_BTN_A;
        if (event->buttons & USBR_BUTTON_B2) xinput_report.buttons1 |= XINPUT_BTN_B;
        if (event->buttons & USBR_BUTTON_B3) xinput_report.buttons1 |= XINPUT_BTN_X;
        if (event->buttons & USBR_BUTTON_B4) xinput_report.buttons1 |= XINPUT_BTN_Y;

        // Analog triggers (0-255)
        xinput_report.trigger_l = (event->buttons & USBR_BUTTON_L2) ? 0xFF : 0x00;
        xinput_report.trigger_r = (event->buttons & USBR_BUTTON_R2) ? 0xFF : 0x00;

        // Analog sticks (signed 16-bit, -32768 to +32767)
        xinput_report.stick_lx = convert_axis_to_s16(event->analog[ANALOG_X]);
        xinput_report.stick_ly = convert_axis_to_s16(event->analog[ANALOG_Y]);
        xinput_report.stick_rx = convert_axis_to_s16(event->analog[ANALOG_Z]);
        xinput_report.stick_ry = convert_axis_to_s16(event->analog[ANALOG_RX]);
    } else {
        // No input - send neutral state
        xinput_report.buttons0 = 0;
        xinput_report.buttons1 = 0;
        xinput_report.trigger_l = 0;
        xinput_report.trigger_r = 0;
        xinput_report.stick_lx = 0;
        xinput_report.stick_ly = 0;
        xinput_report.stick_rx = 0;
        xinput_report.stick_ry = 0;
    }

    return tud_xinput_send_report(&xinput_report);
}

// Send Switch report (Nintendo Switch mode)
static bool usbd_send_switch_report(uint8_t player_index)
{
    if (!tud_hid_ready()) {
        return false;
    }

    const input_event_t* event = router_get_output(OUTPUT_TARGET_USB_DEVICE, player_index);

    if (event) {
        // Buttons (16-bit)
        switch_report.buttons = 0;
        if (event->buttons & USBR_BUTTON_B4) switch_report.buttons |= SWITCH_MASK_Y;  // Y
        if (event->buttons & USBR_BUTTON_B2) switch_report.buttons |= SWITCH_MASK_B;  // B
        if (event->buttons & USBR_BUTTON_B1) switch_report.buttons |= SWITCH_MASK_A;  // A
        if (event->buttons & USBR_BUTTON_B3) switch_report.buttons |= SWITCH_MASK_X;  // X
        if (event->buttons & USBR_BUTTON_L1) switch_report.buttons |= SWITCH_MASK_L;  // L
        if (event->buttons & USBR_BUTTON_R1) switch_report.buttons |= SWITCH_MASK_R;  // R
        if (event->buttons & USBR_BUTTON_L2) switch_report.buttons |= SWITCH_MASK_ZL; // ZL
        if (event->buttons & USBR_BUTTON_R2) switch_report.buttons |= SWITCH_MASK_ZR; // ZR
        if (event->buttons & USBR_BUTTON_S1) switch_report.buttons |= SWITCH_MASK_MINUS;  // Minus
        if (event->buttons & USBR_BUTTON_S2) switch_report.buttons |= SWITCH_MASK_PLUS;   // Plus
        if (event->buttons & USBR_BUTTON_L3) switch_report.buttons |= SWITCH_MASK_L3;
        if (event->buttons & USBR_BUTTON_R3) switch_report.buttons |= SWITCH_MASK_R3;
        if (event->buttons & USBR_BUTTON_A1) switch_report.buttons |= SWITCH_MASK_HOME;
        if (event->buttons & USBR_BUTTON_A2) switch_report.buttons |= SWITCH_MASK_CAPTURE;

        // D-pad as hat switch
        switch_report.hat = convert_dpad_to_hat(event->buttons);

        // Analog sticks (0-255, inverted Y)
        switch_report.lx = event->analog[ANALOG_X];
        switch_report.ly = 255 - event->analog[ANALOG_Y];
        switch_report.rx = event->analog[ANALOG_Z];
        switch_report.ry = 255 - event->analog[ANALOG_RX];

        switch_report.vendor = 0;
    } else {
        // No input - send neutral state
        switch_report.buttons = 0;
        switch_report.hat = SWITCH_HAT_CENTER;
        switch_report.lx = SWITCH_JOYSTICK_MID;
        switch_report.ly = SWITCH_JOYSTICK_MID;
        switch_report.rx = SWITCH_JOYSTICK_MID;
        switch_report.ry = SWITCH_JOYSTICK_MID;
        switch_report.vendor = 0;
    }

    return tud_hid_report(0, &switch_report, sizeof(switch_report));
}

// Send PS3 report (PlayStation 3 DualShock 3 mode)
static bool usbd_send_ps3_report(uint8_t player_index)
{
    if (!tud_hid_ready()) {
        return false;
    }

    const input_event_t* event = router_get_output(OUTPUT_TARGET_USB_DEVICE, player_index);

    if (event) {
        // Digital buttons byte 0
        ps3_report.buttons[0] = 0;
        if (event->buttons & USBR_BUTTON_S1) ps3_report.buttons[0] |= PS3_BTN_SELECT;
        if (event->buttons & USBR_BUTTON_L3) ps3_report.buttons[0] |= PS3_BTN_L3;
        if (event->buttons & USBR_BUTTON_R3) ps3_report.buttons[0] |= PS3_BTN_R3;
        if (event->buttons & USBR_BUTTON_S2) ps3_report.buttons[0] |= PS3_BTN_START;
        if (event->buttons & USBR_BUTTON_DU) ps3_report.buttons[0] |= PS3_BTN_DPAD_UP;
        if (event->buttons & USBR_BUTTON_DR) ps3_report.buttons[0] |= PS3_BTN_DPAD_RIGHT;
        if (event->buttons & USBR_BUTTON_DD) ps3_report.buttons[0] |= PS3_BTN_DPAD_DOWN;
        if (event->buttons & USBR_BUTTON_DL) ps3_report.buttons[0] |= PS3_BTN_DPAD_LEFT;

        // Digital buttons byte 1
        ps3_report.buttons[1] = 0;
        if (event->buttons & USBR_BUTTON_L2) ps3_report.buttons[1] |= PS3_BTN_L2;
        if (event->buttons & USBR_BUTTON_R2) ps3_report.buttons[1] |= PS3_BTN_R2;
        if (event->buttons & USBR_BUTTON_L1) ps3_report.buttons[1] |= PS3_BTN_L1;
        if (event->buttons & USBR_BUTTON_R1) ps3_report.buttons[1] |= PS3_BTN_R1;
        if (event->buttons & USBR_BUTTON_B4) ps3_report.buttons[1] |= PS3_BTN_TRIANGLE;
        if (event->buttons & USBR_BUTTON_B2) ps3_report.buttons[1] |= PS3_BTN_CIRCLE;
        if (event->buttons & USBR_BUTTON_B1) ps3_report.buttons[1] |= PS3_BTN_CROSS;
        if (event->buttons & USBR_BUTTON_B3) ps3_report.buttons[1] |= PS3_BTN_SQUARE;

        // Digital buttons byte 2 (PS button)
        ps3_report.buttons[2] = 0;
        if (event->buttons & USBR_BUTTON_A1) ps3_report.buttons[2] |= PS3_BTN_PS;

        // Analog sticks (0-255, no inversion - PS3 uses raw values)
        // Note: PS3 Y-axis: 0x00=up, 0xFF=down (same as raw HID)
        ps3_report.lx = event->analog[ANALOG_X];
        ps3_report.ly = event->analog[ANALOG_Y];
        ps3_report.rx = event->analog[ANALOG_Z];
        ps3_report.ry = event->analog[ANALOG_RX];

        // Pressure-sensitive buttons (D-pad)
        ps3_report.pressure_up    = (event->buttons & USBR_BUTTON_DU) ? 0xFF : 0x00;
        ps3_report.pressure_right = (event->buttons & USBR_BUTTON_DR) ? 0xFF : 0x00;
        ps3_report.pressure_down  = (event->buttons & USBR_BUTTON_DD) ? 0xFF : 0x00;
        ps3_report.pressure_left  = (event->buttons & USBR_BUTTON_DL) ? 0xFF : 0x00;

        // Pressure-sensitive buttons (triggers/bumpers)
        // Use actual analog values for L2/R2, fall back to digital for L1/R1
        ps3_report.pressure_l2 = event->analog[ANALOG_RZ];      // Left trigger analog
        ps3_report.pressure_r2 = event->analog[ANALOG_SLIDER];  // Right trigger analog
        ps3_report.pressure_l1 = (event->buttons & USBR_BUTTON_L1) ? 0xFF : 0x00;
        ps3_report.pressure_r1 = (event->buttons & USBR_BUTTON_R1) ? 0xFF : 0x00;

        // Pressure-sensitive buttons (face buttons)
        ps3_report.pressure_triangle = (event->buttons & USBR_BUTTON_B4) ? 0xFF : 0x00;
        ps3_report.pressure_circle   = (event->buttons & USBR_BUTTON_B2) ? 0xFF : 0x00;
        ps3_report.pressure_cross    = (event->buttons & USBR_BUTTON_B1) ? 0xFF : 0x00;
        ps3_report.pressure_square   = (event->buttons & USBR_BUTTON_B3) ? 0xFF : 0x00;
    } else {
        // No input - keep neutral state (already initialized)
        ps3_report.buttons[0] = 0;
        ps3_report.buttons[1] = 0;
        ps3_report.buttons[2] = 0;
        ps3_report.lx = PS3_JOYSTICK_MID;
        ps3_report.ly = PS3_JOYSTICK_MID;
        ps3_report.rx = PS3_JOYSTICK_MID;
        ps3_report.ry = PS3_JOYSTICK_MID;
        memset(&ps3_report.pressure_up, 0, 12);  // Clear all 12 pressure bytes
    }

    // Send full report including report_id
    return tud_hid_report(0, &ps3_report, sizeof(ps3_report));
}

bool usbd_send_report(uint8_t player_index)
{
    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL:
            return usbd_send_xid_report(player_index);
        case USB_OUTPUT_MODE_XINPUT:
            return usbd_send_xinput_report(player_index);
        case USB_OUTPUT_MODE_SWITCH:
            return usbd_send_switch_report(player_index);
        case USB_OUTPUT_MODE_PS3:
            return usbd_send_ps3_report(player_index);
        case USB_OUTPUT_MODE_HID:
        default:
            return usbd_send_hid_report(player_index);
    }
}

// Get rumble value from USB host (for feedback to input controllers)
static uint8_t usbd_get_rumble(void)
{
    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL: {
            // Xbox OG has two 16-bit motors - combine to single 8-bit value
            uint16_t max_rumble = (xid_rumble.rumble_l > xid_rumble.rumble_r)
                                  ? xid_rumble.rumble_l : xid_rumble.rumble_r;
            return (uint8_t)(max_rumble >> 8);  // Scale 0-65535 to 0-255
        }
        case USB_OUTPUT_MODE_XINPUT: {
            // XInput has two 8-bit motors - take the stronger one
            return (xinput_output.rumble_l > xinput_output.rumble_r)
                   ? xinput_output.rumble_l : xinput_output.rumble_r;
        }
        case USB_OUTPUT_MODE_PS3: {
            // PS3 has left (large) and right (small, on/off only) motors
            return (ps3_output.rumble_left_force > 0) ? ps3_output.rumble_left_force :
                   (ps3_output.rumble_right_on > 0) ? 0xFF : 0x00;
        }
        default:
            // HID/Switch modes: no standard rumble protocol
            return 0;
    }
}

// ============================================================================
// OUTPUT INTERFACE
// ============================================================================

const OutputInterface usbd_output_interface = {
    .name = "USB",
    .target = OUTPUT_TARGET_USB_DEVICE,
    .init = usbd_init,
    .task = usbd_task,
    .core1_task = NULL,  // Runs from core0 task - doesn't need dedicated core
    .get_rumble = usbd_get_rumble,
    .get_player_led = NULL,
    .get_profile_count = NULL,
    .get_active_profile = NULL,
    .set_active_profile = NULL,
    .get_profile_name = NULL,
    .get_trigger_threshold = NULL,
};

// ============================================================================
// TINYUSB DEVICE CALLBACKS
// ============================================================================

// ============================================================================
// INTERFACE AND ENDPOINT NUMBERS
// ============================================================================

// Interface numbers
enum {
    ITF_NUM_HID = 0,
#if CFG_TUD_CDC >= 1
    ITF_NUM_CDC_0,        // CDC 0 control interface (data port)
    ITF_NUM_CDC_0_DATA,   // CDC 0 data interface
#endif
#if CFG_TUD_CDC >= 2
    ITF_NUM_CDC_1,        // CDC 1 control interface (debug port)
    ITF_NUM_CDC_1_DATA,   // CDC 1 data interface
#endif
    ITF_NUM_TOTAL
};

// Endpoint numbers
#define EPNUM_HID           0x81

#if CFG_TUD_CDC >= 1
#define EPNUM_CDC_0_NOTIF   0x82
#define EPNUM_CDC_0_OUT     0x03
#define EPNUM_CDC_0_IN      0x83
#endif

#if CFG_TUD_CDC >= 2
#define EPNUM_CDC_1_NOTIF   0x84
#define EPNUM_CDC_1_OUT     0x05
#define EPNUM_CDC_1_IN      0x85
#endif

// ============================================================================
// DEVICE DESCRIPTOR
// ============================================================================

// HID mode device descriptor (PS3-compatible DInput)
static const tusb_desc_device_t desc_device_hid = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
#if CFG_TUD_CDC > 0
    // Use IAD for composite device with CDC
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
#else
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
#endif
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_HID_VID,
    .idProduct          = USB_HID_PID,
    .bcdDevice          = USB_HID_BCD,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL:
            return (uint8_t const *)&xbox_og_device_descriptor;
        case USB_OUTPUT_MODE_XINPUT:
            return (uint8_t const *)&xinput_device_descriptor;
        case USB_OUTPUT_MODE_SWITCH:
            return (uint8_t const *)&switch_device_descriptor;
        case USB_OUTPUT_MODE_PS3:
            return (uint8_t const *)&ps3_device_descriptor;
        case USB_OUTPUT_MODE_HID:
        default:
            return (uint8_t const *)&desc_device_hid;
    }
}

// ============================================================================
// CONFIGURATION DESCRIPTOR
// ============================================================================

// HID mode configuration descriptor
#define CONFIG_TOTAL_LEN_HID (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + (CFG_TUD_CDC * TUD_CDC_DESC_LEN))

static const uint8_t desc_configuration_hid[] = {
    // Config: bus powered, max 100mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN_HID, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface 0: HID gamepad
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1),

#if CFG_TUD_CDC >= 1
    // CDC 0: Data port (commands, config)
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),
#endif

#if CFG_TUD_CDC >= 2
    // CDC 1: Debug port (logging)
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 5, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),
#endif
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL:
            return xbox_og_config_descriptor;
        case USB_OUTPUT_MODE_XINPUT:
            return xinput_config_descriptor;
        case USB_OUTPUT_MODE_SWITCH:
            return switch_config_descriptor;
        case USB_OUTPUT_MODE_PS3:
            return ps3_config_descriptor;
        case USB_OUTPUT_MODE_HID:
        default:
            return desc_configuration_hid;
    }
}

// ============================================================================
// STRING DESCRIPTORS
// ============================================================================

// String descriptor indices
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
#if CFG_TUD_CDC >= 1
    STRID_CDC_DATA,
#endif
#if CFG_TUD_CDC >= 2
    STRID_CDC_DEBUG,
#endif
    STRID_COUNT
};

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    // Xbox OG has no string descriptors
    if (output_mode == USB_OUTPUT_MODE_XBOX_ORIGINAL) {
        return NULL;
    }

    static uint16_t _desc_str[32];
    const char *str = NULL;
    uint8_t chr_count;

    switch (index) {
        case STRID_LANGID:
            _desc_str[1] = 0x0409;  // English
            chr_count = 1;
            break;
        case STRID_MANUFACTURER:
            // Mode-specific manufacturer
            if (output_mode == USB_OUTPUT_MODE_XINPUT) {
                str = XINPUT_MANUFACTURER;
            } else if (output_mode == USB_OUTPUT_MODE_SWITCH) {
                str = SWITCH_MANUFACTURER;
            } else if (output_mode == USB_OUTPUT_MODE_PS3) {
                str = PS3_MANUFACTURER;
            } else {
                str = USB_HID_MANUFACTURER;
            }
            break;
        case STRID_PRODUCT:
            // Mode-specific product
            if (output_mode == USB_OUTPUT_MODE_XINPUT) {
                str = XINPUT_PRODUCT;
            } else if (output_mode == USB_OUTPUT_MODE_SWITCH) {
                str = SWITCH_PRODUCT;
            } else if (output_mode == USB_OUTPUT_MODE_PS3) {
                str = PS3_PRODUCT;
            } else {
                str = USB_HID_PRODUCT;
            }
            break;
        case STRID_SERIAL:
            str = usb_serial_str;  // Dynamic from board unique ID
            break;
#if CFG_TUD_CDC >= 1
        case STRID_CDC_DATA:
            str = "USBR Data";
            break;
#endif
#if CFG_TUD_CDC >= 2
        case STRID_CDC_DEBUG:
            str = "USBR Debug";
            break;
#endif
        default:
            return NULL;
    }

    if (str) {
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // First byte is length (in bytes), second byte is descriptor type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

// HID Callbacks
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    // Return mode-specific HID report descriptor
    if (output_mode == USB_OUTPUT_MODE_SWITCH) {
        return switch_report_descriptor;
    }
    if (output_mode == USB_OUTPUT_MODE_PS3) {
        return ps3_report_descriptor;
    }
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)itf;

    // PS3 feature reports
    if (output_mode == USB_OUTPUT_MODE_PS3 && report_type == HID_REPORT_TYPE_FEATURE) {
        uint16_t len = 0;
        switch (report_id) {
            case PS3_REPORT_ID_FEATURE_01:
                len = sizeof(ps3_feature_01);
                if (reqlen < len) len = reqlen;
                memcpy(buffer, ps3_feature_01, len);
                return len;
            case PS3_REPORT_ID_PAIRING: {
                // Pairing info (0xF2) - return dummy BT addresses
                static ps3_pairing_info_t pairing = {0};
                len = sizeof(pairing);
                if (reqlen < len) len = reqlen;
                memcpy(buffer, &pairing, len);
                return len;
            }
            case PS3_REPORT_ID_FEATURE_EF:
                len = sizeof(ps3_feature_ef);
                if (reqlen < len) len = reqlen;
                memcpy(buffer, ps3_feature_ef, len);
                return len;
            case PS3_REPORT_ID_FEATURE_F7:
                len = sizeof(ps3_feature_f7);
                if (reqlen < len) len = reqlen;
                memcpy(buffer, ps3_feature_f7, len);
                return len;
            case PS3_REPORT_ID_FEATURE_F8:
                len = sizeof(ps3_feature_f8);
                if (reqlen < len) len = reqlen;
                memcpy(buffer, ps3_feature_f8, len);
                return len;
            default:
                break;
        }
    }

    // Default: return current input report
    (void)report_id;
    (void)report_type;
    uint16_t len = sizeof(usbretro_hid_report_t);
    if (reqlen < len) len = reqlen;
    memcpy(buffer, &hid_report, len);
    return len;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)itf;
    (void)report_type;

    // PS3 output report (rumble/LED)
    if (output_mode == USB_OUTPUT_MODE_PS3 && bufsize >= sizeof(ps3_out_report_t)) {
        memcpy(&ps3_output, buffer, sizeof(ps3_out_report_t));
        ps3_output_available = true;
        return;
    }

    (void)report_id;
    (void)buffer;
    (void)bufsize;
}

// ============================================================================
// CUSTOM CLASS DRIVER REGISTRATION
// ============================================================================

// Register custom class drivers for vendor-specific modes
usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count)
{
    switch (output_mode) {
        case USB_OUTPUT_MODE_XBOX_ORIGINAL:
            *driver_count = 1;
            return tud_xid_class_driver();

        case USB_OUTPUT_MODE_XINPUT:
            *driver_count = 1;
            return tud_xinput_class_driver();

        default:
            // HID/Switch modes use built-in HID class driver
            *driver_count = 0;
            return NULL;
    }
}

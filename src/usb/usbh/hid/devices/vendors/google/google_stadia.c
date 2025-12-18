// google_stadia.c - Google Stadia Controller driver
#include "google_stadia.h"
#include "core/buttons.h"
#include "core/router/router.h"
#include "core/input_event.h"

// Google Stadia Controller IDs
#define GOOGLE_VID      0x18D1
#define STADIA_PID      0x9400

// Per-device state
typedef struct {
    uint8_t rumble;
    uint8_t player;
} stadia_instance_t;

typedef struct {
    stadia_instance_t instances[CFG_TUH_HID];
} stadia_device_t;

static stadia_device_t stadia_devices[CFG_TUH_DEVICE_MAX + 1];
static stadia_report_t prev_report[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID];

// Check if device is Google Stadia controller
static bool is_google_stadia(uint16_t vid, uint16_t pid) {
    return (vid == GOOGLE_VID && pid == STADIA_PID);
}

// Check if reports differ enough to process
static bool diff_report_stadia(const stadia_report_t* rpt1, const stadia_report_t* rpt2) {
    // Check digital buttons and dpad
    if (rpt1->dpad != rpt2->dpad ||
        rpt1->buttons1 != rpt2->buttons1 ||
        rpt1->buttons2 != rpt2->buttons2) {
        return true;
    }

    // Check analog sticks with deadzone
    if (diff_than_n(rpt1->left_x, rpt2->left_x, 2) ||
        diff_than_n(rpt1->left_y, rpt2->left_y, 2) ||
        diff_than_n(rpt1->right_x, rpt2->right_x, 2) ||
        diff_than_n(rpt1->right_y, rpt2->right_y, 2)) {
        return true;
    }

    // Check triggers
    if (diff_than_n(rpt1->l2_trigger, rpt2->l2_trigger, 2) ||
        diff_than_n(rpt1->r2_trigger, rpt2->r2_trigger, 2)) {
        return true;
    }

    return false;
}

// Initialize device on mount
static bool init_google_stadia(uint8_t dev_addr, uint8_t instance) {
    printf("[Stadia] Device mounted: dev_addr=%d, instance=%d\n", dev_addr, instance);
    memset(&prev_report[dev_addr][instance], 0, sizeof(stadia_report_t));
    prev_report[dev_addr][instance].dpad = 8;  // Neutral
    stadia_devices[dev_addr].instances[instance].rumble = 0;
    stadia_devices[dev_addr].instances[instance].player = 0xff;
    return true;
}

// Process input reports
static void process_google_stadia(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    // Skip report ID if present (0x03 for input report)
    if (len == sizeof(stadia_report_t) + 1 && report[0] == 0x03) {
        report++;
        len--;
    }

    if (len < sizeof(stadia_report_t)) return;

    stadia_report_t stadia_report;
    memcpy(&stadia_report, report, sizeof(stadia_report_t));

    if (diff_report_stadia(&prev_report[dev_addr][instance], &stadia_report)) {
        // Debug logging
        TU_LOG1("(lx, ly, rx, ry, l2, r2) = (%u, %u, %u, %u, %u, %u)\r\n",
                stadia_report.left_x, stadia_report.left_y,
                stadia_report.right_x, stadia_report.right_y,
                stadia_report.l2_trigger, stadia_report.r2_trigger);
        TU_LOG1("DPad = %d ", stadia_report.dpad);
        if (stadia_report.buttons2 & STADIA_BTN2_B1) TU_LOG1("A ");
        if (stadia_report.buttons2 & STADIA_BTN2_B2) TU_LOG1("B ");
        if (stadia_report.buttons2 & STADIA_BTN2_B3) TU_LOG1("X ");
        if (stadia_report.buttons2 & STADIA_BTN2_B4) TU_LOG1("Y ");
        if (stadia_report.buttons2 & STADIA_BTN2_L1) TU_LOG1("L1 ");
        if (stadia_report.buttons2 & STADIA_BTN2_R1) TU_LOG1("R1 ");
        if (stadia_report.buttons1 & STADIA_BTN1_L2) TU_LOG1("L2 ");
        if (stadia_report.buttons1 & STADIA_BTN1_R2) TU_LOG1("R2 ");
        if (stadia_report.buttons1 & STADIA_BTN1_S1) TU_LOG1("Select ");
        if (stadia_report.buttons1 & STADIA_BTN1_S2) TU_LOG1("Start ");
        if (stadia_report.buttons2 & STADIA_BTN2_L3) TU_LOG1("L3 ");
        if (stadia_report.buttons1 & STADIA_BTN1_R3) TU_LOG1("R3 ");
        if (stadia_report.buttons1 & STADIA_BTN1_A1) TU_LOG1("Stadia ");
        TU_LOG1("\r\n");

        // Parse D-pad (hat switch)
        bool dpad_up    = (stadia_report.dpad == 0 || stadia_report.dpad == 1 || stadia_report.dpad == 7);
        bool dpad_right = (stadia_report.dpad >= 1 && stadia_report.dpad <= 3);
        bool dpad_down  = (stadia_report.dpad >= 3 && stadia_report.dpad <= 5);
        bool dpad_left  = (stadia_report.dpad >= 5 && stadia_report.dpad <= 7);

        // Map buttons to JP_BUTTON format
        uint32_t buttons = 0;

        // D-pad
        if (dpad_up)    buttons |= JP_BUTTON_DU;
        if (dpad_down)  buttons |= JP_BUTTON_DD;
        if (dpad_left)  buttons |= JP_BUTTON_DL;
        if (dpad_right) buttons |= JP_BUTTON_DR;

        // Face buttons (from buttons2)
        if (stadia_report.buttons2 & STADIA_BTN2_B1) buttons |= JP_BUTTON_B1;  // A
        if (stadia_report.buttons2 & STADIA_BTN2_B2) buttons |= JP_BUTTON_B2;  // B
        if (stadia_report.buttons2 & STADIA_BTN2_B3) buttons |= JP_BUTTON_B3;  // X
        if (stadia_report.buttons2 & STADIA_BTN2_B4) buttons |= JP_BUTTON_B4;  // Y

        // Shoulders (from buttons2)
        if (stadia_report.buttons2 & STADIA_BTN2_L1) buttons |= JP_BUTTON_L1;
        if (stadia_report.buttons2 & STADIA_BTN2_R1) buttons |= JP_BUTTON_R1;

        // Triggers (from buttons1)
        if (stadia_report.buttons1 & STADIA_BTN1_L2) buttons |= JP_BUTTON_L2;
        if (stadia_report.buttons1 & STADIA_BTN1_R2) buttons |= JP_BUTTON_R2;

        // System buttons (from buttons1)
        if (stadia_report.buttons1 & STADIA_BTN1_S1) buttons |= JP_BUTTON_S1;  // Options/Select
        if (stadia_report.buttons1 & STADIA_BTN1_S2) buttons |= JP_BUTTON_S2;  // Menu/Start

        // Stick clicks
        if (stadia_report.buttons2 & STADIA_BTN2_L3) buttons |= JP_BUTTON_L3;
        if (stadia_report.buttons1 & STADIA_BTN1_R3) buttons |= JP_BUTTON_R3;

        // Guide button (Stadia button)
        if (stadia_report.buttons1 & STADIA_BTN1_A1) buttons |= JP_BUTTON_A1;

        // Process analog sticks (ensure non-zero)
        uint8_t axis_lx = (stadia_report.left_x == 255) ? 255 : stadia_report.left_x + 1;
        uint8_t axis_ly = (stadia_report.left_y == 255) ? 255 : stadia_report.left_y + 1;
        uint8_t axis_rx = (stadia_report.right_x == 255) ? 255 : stadia_report.right_x + 1;
        uint8_t axis_ry = (stadia_report.right_y == 255) ? 255 : stadia_report.right_y + 1;

        ensureAllNonZero(&axis_lx, &axis_ly, &axis_rx, &axis_ry);

        // Submit input event
        input_event_t event = {
            .dev_addr = dev_addr,
            .instance = instance,
            .type = INPUT_TYPE_GAMEPAD,
            .transport = INPUT_TRANSPORT_USB,
            .buttons = buttons,
            .button_count = 14,  // A, B, X, Y, L1, R1, L2, R2, L3, R3, Select, Start, Guide, Capture
            .analog = {axis_lx, axis_ly, axis_rx, axis_ry, 128, stadia_report.l2_trigger, stadia_report.r2_trigger, 128},
            .keys = 0,
        };
        router_submit_input(&event);

        prev_report[dev_addr][instance] = stadia_report;
    }
}

// Send rumble output
static void output_google_stadia(uint8_t dev_addr, uint8_t instance, device_output_config_t* config) {
    bool should_update = (stadia_devices[dev_addr].instances[instance].rumble != config->rumble ||
                          stadia_devices[dev_addr].instances[instance].player != config->player_index + 1 ||
                          config->test);

    if (should_update) {
        stadia_devices[dev_addr].instances[instance].rumble = config->rumble;
        stadia_devices[dev_addr].instances[instance].player = config->test ? config->test : config->player_index + 1;

        // Build rumble report (Report ID 0x05)
        stadia_output_report_t output_report = {0};

        if (config->rumble) {
            // Scale 0-255 to 0-65535
            uint16_t motor_value = (uint16_t)config->rumble * 257;
            output_report.left_motor = motor_value;
            output_report.right_motor = motor_value;
        }

        tuh_hid_send_report(dev_addr, instance, 0x05, &output_report, sizeof(output_report));
    }
}

// Task callback for output
static void task_google_stadia(uint8_t dev_addr, uint8_t instance, device_output_config_t* config) {
    output_google_stadia(dev_addr, instance, config);
}

// Cleanup on unmount
static void unmount_google_stadia(uint8_t dev_addr, uint8_t instance) {
    printf("[Stadia] Device unmounted: dev_addr=%d, instance=%d\n", dev_addr, instance);
    stadia_devices[dev_addr].instances[instance].rumble = 0;
    stadia_devices[dev_addr].instances[instance].player = 0xff;
}

// Device interface
DeviceInterface google_stadia_interface = {
    .name = "Google Stadia Controller",
    .is_device = is_google_stadia,
    .init = init_google_stadia,
    .process = process_google_stadia,
    .task = task_google_stadia,
    .unmount = unmount_google_stadia
};

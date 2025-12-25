// switch2_ble.c - Nintendo Switch 2 Controller BLE Driver
// Handles Switch 2 Pro Controller, Joy-Con 2, and NSO GameCube controller over BLE
//
// Switch 2 controllers use BLE (not classic BR/EDR) with a custom protocol.
// Detection is via manufacturer data (company ID 0x0553) in BLE advertisements.
//
// Reference: BlueRetro upstream/master (June-July 2025)

#include "switch2_ble.h"
#include "bt/bthid/bthid.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include "core/buttons.h"
#include "core/services/players/manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// SWITCH 2 CONSTANTS
// ============================================================================

// Product IDs (VID 0x057E - Nintendo)
#define SW2_LJC_PID     0x2066  // Left Joy-Con 2
#define SW2_RJC_PID     0x2067  // Right Joy-Con 2
#define SW2_PRO2_PID    0x2069  // Pro Controller 2
#define SW2_GC_PID      0x2073  // NSO GameCube Controller

// Button bit positions in the 32-bit button field
#define SW2_Y           0
#define SW2_X           1
#define SW2_B           2
#define SW2_A           3
#define SW2_R_SR        4
#define SW2_R_SL        5
#define SW2_R           6
#define SW2_ZR          7
#define SW2_MINUS       8
#define SW2_PLUS        9
#define SW2_RJ          10  // Right stick click
#define SW2_LJ          11  // Left stick click
#define SW2_HOME        12
#define SW2_CAPTURE     13
#define SW2_C           14  // C button (under right stick on Pro2)
#define SW2_GR          24  // Right grip button
#define SW2_GL          25  // Left grip button
#define SW2_DOWN        16
#define SW2_UP          17
#define SW2_RIGHT       18
#define SW2_LEFT        19
#define SW2_L_SR        20
#define SW2_L_SL        21
#define SW2_L           22
#define SW2_ZL          23

// Axis constants
#define SW2_AXIS_NEUTRAL    0x800   // 2048 - center for 12-bit axes
#define SW2_PRO_AXIS_RANGE  1610    // Pro Controller axis range
#define SW2_GC_AXIS_RANGE   1225    // GameCube main stick range
#define SW2_GC_CSTICK_RANGE 1120    // GameCube C-stick range

// ============================================================================
// DRIVER DATA
// ============================================================================

typedef struct {
    input_event_t event;
    bool initialized;
    uint16_t pid;
} switch2_ble_data_t;

static switch2_ble_data_t switch2_data[BTHID_MAX_DEVICES];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Convert 12-bit axis value (0-4095, neutral 2048) to 8-bit (0-255, neutral 128)
static inline uint8_t axis_12bit_to_8bit(uint16_t value) {
    // Clamp and scale from 0-4095 to 0-255
    if (value > 4095) value = 4095;
    return (uint8_t)(value >> 4);
}

// ============================================================================
// DRIVER IMPLEMENTATION
// ============================================================================

static bool switch2_ble_match(const char* device_name, const uint8_t* class_of_device,
                               uint16_t vendor_id, uint16_t product_id)
{
    (void)class_of_device;  // BLE doesn't use COD
    (void)device_name;      // We match by manufacturer data, not name

    // Match by VID/PID if available (extracted from BLE manufacturer data)
    if (vendor_id == 0x057E) {
        switch (product_id) {
            case SW2_LJC_PID:
            case SW2_RJC_PID:
            case SW2_PRO2_PID:
            case SW2_GC_PID:
                return true;
        }
    }

    return false;
}

static bool switch2_ble_init(bthid_device_t* device)
{
    printf("[SW2_BLE] Init for device: %s\n", device->name);

    // Find free data slot
    for (int i = 0; i < BTHID_MAX_DEVICES; i++) {
        if (!switch2_data[i].initialized) {
            init_input_event(&switch2_data[i].event);
            switch2_data[i].initialized = true;
            switch2_data[i].pid = 0;  // Will be set when we get VID/PID

            switch2_data[i].event.type = INPUT_TYPE_GAMEPAD;
            switch2_data[i].event.transport = INPUT_TRANSPORT_BT_BLE;
            switch2_data[i].event.dev_addr = device->conn_index;
            switch2_data[i].event.instance = 0;
            switch2_data[i].event.button_count = 14;

            device->driver_data = &switch2_data[i];

            return true;
        }
    }

    return false;
}

static void switch2_ble_process_report(bthid_device_t* device, const uint8_t* data, uint16_t len)
{
    switch2_ble_data_t* sw2 = (switch2_ble_data_t*)device->driver_data;
    if (!sw2) {
        printf("[SW2_BLE] process_report: no driver data!\n");
        return;
    }

    // Debug: log first call to process_report
    static bool first_process = true;
    if (first_process) {
        printf("[SW2_BLE] process_report called: len=%d data[0]=0x%02X data[1]=0x%02X\n",
               len, len > 0 ? data[0] : 0, len > 1 ? data[1] : 0);
        first_process = false;
    }

    // Switch 2 reports are 64 bytes via BLE notification
    // May have 0xA1 header from bthid layer
    const uint8_t* report;
    uint16_t report_len;

    if (len >= 64 && data[0] == 0xA1) {
        // Has DATA|INPUT header from bthid layer
        report = data + 1;
        report_len = len - 1;
    } else if (len >= 63) {
        // Raw report (direct from BLE notification) - Switch 2 reports are 63 bytes
        report = data;
        report_len = len;
    } else {
        printf("[SW2_BLE] process_report: report too short (%d bytes)\n", len);
        return;  // Too short
    }

    // Switch 2 input report structure:
    // Bytes 0-3:   Unknown
    // Bytes 4-7:   Buttons (32-bit, little-endian)
    // Bytes 8-9:   Unknown
    // Bytes 10-15: Axes (6 bytes, packed 12-bit values)
    // Bytes 16-59: Unknown
    // Bytes 60-61: Triggers (for GC controller)
    // Byte 62:     Unknown

    if (report_len < 16) return;

    // Debug: print first 16 bytes of report periodically
    static uint32_t report_count = 0;
    static uint32_t last_buttons_raw = 0;
    report_count++;

    // Debug: Print first report only
    if (report_count == 1) {
        uint16_t raw_lx = report[10] | ((report[11] & 0x0F) << 8);
        uint16_t raw_ly = (report[11] >> 4) | (report[12] << 4);
        printf("[SW2_BLE] First report: LX=%d LY=%d btns=0x%02X%02X%02X%02X\n",
               raw_lx, raw_ly, report[7], report[6], report[5], report[4]);
    }

    // Parse buttons (little-endian 32-bit at offset 4)
    uint32_t sw2_buttons = (uint32_t)report[4] |
                           ((uint32_t)report[5] << 8) |
                           ((uint32_t)report[6] << 16) |
                           ((uint32_t)report[7] << 24);

    // Debug: print when buttons change
    if (sw2_buttons != last_buttons_raw) {
        printf("[SW2_BLE] Buttons raw: 0x%08lX (bytes: %02X %02X %02X %02X)\n",
               (unsigned long)sw2_buttons, report[4], report[5], report[6], report[7]);
        printf("[SW2_BLE] Report[0-7]: %02X %02X %02X %02X %02X %02X %02X %02X\n",
               report[0], report[1], report[2], report[3],
               report[4], report[5], report[6], report[7]);
        last_buttons_raw = sw2_buttons;
    }

    // Parse axes (12-bit packed values at offset 10)
    // LX: bytes 10-11 (lower 12 bits)
    // LY: bytes 11-12 (upper 4 of byte 11 + byte 12)
    // RX: bytes 13-14 (lower 12 bits)
    // RY: bytes 14-15 (upper 4 of byte 14 + byte 15)
    uint16_t raw_lx = report[10] | ((report[11] & 0x0F) << 8);
    uint16_t raw_ly = (report[11] >> 4) | (report[12] << 4);
    uint16_t raw_rx = report[13] | ((report[14] & 0x0F) << 8);
    uint16_t raw_ry = (report[14] >> 4) | (report[15] << 4);

    // Convert 12-bit to 8-bit
    uint8_t lx = axis_12bit_to_8bit(raw_lx);
    uint8_t ly = axis_12bit_to_8bit(raw_ly);
    uint8_t rx = axis_12bit_to_8bit(raw_rx);
    uint8_t ry = axis_12bit_to_8bit(raw_ry);

    // Invert Y axes (Switch uses up=high, we use up=low)
    ly = 255 - ly;
    ry = 255 - ry;

    // Parse triggers (for GC controller, at offset 60-61)
    uint8_t lt = 0;
    uint8_t rt = 0;
    if (report_len >= 62) {
        lt = report[60];
        rt = report[61];
    }

    // Map Switch 2 buttons to JP_BUTTON_*
    uint32_t buttons = 0;

    // Face buttons (Nintendo layout: B=right, A=bottom)
    // Map to W3C layout: B1=bottom, B2=right, B3=left, B4=top
    if (sw2_buttons & (1 << SW2_B)) buttons |= JP_BUTTON_B1;  // B -> B1 (bottom)
    if (sw2_buttons & (1 << SW2_A)) buttons |= JP_BUTTON_B2;  // A -> B2 (right)
    if (sw2_buttons & (1 << SW2_Y)) buttons |= JP_BUTTON_B3;  // Y -> B3 (left)
    if (sw2_buttons & (1 << SW2_X)) buttons |= JP_BUTTON_B4;  // X -> B4 (top)

    // Shoulders and triggers
    if (sw2_buttons & (1 << SW2_L))  buttons |= JP_BUTTON_L1;
    if (sw2_buttons & (1 << SW2_R))  buttons |= JP_BUTTON_R1;
    if (sw2_buttons & (1 << SW2_ZL)) buttons |= JP_BUTTON_L2;
    if (sw2_buttons & (1 << SW2_ZR)) buttons |= JP_BUTTON_R2;

    // Start/Select
    if (sw2_buttons & (1 << SW2_MINUS)) buttons |= JP_BUTTON_S1;
    if (sw2_buttons & (1 << SW2_PLUS))  buttons |= JP_BUTTON_S2;

    // Stick clicks
    if (sw2_buttons & (1 << SW2_LJ)) buttons |= JP_BUTTON_L3;
    if (sw2_buttons & (1 << SW2_RJ)) buttons |= JP_BUTTON_R3;

    // D-pad
    if (sw2_buttons & (1 << SW2_UP))    buttons |= JP_BUTTON_DU;
    if (sw2_buttons & (1 << SW2_DOWN))  buttons |= JP_BUTTON_DD;
    if (sw2_buttons & (1 << SW2_LEFT))  buttons |= JP_BUTTON_DL;
    if (sw2_buttons & (1 << SW2_RIGHT)) buttons |= JP_BUTTON_DR;

    // Home/Guide and auxiliary buttons
    if (sw2_buttons & (1 << SW2_HOME))    buttons |= JP_BUTTON_A1;
    if (sw2_buttons & (1 << SW2_CAPTURE)) buttons |= JP_BUTTON_A2;
    if (sw2_buttons & (1 << SW2_C))       buttons |= JP_BUTTON_A3;

    // Grip buttons (L4/R4)
    if (sw2_buttons & (1 << SW2_GL)) buttons |= JP_BUTTON_L4;
    if (sw2_buttons & (1 << SW2_GR)) buttons |= JP_BUTTON_R4;

    // Fill event struct
    sw2->event.buttons = buttons;
    sw2->event.analog[ANALOG_X] = lx;
    sw2->event.analog[ANALOG_Y] = ly;
    sw2->event.analog[ANALOG_Z] = rx;
    sw2->event.analog[ANALOG_RX] = ry;
    sw2->event.analog[ANALOG_RZ] = lt;
    sw2->event.analog[ANALOG_SLIDER] = rt;

    // Submit to router
    router_submit_input(&sw2->event);
}

static void switch2_ble_task(bthid_device_t* device)
{
    (void)device;
    // TODO: Implement rumble output
    // Switch 2 uses LRA haptics, sent to ATT handle 0x0012
}

static void switch2_ble_disconnect(bthid_device_t* device)
{
    printf("[SW2_BLE] Disconnect: %s\n", device->name);

    switch2_ble_data_t* sw2 = (switch2_ble_data_t*)device->driver_data;
    if (sw2) {
        // Clear router state first (sends zeroed input report)
        router_device_disconnected(sw2->event.dev_addr, sw2->event.instance);
        // Remove player assignment
        remove_players_by_address(sw2->event.dev_addr, sw2->event.instance);

        init_input_event(&sw2->event);
        sw2->initialized = false;
    }
}

// ============================================================================
// DRIVER STRUCT
// ============================================================================

const bthid_driver_t switch2_ble_driver = {
    .name = "Nintendo Switch 2 Controller (BLE)",
    .match = switch2_ble_match,
    .init = switch2_ble_init,
    .process_report = switch2_ble_process_report,
    .task = switch2_ble_task,
    .disconnect = switch2_ble_disconnect,
};

void switch2_ble_register(void)
{
    bthid_register_driver(&switch2_ble_driver);
}

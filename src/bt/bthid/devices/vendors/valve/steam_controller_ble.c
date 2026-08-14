// steam_controller_ble.c - Valve Steam Controller 1 (original) over Bluetooth LE
//
// Runs on the nRF51822 BLE firmware (community-flashed / Valve). The Valve GATT
// client (btstack_host.c) subscribes to the input characteristic (100F6C33) and
// prepends report id 0xC1 before routing notifications here.
//
// The BLE reports are DELTA-COMPRESSED: each notification carries only the
// chunks that changed, tagged by a mask. Framing (decoded on hardware):
//
//   [0xC1]        report id (prepended by the GATT client)
//   [0xC0]        segment header (0x80 data | 0x40 last | segment 0)
//   [b2]          low nibble = report type: 0x4 = STATE, 0x5 = STATUS(battery)
//                 high nibble = low bits of the 16-bit chunk mask
//   [b3]          high byte of the chunk mask
//   [chunks...]   present chunks, in ascending mask-bit order
//
//   chunk mask = (b2 & 0xF0) | (b3 << 8), bits (Valve EBLEOptionDataChunksBitmask):
//     0x0010 ButtonChunk1  -> 3 bytes: ulButtons[0..2] (same layout as USB SC1)
//     0x0020 ButtonChunk2  -> 2 bytes: left trigger, right trigger (analog 0..255)
//     0x0040 ButtonChunk3  -> 3 bytes: ulButtons[5..7] (unused here)
//     0x0080 LeftJoystick  -> 4 bytes: stick X, Y (int16)
//     0x0100 LeftTrackpad  -> 4 bytes: left pad X, Y (int16)
//     0x0200 RightTrackpad -> 4 bytes: right pad X, Y (int16)
//
// We accumulate the chunks into a persistent state and emit a full gamepad
// event each report. Report layout + masks verified against live hardware; the
// button bit layout matches the USB SC1 driver.
//
// SPDX-License-Identifier: Apache-2.0

#include "steam_controller_ble.h"
#include "bt/bthid/bthid.h"
#include "core/buttons.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include <stdio.h>
#include <string.h>

// --- chunk mask bits ---
#define SC1B_CHUNK_BUTTONS1   0x0010
#define SC1B_CHUNK_BUTTONS2   0x0020   // triggers live here
#define SC1B_CHUNK_BUTTONS3   0x0040
#define SC1B_CHUNK_LSTICK     0x0080
#define SC1B_CHUNK_LPAD       0x0100
#define SC1B_CHUNK_RPAD       0x0200
#define SC1B_TYPE_STATE       0x04

static inline int16_t sc1b_i16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
// int16 stick/pad (±32767) -> u8 (0..255, 128 = center); 0 reserved for "no data".
static inline uint8_t sc1b_s16_to_u8(int16_t v) {
    int32_t s = ((int32_t)v + 32768) >> 8;
    if (s < 0) s = 0; if (s > 255) s = 255; if (s == 0) s = 1;
    return (uint8_t)s;
}
static inline uint16_t sc1b_pad_x(int16_t v) { return touch_norm_from_s16(v); }
static inline uint16_t sc1b_pad_y(int16_t v) { return (uint16_t)(32767 - (int32_t)v); }

typedef struct {
    input_event_t event;   // MUST be first (bthid_set_battery_level casts to it)
    bool initialized;
    // Accumulated (delta-compressed) state.
    uint8_t  b0, b1, b2;   // ulButtons[0..2]
    uint8_t  lt, rt;       // analog triggers
    int16_t  lsx, lsy;     // left STICK
    int16_t  lpx, lpy;     // left PAD (trackpad)
    int16_t  rx, ry;       // right pad
} sc1_ble_data_t;

static sc1_ble_data_t sc1_data[BTHID_MAX_DEVICES];

static bool sc1_ble_match(const char* device_name, const uint8_t* class_of_device,
                          uint16_t vendor_id, uint16_t product_id, bool is_ble)
{
    (void)class_of_device; (void)device_name;
    if (!is_ble) return false;
    // Match ONLY by the synthetic VID/PID the Valve GATT client assigns the SC1.
    // No name fallback — "SteamController" would otherwise also match the SC2 driver.
    return (vendor_id == SC1_BLE_VID && product_id == SC1_BLE_PID);
}

static bool sc1_ble_init(bthid_device_t* device)
{
    printf("[SC1_BLE] Init for device: %s\n", device->name);
    for (int i = 0; i < BTHID_MAX_DEVICES; i++) {
        if (!sc1_data[i].initialized) {
            sc1_ble_data_t* d = &sc1_data[i];
            memset(d, 0, sizeof(*d));
            init_input_event(&d->event);
            d->initialized = true;
            d->event.type = INPUT_TYPE_GAMEPAD;
            d->event.transport = INPUT_TRANSPORT_BT_BLE;
            d->event.layout = LAYOUT_MODERN_4FACE;
            d->event.dev_addr = device->conn_index;
            d->event.instance = 0;
            d->event.button_count = 18;
            d->lsx = d->lsy = d->lpx = d->lpy = d->rx = d->ry = 0;   // centered
            device->driver_data = d;
            return true;
        }
    }
    return false;
}

static void sc1_ble_process_report(bthid_device_t* device, const uint8_t* data, uint16_t len)
{
    sc1_ble_data_t* d = (sc1_ble_data_t*)device->driver_data;
    if (!d) return;
    // [0xC1][0xC0 seg][b2][b3][chunks...]
    if (len < 4 || data[0] != 0xC1) return;
    if ((data[2] & 0x0F) != SC1B_TYPE_STATE) return;   // ignore STATUS/battery heartbeat here

    uint16_t mask = (uint16_t)(data[2] & 0xF0) | ((uint16_t)data[3] << 8);
    int off = 4;
    if ((mask & SC1B_CHUNK_BUTTONS1) && off + 3 <= len) {
        d->b0 = data[off]; d->b1 = data[off+1]; d->b2 = data[off+2]; off += 3;
    }
    if ((mask & SC1B_CHUNK_BUTTONS2) && off + 2 <= len) {
        d->lt = data[off]; d->rt = data[off+1]; off += 2;
    }
    if ((mask & SC1B_CHUNK_BUTTONS3) && off + 3 <= len) {
        off += 3;   // ulButtons[5..7] — unused
    }
    if ((mask & SC1B_CHUNK_LSTICK) && off + 4 <= len) {
        d->lsx = sc1b_i16(&data[off]); d->lsy = sc1b_i16(&data[off+2]); off += 4;
    }
    if ((mask & SC1B_CHUNK_LPAD) && off + 4 <= len) {
        d->lpx = sc1b_i16(&data[off]); d->lpy = sc1b_i16(&data[off+2]); off += 4;
    }
    if ((mask & SC1B_CHUNK_RPAD) && off + 4 <= len) {
        d->rx = sc1b_i16(&data[off]); d->ry = sc1b_i16(&data[off+2]); off += 4;
    }

    // ---- map accumulated state -> gamepad event ----
    uint32_t buttons = 0;
    uint8_t b0 = d->b0, b1 = d->b1, b2 = d->b2;
    // b0: face + shoulders + trigger full-pull clicks (USB SC1 layout)
    if (b0 & 0x80) buttons |= JP_BUTTON_B1;  // A
    if (b0 & 0x40) buttons |= JP_BUTTON_B3;  // X
    if (b0 & 0x20) buttons |= JP_BUTTON_B2;  // B
    if (b0 & 0x10) buttons |= JP_BUTTON_B4;  // Y
    if (b0 & 0x08) buttons |= JP_BUTTON_L1;  // LB
    if (b0 & 0x04) buttons |= JP_BUTTON_R1;  // RB
    // b1: system + left grip. (D-pad is NOT taken from these bits — it's derived from
    // the left-pad position below, 8-way, matching the USB SC1 driver.)
    if (b1 & 0x10) buttons |= JP_BUTTON_S1;  // ⧉ back/select
    if (b1 & 0x20) buttons |= JP_BUTTON_A1;  // Steam / Home
    if (b1 & 0x40) buttons |= JP_BUTTON_S2;  // ☰ start
    if (b1 & 0x80) buttons |= JP_BUTTON_L4;  // left grip
    // b2: right grip + stick/pad clicks
    if (b2 & 0x01) buttons |= JP_BUTTON_R4;  // right grip
    if (b2 & 0x04) buttons |= JP_BUTTON_R3;  // right pad click -> R3
    if (b2 & 0x40) buttons |= JP_BUTTON_L3;  // stick click -> L3
    bool lpad_click = (b2 & 0x02) != 0;      // left pad pressed
    bool rpad_touch = (b2 & 0x10) != 0;

    // Triggers: analog + a digital press past a partial-pull threshold.
    uint8_t l2 = d->lt, r2 = d->rt;
    if (l2 >= 40 || (b0 & 0x02)) buttons |= JP_BUTTON_L2;
    if (r2 >= 40 || (b0 & 0x01)) buttons |= JP_BUTTON_R2;

    // Left PAD -> 8-way D-pad from its position on click (diagonals; same as USB SC1).
    if (lpad_click) {
        const int32_t DZ = 6000;
        if (d->lpy >  DZ) buttons |= JP_BUTTON_DU;   // +Y = up
        if (d->lpy < -DZ) buttons |= JP_BUTTON_DD;
        if (d->lpx < -DZ) buttons |= JP_BUTTON_DL;
        if (d->lpx >  DZ) buttons |= JP_BUTTON_DR;
    }

    // Left STICK -> left analog stick.
    uint8_t lx = sc1b_s16_to_u8(d->lsx);
    uint8_t ly = (uint8_t)(255 - sc1b_s16_to_u8(d->lsy));
    if (ly == 0) ly = 1;
    // Right PAD -> right analog stick when touched (SC1 has no physical right stick).
    uint8_t rx = 128, ry = 128;
    if (rpad_touch) {
        rx = sc1b_s16_to_u8(d->rx);
        ry = (uint8_t)(255 - sc1b_s16_to_u8(d->ry));
        if (ry == 0) ry = 1;
    }

    input_event_t* e = &d->event;
    e->buttons = buttons;
    e->analog[0] = lx; e->analog[1] = ly;
    e->analog[2] = rx; e->analog[3] = ry;
    e->analog[4] = l2; e->analog[5] = r2;
    // Touchpads intentionally NOT passed through the touch pipe (a later config option
    // could expose them); the pads drive the D-pad / right stick instead.
    e->has_touch = false;
    router_submit_input(e);
}

static void sc1_ble_disconnect(bthid_device_t* device)
{
    sc1_ble_data_t* d = (sc1_ble_data_t*)device->driver_data;
    if (d) d->initialized = false;
}

static const bthid_driver_t steam_controller_ble_driver = {
    .name = "Valve Steam Controller (BLE)",
    .match = sc1_ble_match,
    .init = sc1_ble_init,
    .process_report = sc1_ble_process_report,
    .task = NULL,
    .disconnect = sc1_ble_disconnect,
};

void steam_controller_ble_register(void)
{
    bthid_register_driver(&steam_controller_ble_driver);
}

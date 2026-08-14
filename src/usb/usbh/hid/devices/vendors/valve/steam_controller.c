// steam_controller.c - Valve Steam Controller 1 (the original, codename "D0G")
//
// One driver covers two USB transports:
//   VID 0x28DE PID 0x1102  Direct wired Steam Controller
//   VID 0x28DE PID 0x1142  Steam Controller wireless "dongle" (up to 4 slots)
//
// Like the SC2, the SC1 ships in "lizard mode" — Valve's keyboard + mouse
// emulation. Its vendor (protocol NONE) interface stays silent for gamepad
// data until lizard mode is disabled; only the keyboard/mouse interfaces emit
// (which is why, unhandled, it shows up as a stray "HID Keyboard"). So on a
// heartbeat we disable lizard mode via the SC1 register protocol, after which
// the vendor interface streams the native 64-byte state report parsed here.
//
// The dongle exposes multiple HID interfaces (one keyboard + several vendor
// slots); is_device() matches every interface by VID/PID, and we send the
// lizard-disable to each NONE-protocol interface and parse reports from it.
//
// Report layout + lizard-disable protocol: Linux hid-steam.c + SDL3
// SDL_hidapi_steam.c. Offsets are VERIFIED against raw-report logging on
// hardware (SC1_DEBUG) — do not trust the constants until confirmed.
//
// SPDX-License-Identifier: Apache-2.0

#include "steam_controller.h"
#include "core/buttons.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include "usb/usbh/hid/hid_registry.h"  // device_interfaces[], CONTROLLER_KEYBOARD/MOUSE
#include "platform/platform.h"          // platform_time_ms (lizard heartbeat)
#include "tusb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lizard-mode disable must be re-sent periodically — the firmware re-enables
// keyboard/mouse emulation on its own. SDL refreshes ~every second.
#define SC1_LIZARD_REFRESH_MS 1000

#ifndef SC1_DEBUG
#define SC1_DEBUG 0   // raw-report logging (bring-up only)
#endif

// The SC1 dongle wraps controller data as: [0x01][0x00][ev_type][len][payload...].
// ev_type 0x01 = input state. Require enough bytes for sticks/pads.
#define SC1_MIN_REPORT_LEN 24

// --- Report helpers --------------------------------------------------------

static inline int16_t sc1_i16(const uint8_t *r, int off) {
    return (int16_t)((uint16_t)r[off] | ((uint16_t)r[off + 1] << 8));
}

// Scale signed 16-bit stick/pad (-32768..32767) to unsigned 8-bit (0..255, 128=center).
static inline uint8_t sc1_s16_to_u8(int16_t v) {
    int32_t scaled = ((int32_t)v + 32768) >> 8;
    if (scaled < 0) scaled = 0;
    if (scaled > 255) scaled = 255;
    if (scaled == 0) scaled = 1;   // 0 reserved for "no data"
    return (uint8_t)scaled;
}

// SC1 pads/sticks are native int16 (±32767, +Y = up). Normalize to the
// device-agnostic 0..65535 touch scale (see input_event.h), Y inverted.
static inline uint16_t sc1_pad_x(int16_t v) { return touch_norm_from_s16(v); }
static inline uint16_t sc1_pad_y(int16_t v) { return (uint16_t)(32767 - (int32_t)v); }

// --- Driver state ----------------------------------------------------------

static uint32_t last_lizard_ms[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID];
// Last-known left analog stick, held across reports where the shared left field is
// NOT reporting the stick (pad active, right pad active, etc.) so those don't snap
// the stick to center. Center (128) until first stick sample.
static uint8_t  held_lx[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID];
static uint8_t  held_ly[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID];
// Last time the left pad reported a finger down, to debounce the shared left click
// bit (stick click vs pad click) against the touch flag flickering off mid-press.
static uint32_t last_pad_touch_ms[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID];
#if SC1_DEBUG
static uint8_t  logged_report[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID];
#endif

// --- DeviceInterface callbacks --------------------------------------------

static bool sc1_is_device(uint16_t vid, uint16_t pid) {
    return vid == SC1_VID && (pid == SC1_PID_WIRED || pid == SC1_PID_DONGLE);
}

// Disable "lizard mode" (Valve's default keyboard + mouse emulation) so the
// controller streams its native gamepad state on the vendor interface.
// SC1 protocol (Linux hid-steam.c / SDL):
//   0x81  ID_CLEAR_DIGITAL_MAPPINGS         -> stop keyboard button mappings
//   0x87  ID_SET_SETTINGS_VALUES, register writes to stop mouse emulation:
//         [0x87, len, reg, val_lo, val_hi, ...]
//         LPAD_MODE (0x07) = 0x07, RPAD_MODE (0x08) = 0x07  (mouse off)
// Sent as 64-byte FEATURE reports; must be re-sent on a heartbeat.
static void sc1_disable_lizard(uint8_t dev_addr, uint8_t instance) {
    static uint8_t clear_map[64];
    memset(clear_map, 0, sizeof(clear_map));
    clear_map[0] = 0x81;  // ID_CLEAR_DIGITAL_MAPPINGS
    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_FEATURE,
                       clear_map, sizeof(clear_map));

    static uint8_t settings[64];
    memset(settings, 0, sizeof(settings));
    settings[0] = 0x87;  // ID_SET_SETTINGS_VALUES
    settings[1] = 0x06;  // payload length: 2 register writes x 3 bytes
    settings[2] = 0x07;  // SETTING_LEFT_TRACKPAD_MODE
    settings[3] = 0x07;  // value LO (7 = no mouse/keyboard emulation)
    settings[4] = 0x00;  // value HI
    settings[5] = 0x08;  // SETTING_RIGHT_TRACKPAD_MODE
    settings[6] = 0x07;  // value LO (7 = no mouse)
    settings[7] = 0x00;  // value HI
    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_FEATURE,
                       settings, sizeof(settings));
}

static bool sc1_init(uint8_t dev_addr, uint8_t instance) {
    printf("[SC1] mounted dev_addr=%u instance=%u proto=%u\n",
           dev_addr, instance, tuh_hid_interface_protocol(dev_addr, instance));
    last_lizard_ms[dev_addr][instance] = 0;  // force immediate lizard-disable in task()
    held_lx[dev_addr][instance] = 128;
    held_ly[dev_addr][instance] = 128;
    last_pad_touch_ms[dev_addr][instance] = 0;
#if SC1_DEBUG
    logged_report[dev_addr][instance] = 0;
#endif
    return true;
}

static void sc1_unmount(uint8_t dev_addr, uint8_t instance) {
    printf("[SC1] unmounted dev_addr=%u instance=%u\n", dev_addr, instance);
}

static void sc1_task(uint8_t dev_addr, uint8_t instance,
                     device_output_config_t *config) {
    (void)config;
    // Only the vendor interface (protocol NONE) accepts settings reports.
    if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_NONE) {
        return;
    }
    uint32_t now = platform_time_ms();
    if (now - last_lizard_ms[dev_addr][instance] >= SC1_LIZARD_REFRESH_MS) {
        sc1_disable_lizard(dev_addr, instance);
        last_lizard_ms[dev_addr][instance] = now;
    }
}

static void sc1_process(uint8_t dev_addr, uint8_t instance,
                        const uint8_t *report, uint16_t len) {
    uint8_t itf = tuh_hid_interface_protocol(dev_addr, instance);

    // Lizard (keyboard/mouse) interfaces: DROP them. Forwarding them to the generic
    // handlers made the left pad's arrow-key emulation show up as a phantom D-pad and
    // the right pad's mouse emulation fire random buttons. We drive gamepad output
    // purely from the native vendor stream below; the lizard-disable heartbeat quiets
    // these interfaces anyway.
    if (itf == HID_ITF_PROTOCOL_KEYBOARD || itf == HID_ITF_PROTOCOL_MOUSE) {
        return;
    }

    // SC1 dongle framing: [0]=0x01 [1]=0x00 [2]=ev_type [3]=len. Input = ev 0x01.
    if (len < SC1_MIN_REPORT_LEN || report[0] != 0x01 || report[2] != 0x01) {
        return;
    }

    // NOTE: the physical left stick and the left trackpad SHARE report[16]/[18] —
    // there is no separate stick field (bytes 24+ are unused). The firmware sends
    // stick data when the pad isn't touched and pad data when it is, so the two are
    // mutually exclusive by hardware design (same as Steam / Linux hid-steam.c). We
    // therefore drive the analog stick from that field only while the pad is
    // untouched, and turn a pad press into an 8-way D-pad.

    // ===== Documented layout (Linux drivers/hid/hid-steam.c, steam_do_input_event) =====
    // Buttons: report[8] (b0), report[9] (b1), report[10] (b2, includes pad flags).
    // Triggers: report[11] = LT, report[12] = RT (analog 0..255).
    // Left stick OR left pad: report[16]/[18] (s16). Right pad: report[20]/[22] (s16).
    uint8_t b0 = report[8], b1 = report[9], b2 = report[10];
    uint32_t buttons = 0;
    // report[8]: face + shoulders + analog-trigger full-pull clicks
    if (b0 & 0x80) buttons |= JP_BUTTON_B1;  // A
    if (b0 & 0x40) buttons |= JP_BUTTON_B3;  // X
    if (b0 & 0x20) buttons |= JP_BUTTON_B2;  // B
    if (b0 & 0x10) buttons |= JP_BUTTON_B4;  // Y
    if (b0 & 0x08) buttons |= JP_BUTTON_L1;  // LB
    if (b0 & 0x04) buttons |= JP_BUTTON_R1;  // RB
    // NOTE: report[8] bits 0/1 are the trigger FULL-PULL click detents (only at 100%).
    // The digital L2/R2 buttons are derived from the analog value threshold below so
    // they engage on a partial pull like a normal trigger, not just at the bottom.
    // report[9]: system + left grip. (D-pad is NOT taken from these discrete bits —
    // they're 4-way only. It's derived from the left-pad position below so diagonals
    // work.)
    if (b1 & 0x10) buttons |= JP_BUTTON_S1;  // ⧉ (left menu)  -> Select/Back
    if (b1 & 0x20) buttons |= JP_BUTTON_A1;  // Steam / Home
    if (b1 & 0x40) buttons |= JP_BUTTON_S2;  // ☰ (right menu) -> Start
    if (b1 & 0x80) buttons |= JP_BUTTON_L4;  // left grip (lower back paddle)
    // report[10]: right grip + click/touch flags.
    if (b2 & 0x01) buttons |= JP_BUTTON_R4;  // right grip (lower back paddle)
    bool rpad_click = (b2 & 0x04) != 0;      // right pad pressed (click)
    bool lpad_touch = (b2 & 0x08) != 0;      // left pad finger down
    bool rpad_touch = (b2 & 0x10) != 0;      // right pad finger down
    // SC1 quirk: the analog-STICK click and the left-PAD click report on the SAME bit
    // (b2 0x02). Disambiguate by finger-on-pad — click while touching the pad is a
    // D-pad press; click with no finger on the pad is the stick click (L3). (Without
    // this, clicking the deflected stick fired the D-pad.)
    // Debounce: a left-pad press briefly drops lpad_touch on some frames, which would
    // misread the shared click as a stick click (spurious L3). Treat the pad as
    // "engaged" for a short window after any touch so mid-press flickers stay D-pad.
    uint32_t now_ms = platform_time_ms();
    if (lpad_touch) last_pad_touch_ms[dev_addr][instance] = now_ms;
    bool pad_engaged = (now_ms - last_pad_touch_ms[dev_addr][instance]) < 80;
    bool left_click  = (b2 & 0x02) != 0;
    bool pad_dpad    = left_click && pad_engaged;
    bool stick_click = left_click && !pad_engaged;
    if (stick_click) buttons |= JP_BUTTON_L3;  // stick click (no recent pad touch)
    if (rpad_click)  buttons |= JP_BUTTON_R3;  // right pad click -> R3

    int16_t lx_raw = sc1_i16(report, 16);
    int16_t ly_raw = sc1_i16(report, 18);
    int16_t rpad_x_raw = sc1_i16(report, 20);
    int16_t rpad_y_raw = sc1_i16(report, 22);

#if SC1_DEBUG
    // Trigger probe: dump bytes 6..15 whenever ANY byte in the button/trigger region
    // (8,9,11,12,13,14,15 — skipping 10, the pad-touch flags) changes, to locate the
    // analog trigger ramp regardless of which byte it's in.
    {
        static uint8_t pt[CFG_TUH_DEVICE_MAX + 1][CFG_TUH_HID][7];
        static const int bi[] = {8,9,11,12,13,14,15};
        bool chg = false;
        for (unsigned k=0;k<7;k++) if (report[bi[k]] != pt[dev_addr][instance][k]) chg = true;
        if (chg) {
            for (unsigned k=0;k<7;k++) pt[dev_addr][instance][k] = report[bi[k]];
            printf("[SC1] r6-15:");
            for (int i=6;i<16;i++) printf(" %02X", report[i]);
            printf("\n");
        }
    }
#endif

    // Left field (report[16]/[18]) is the STICK when the pad isn't touched, the left
    // PAD when it is. Priority:
    //   1. pad_dpad (click WHILE touching the pad) -> 8-way D-pad from pad position.
    //   2. pad untouched -> sample the analog stick.
    //   3. pad touched, no click -> hold last stick value (no drag, no snap-center).
    if (pad_dpad) {
        const int32_t DZ = 6000;   // ignore near-center clicks
        if (ly_raw >  DZ) buttons |= JP_BUTTON_DU;   // +Y = up
        if (ly_raw < -DZ) buttons |= JP_BUTTON_DD;
        if (lx_raw < -DZ) buttons |= JP_BUTTON_DL;
        if (lx_raw >  DZ) buttons |= JP_BUTTON_DR;
    } else if (!lpad_touch) {
        // Left pad NOT touched -> the shared field is the stick (centered when the
        // stick is released). Sample it.
        uint8_t v_lx = sc1_s16_to_u8(lx_raw);
        uint8_t v_ly = (uint8_t)(255 - sc1_s16_to_u8(ly_raw));   // +up -> HID 0=up
        if (v_ly == 0) v_ly = 1;
        held_lx[dev_addr][instance] = v_lx;
        held_ly[dev_addr][instance] = v_ly;
    }
    // else: left pad IS touched -> field is pad data, not the stick -> HOLD the last
    // stick value (so a pad touch neither drags nor snap-centers the stick).
    uint8_t lx = held_lx[dev_addr][instance];
    uint8_t ly = held_ly[dev_addr][instance];

    // Right analog stick — the SC1 has no physical right stick, so the right trackpad
    // acts as an absolute right stick when touched (centered when released). This is
    // the standard dual-stick mapping and fixes "right pad maps to nothing".
    uint8_t rx = 128, ry = 128;
    if (rpad_touch) {
        rx = sc1_s16_to_u8(rpad_x_raw);
        ry = (uint8_t)(255 - sc1_s16_to_u8(rpad_y_raw));
        if (ry == 0) ry = 1;
    }

    // Analog triggers (0..255) live at report[11]/[12].
    uint8_t l2 = report[11];
    uint8_t r2 = report[12];
    // Digital L2/R2 engage on a partial pull (~15%), like a normal trigger, instead of
    // only at the SC1's full-pull click detent. Keep the detent as a guaranteed max.
    #define SC1_TRIGGER_THRESHOLD 40
    if (l2 >= SC1_TRIGGER_THRESHOLD || (b0 & 0x02)) buttons |= JP_BUTTON_L2;
    if (r2 >= SC1_TRIGGER_THRESHOLD || (b0 & 0x01)) buttons |= JP_BUTTON_R2;

    input_event_t event = {
        .dev_addr = dev_addr,
        .instance = instance,
        .type = INPUT_TYPE_GAMEPAD,
        .transport = INPUT_TRANSPORT_USB,
        .layout = LAYOUT_MODERN_4FACE,
        .buttons = buttons,
        .button_count = 18,
        .analog = { lx, ly, rx, ry, l2, r2, 0 },
        // Expose both pads as touch points too, for touchpad-aware outputs/visualizers.
        .has_touch = (lpad_touch || rpad_touch),
        .touch = {
            { .x = sc1_pad_x(lx_raw),     .y = sc1_pad_y(ly_raw),     .active = lpad_touch },
            { .x = sc1_pad_x(rpad_x_raw), .y = sc1_pad_y(rpad_y_raw), .active = rpad_touch },
        },
    };
    router_submit_input(&event);
}

DeviceInterface steam_controller_interface = {
    .name = "Valve Steam Controller",
    .is_device = sc1_is_device,
    .init = sc1_init,
    .process = sc1_process,
    .task = sc1_task,
    .unmount = sc1_unmount,
};

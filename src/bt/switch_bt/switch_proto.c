// switch_proto.c - Switch Pro Controller protocol engine (see switch_proto.h)
// SPDX-License-Identifier: Apache-2.0
//
// Clean-room from dekuNukem/Nintendo_Switch_Reverse_Engineering; cross-checked vs
// HOJA/NS-LIB-HID (no code copied). Report layouts, subcommand IDs and offsets are
// public documentation.

#include "switch_proto.h"
#include "switch_spi.h"
#include "core/buttons.h"
#include <string.h>

// Host OUT report 0x01 layout: [0]=id [1]=counter [2..9]=rumble [10]=subcmd [11..]=args
#define OUT_SUBCMD_ID     10
#define OUT_SUBCMD_ARG    11
// Device IN report shared prefix: [1]=timer [2]=batt/conn [3..5]=buttons
// [6..8]=Lstick [9..11]=Rstick [12]=vibration-report. 0x21 tail: [13]=ack [14]=subcmd
// [15..]=payload. 0x30 tail: [13..48]=IMU.
#define IN_ACK            13
#define IN_SUBCMD_ECHO    14
#define IN_PAYLOAD        15

static const switch_proto_hooks_t* g_hooks;
static void* g_ctx;

void switch_proto_init(switch_proto_t* s, const uint8_t mac[6], bool usb,
                       const switch_proto_hooks_t* hooks, void* hook_ctx)
{
    memset(s, 0, sizeof(*s));
    if (mac) memcpy(s->mac, mac, 6);
    s->usb = usb;
    s->report_mode = SW_MODE_SIMPLE;   // host promotes to FULL via subcmd 0x03
    g_hooks = hooks;
    g_ctx = hook_ctx;
}

// --- player-LED bitmask -> player number (dekuNukem: 1..4 solid, 5..8 combos) -----
static uint8_t decode_player(uint8_t mask)
{
    switch (mask & 0x0F) {
        case 0x01: return 1; case 0x03: return 2; case 0x07: return 3; case 0x0F: return 4;
        case 0x09: return 5; case 0x05: return 6; case 0x0D: return 7; case 0x06: return 8;
        default:   return 0;
    }
}

// --- queue a subcommand reply (emitted on the next build_input) --------------------
static void queue_reply(switch_proto_t* s, uint8_t ack, uint8_t subcmd,
                        const uint8_t* payload, uint8_t len)
{
    s->reply_pending = true;
    s->reply_ack = ack;
    s->reply_subcmd = subcmd;
    s->reply_len = (len > sizeof(s->reply_data)) ? sizeof(s->reply_data) : len;
    if (payload && s->reply_len) memcpy(s->reply_data, payload, s->reply_len);
    else memset(s->reply_data, 0, sizeof(s->reply_data));
}

static void handle_subcommand(switch_proto_t* s, const uint8_t* d, uint16_t len)
{
    if (len <= OUT_SUBCMD_ID) return;
    uint8_t sub = d[OUT_SUBCMD_ID];
    const uint8_t* arg = d + OUT_SUBCMD_ARG;
    uint16_t argn = len - OUT_SUBCMD_ARG;
    uint8_t p[35];

    switch (sub) {
    case SW_SUBCMD_DEVICE_INFO: {                  // ACK 0x82
        memset(p, 0, sizeof(p));
        p[0] = 0x04; p[1] = 0x33;                  // firmware version
        p[2] = 0x03; p[3] = 0x02;                  // Pro Controller type
        for (int i = 0; i < 6; i++) p[4 + i] = s->mac[5 - i];  // MAC, big-endian
        p[10] = 0x01;                              // use SPI color = yes
        p[11] = 0x01;
        queue_reply(s, 0x82, sub, p, 12);
        break;
    }
    case SW_SUBCMD_SET_INPUT_MODE:
        if (argn >= 1) s->report_mode = arg[0];    // 0x30 full / 0x3f simple
        queue_reply(s, 0x80, sub, NULL, 0);
        break;
    case SW_SUBCMD_TRIGGER_TIME:                   // ACK 0x83
        memset(p, 0, sizeof(p));                    // report zero elapsed for all
        queue_reply(s, 0x83, sub, p, 18);
        break;
    case SW_SUBCMD_SET_SHIPMENT:
        queue_reply(s, 0x80, sub, NULL, 0);
        break;
    case SW_SUBCMD_SPI_READ: {                     // ACK 0x90
        // args: [0..3]=addr LE, [4]=len.  region = addr byte1, offset = addr byte0.
        uint8_t region = (argn >= 2) ? arg[1] : 0;
        uint8_t off    = (argn >= 1) ? arg[0] : 0;
        uint8_t rlen   = (argn >= 5) ? arg[4] : 0;
        if (rlen > sizeof(p) - 5) rlen = sizeof(p) - 5;
        memset(p, 0, sizeof(p));
        p[0] = off; p[1] = region; p[2] = 0; p[3] = 0; p[4] = rlen;  // echo addr+len
        switch_spi_read(region, off, rlen, &p[5]);
        queue_reply(s, 0x90, sub, p, (uint8_t)(5 + rlen));
        break;
    }
    case SW_SUBCMD_SET_PLAYER_LED:
        if (argn >= 1) {
            s->player_leds = arg[0];
            s->player_number = decode_player(arg[0]);
            if (g_hooks && g_hooks->set_player)
                g_hooks->set_player(g_ctx, s->player_number, arg[0]);
        }
        queue_reply(s, 0x80, sub, NULL, 0);
        break;
    case SW_SUBCMD_ENABLE_IMU:
        if (argn >= 1) {
            s->imu_enabled = (arg[0] != 0);
            if (g_hooks && g_hooks->set_imu) g_hooks->set_imu(g_ctx, s->imu_enabled);
        }
        queue_reply(s, 0x80, sub, NULL, 0);
        break;
    case SW_SUBCMD_ENABLE_VIBRATE:
        if (argn >= 1) s->vibration_enabled = (arg[0] != 0);
        queue_reply(s, 0x80, sub, NULL, 0);
        break;
    case SW_SUBCMD_SET_PAIRING: {                  // ACK 0x81 (BT manual pairing)
        // dekuNukem: arg[0] = phase. Pro Controller pairing string ends 0x68 (Switch).
        static const uint8_t pro_str[25] = {
            0x00,0x25,0x08,'P','r','o',' ','C','o','n','t','r','o','l','l','e','r',
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x68
        };
        memset(p, 0, sizeof(p));
        uint8_t phase = (argn >= 1) ? arg[0] : 0;
        if (phase == 1) {                          // console has no key: send MAC + str
            p[0] = 0x01;
            for (int i = 0; i < 6; i++) p[1 + i] = s->mac[5 - i];
            memcpy(&p[7], pro_str, sizeof(pro_str));
            // capture host MAC (big-endian in arg[1..6]) if present
            if (argn >= 7) { for (int i = 0; i < 6; i++) s->host_mac[i] = arg[6 - i]; s->have_host = true; }
            queue_reply(s, 0x81, sub, p, 32);
        } else if (phase == 2) {                   // generate + return link key ^0xAA
            uint8_t key[16];
            if (g_hooks && g_hooks->random_bytes) g_hooks->random_bytes(g_ctx, key, 16);
            else memset(key, 0, 16);
            if (g_hooks && g_hooks->store_pairing && s->have_host)
                g_hooks->store_pairing(g_ctx, s->host_mac, key);
            p[0] = 0x02;
            for (int i = 0; i < 16; i++) p[1 + i] = key[i] ^ 0xAA;
            queue_reply(s, 0x81, sub, p, 17);
        } else {                                   // phase 3/4: confirm
            p[0] = 0x03;
            queue_reply(s, 0x81, sub, p, 1);
        }
        break;
    }
    case SW_SUBCMD_SET_HCI:                         // power off / stop
        if (g_hooks && g_hooks->shutdown) g_hooks->shutdown(g_ctx);
        break;                                      // no reply
    default:                                        // ack everything else so setup proceeds
        queue_reply(s, 0x80, sub, NULL, 0);
        break;
    }
}

void switch_proto_handle_output(switch_proto_t* s, const uint8_t* data, uint16_t len)
{
    if (!len) return;
    switch (data[0]) {
    case SW_OUT_ID_RUMBLE_SUBCMD:
        // rumble bytes [2..9] — decode amplitude crudely (HD rumble not modeled)
        if (g_hooks && g_hooks->set_rumble && len >= 10) {
            uint8_t l = (data[2] || data[3] || data[4] || data[5]) ? 0xff : 0;
            uint8_t r = (data[6] || data[7] || data[8] || data[9]) ? 0xff : 0;
            g_hooks->set_rumble(g_ctx, l, r);
        }
        handle_subcommand(s, data, len);
        break;
    case SW_OUT_ID_RUMBLE:
        if (g_hooks && g_hooks->set_rumble && len >= 10) {
            uint8_t l = (data[2] || data[3] || data[4] || data[5]) ? 0xff : 0;
            uint8_t r = (data[6] || data[7] || data[8] || data[9]) ? 0xff : 0;
            g_hooks->set_rumble(g_ctx, l, r);
        }
        break;
    case SW_OUT_ID_CMD:
        // USB-only vendor handshake (0x80). Over BT this path is unused.
        break;
    default:
        break;
    }
}

// pack (x,y) 12-bit into 3 bytes, center 2048; input 0..255 (128 center), Y inverted
// to the Switch convention (up = high).
static void pack_stick(uint8_t x8, uint8_t y8, uint8_t out3[3])
{
    uint16_t x = (uint16_t)x8 << 4;              // 0..4080
    uint16_t y = (uint16_t)(255 - y8) << 4;      // invert: HID 0=up -> Switch up=high
    out3[0] = x & 0xFF;
    out3[1] = ((x >> 8) & 0x0F) | ((y & 0x0F) << 4);
    out3[2] = (y >> 4) & 0xFF;
}

// fill the shared standard-report prefix out[1..12] (timer, battery, buttons, sticks)
static void fill_standard(switch_proto_t* s, const switch_input_t* in, uint8_t* out)
{
    out[1] = s->timer++;
    uint8_t bat = in->battery ? in->battery : 8;   // default full if unset
    out[2] = ((bat & 0x0F) << 4) | (in->charging ? 0x01 : 0x00);

    uint32_t b = in->buttons;
    uint8_t right = 0, shared = 0, left = 0;
    // right byte: Y X B A R_SR R_SL R ZR
    if (b & JP_BUTTON_B3) right |= 0x01;  // Y (left face)
    if (b & JP_BUTTON_B4) right |= 0x02;  // X (top face)
    if (b & JP_BUTTON_B1) right |= 0x04;  // B (bottom face)
    if (b & JP_BUTTON_B2) right |= 0x08;  // A (right face)
    if (b & JP_BUTTON_R1) right |= 0x40;  // R
    if (b & JP_BUTTON_R2) right |= 0x80;  // ZR
    // shared byte: - + R3 L3 Home Capture . .
    if (b & JP_BUTTON_S1) shared |= 0x01; // Minus
    if (b & JP_BUTTON_S2) shared |= 0x02; // Plus
    if (b & JP_BUTTON_R3) shared |= 0x04; // R stick
    if (b & JP_BUTTON_L3) shared |= 0x08; // L stick
    if (b & JP_BUTTON_A1) shared |= 0x10; // Home
    if (b & JP_BUTTON_A2) shared |= 0x20; // Capture
    // left byte: Down Up Right Left L_SR L_SL L ZL
    if (b & JP_BUTTON_DD) left |= 0x01;
    if (b & JP_BUTTON_DU) left |= 0x02;
    if (b & JP_BUTTON_DR) left |= 0x04;
    if (b & JP_BUTTON_DL) left |= 0x08;
    if (b & JP_BUTTON_L1) left |= 0x40;   // L
    if (b & JP_BUTTON_L2) left |= 0x80;   // ZL
    out[3] = right; out[4] = shared; out[5] = left;

    pack_stick(in->lx, in->ly, &out[6]);
    pack_stick(in->rx, in->ry, &out[9]);
    out[12] = 0x00;  // vibration report / rumble ack
}

static void fill_imu(switch_proto_t* s, const switch_input_t* in, uint8_t* out)
{
    if (!s->imu_enabled) { memset(&out[13], 0, 36); return; }
    // one 12-byte sample (accel xyz, gyro xyz, int16 LE), replicated across 3 groups
    uint8_t g[12];
    int16_t v[6] = { in->accel_x, in->accel_y, in->accel_z,
                     in->gyro_x,  in->gyro_y,  in->gyro_z };
    for (int i = 0; i < 6; i++) { g[i*2] = v[i] & 0xFF; g[i*2+1] = (v[i] >> 8) & 0xFF; }
    memcpy(&out[13], g, 12);
    memcpy(&out[25], g, 12);
    memcpy(&out[37], g, 12);
}

int switch_proto_build_input(switch_proto_t* s, const switch_input_t* in, uint8_t out[64])
{
    memset(out, 0, 64);

    if (s->reply_pending) {                         // subcommand reply (0x21)
        out[0] = SW_IN_ID_SUBCMD_REPLY;
        fill_standard(s, in, out);
        out[IN_ACK] = s->reply_ack;
        out[IN_SUBCMD_ECHO] = s->reply_subcmd;
        if (s->reply_len) memcpy(&out[IN_PAYLOAD], s->reply_data, s->reply_len);
        s->reply_pending = false;
        return 64;
    }

    if (s->report_mode == SW_MODE_SIMPLE) {         // basic HID report (pre-config)
        out[0] = SW_IN_ID_SIMPLE;
        // 0x3f: 2 button bytes + hat + 4 axes. Minimal fill so the host sees us alive.
        return 12;
    }

    out[0] = SW_IN_ID_FULL;                         // standard full report (0x30)
    fill_standard(s, in, out);
    fill_imu(s, in, out);
    return 64;
}

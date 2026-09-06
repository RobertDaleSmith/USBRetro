// switch_proto.h - Nintendo Switch Pro Controller protocol engine (transport-agnostic)
// SPDX-License-Identifier: Apache-2.0
//
// Clean-room reimplementation from the public reverse-engineering spec
// (dekuNukem/Nintendo_Switch_Reverse_Engineering: bluetooth_hid_notes.md,
// bluetooth_hid_subcommands_notes.md, spi_flash_notes.md). Cross-checked against
// HandHeldLegend's HOJA/NS-LIB-HID for correctness only — no code copied (that code
// is CC BY-NC / BY-NC-SA). The protocol facts (report layouts, subcommand IDs, SPI
// addresses) are public documentation, not copyrightable.
//
// This module owns everything above the transport: the 0x30 input report, the 0x21
// subcommand-reply state machine, and the emulated SPI-flash calibration/factory
// blobs. The BT (or USB) layer feeds host->device OUT reports in via
// switch_proto_handle_output() and pumps device->host IN reports out via
// switch_proto_build_input(); it is oblivious to Switch semantics.

#ifndef SWITCH_PROTO_H
#define SWITCH_PROTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ---- HID report IDs -------------------------------------------------------------
#define SW_OUT_ID_RUMBLE_SUBCMD  0x01  // host->device: rumble + subcommand
#define SW_OUT_ID_RUMBLE         0x10  // host->device: rumble only (no subcommand)
#define SW_OUT_ID_CMD            0x80  // host->device: USB handshake / vendor (USB only)
#define SW_IN_ID_SUBCMD_REPLY    0x21  // device->host: standard report + subcommand reply
#define SW_IN_ID_FULL            0x30  // device->host: standard full input report
#define SW_IN_ID_CMD_ACK         0x81  // device->host: USB handshake ack (USB only)
#define SW_IN_ID_SIMPLE          0x3f  // device->host: basic HID report (pre-handshake)

// ---- Subcommand IDs (dekuNukem bluetooth_hid_subcommands_notes.md) ---------------
#define SW_SUBCMD_GET_STATE      0x00
#define SW_SUBCMD_SET_PAIRING    0x01  // Bluetooth manual pairing / link-key exchange
#define SW_SUBCMD_DEVICE_INFO    0x02
#define SW_SUBCMD_SET_INPUT_MODE 0x03
#define SW_SUBCMD_TRIGGER_TIME   0x04  // elapsed time for buttons held at boot
#define SW_SUBCMD_SET_HCI        0x06  // set HCI state (host requests power-down/stop)
#define SW_SUBCMD_SET_SHIPMENT   0x08
#define SW_SUBCMD_SPI_READ       0x10
#define SW_SUBCMD_SPI_WRITE      0x11
#define SW_SUBCMD_SET_NFC_MCU    0x21  // NFC/IR config
#define SW_SUBCMD_SET_NFC_STATE  0x22
#define SW_SUBCMD_SET_PLAYER_LED 0x30
#define SW_SUBCMD_GET_PLAYER_LED 0x31
#define SW_SUBCMD_SET_HOME_LED   0x38
#define SW_SUBCMD_ENABLE_IMU     0x40
#define SW_SUBCMD_SET_IMU_SENS   0x41
#define SW_SUBCMD_ENABLE_VIBRATE 0x48

// Input-report mode set by subcommand 0x03 (dekuNukem):
#define SW_MODE_FULL             0x30  // standard full report @ 60Hz
#define SW_MODE_NFC_IR           0x31
#define SW_MODE_SIMPLE           0x3f  // simple HID (used before host configures)

// ---- Neutral controller state fed by the wiring layer ---------------------------
// Sticks are 0..255 (128 center) as joypad-os provides; packed to 12-bit here.
// IMU is raw int16 sensor units (accel/gyro); left zero if no sensor.
typedef struct {
    uint32_t buttons;   // JP_BUTTON_* bitmap (see core/buttons.h)
    uint8_t  lx, ly;    // left stick 0..255, 128 = center
    uint8_t  rx, ry;    // right stick
    int16_t  accel_x, accel_y, accel_z;
    int16_t  gyro_x, gyro_y, gyro_z;
    uint8_t  battery;   // 0..8 (8 = full); mapped into the batt/connection byte
    bool     charging;
} switch_input_t;

typedef struct {
    uint8_t  mac[6];            // this controller's BD_ADDR (LSB..MSB as stored)
    uint8_t  report_mode;       // SW_MODE_* (starts SIMPLE until host sets FULL)
    bool     imu_enabled;
    bool     vibration_enabled;
    uint8_t  player_leds;       // raw bitmask from subcmd 0x30
    uint8_t  player_number;     // decoded 1..8 (0 = unset)
    uint8_t  timer;             // rolling timer byte, +1 per input report
    bool     usb;               // transport is USB (affects 0x80 handshake); false = BT

    // Host (Switch) pairing info captured during subcmd 0x01.
    uint8_t  host_mac[6];
    bool     have_host;

    // A pending subcommand reply queued by handle_output(), emitted by build_input().
    bool     reply_pending;
    uint8_t  reply_ack;         // e.g. 0x80 generic, 0x82 devinfo, 0x83 trigger, 0x90 SPI, 0x81 pairing
    uint8_t  reply_subcmd;      // echoed subcommand id
    uint8_t  reply_data[35];    // subcommand-reply payload (fits the 0x21 tail)
    uint8_t  reply_len;         // valid bytes in reply_data
} switch_proto_t;

// Callbacks the wiring layer may set to react to host requests (all optional).
typedef struct {
    void (*set_player)(void* ctx, uint8_t player_number, uint8_t raw_led_mask);
    void (*set_imu)(void* ctx, bool enabled);
    void (*set_rumble)(void* ctx, uint8_t left, uint8_t right);
    // Persist a newly negotiated pairing {host_mac[6], link_key[16]}. link_key may be
    // NULL when only the host MAC is known.
    void (*store_pairing)(void* ctx, const uint8_t host_mac[6], const uint8_t* link_key);
    // Fill n cryptographically-usable random bytes (for the link key).
    void (*random_bytes)(void* ctx, uint8_t* out, size_t n);
    // Request power-down / stop (subcommand 0x06 SET_HCI).
    void (*shutdown)(void* ctx);
} switch_proto_hooks_t;

void switch_proto_init(switch_proto_t* s, const uint8_t mac[6], bool usb,
                       const switch_proto_hooks_t* hooks, void* hook_ctx);

// Ingest one host->device OUT report (report-ID byte included at data[0]). Decodes
// rumble and, for 0x01, the subcommand -> queues a 0x21 reply.
void switch_proto_handle_output(switch_proto_t* s, const uint8_t* data, uint16_t len);

// Build the next device->host IN report into out[64]. Emits a queued 0x21 reply if
// pending, else the periodic 0x30 (or 0x3f before the host sets full mode). Returns
// the report length in bytes (report-ID byte at out[0]).
int switch_proto_build_input(switch_proto_t* s, const switch_input_t* in, uint8_t out[64]);

#endif // SWITCH_PROTO_H

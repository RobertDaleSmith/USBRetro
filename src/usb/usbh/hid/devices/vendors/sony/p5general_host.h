// p5general_host.h - P5General dongle relay (USB host side) + shared auth data
// SPDX-License-Identifier: MIT
//
// Ported from GP2040-CE P5GeneralAuth/P5GeneralAuthUSBListener (MIT). The dongle
// (VID 0x2B81/PID 0x0101) on the USB host port does two jobs: answer the PS5's
// F0/F1/F2 auth, and sign every 64-byte input report (fill hash[8]). Signing is
// over the INTERRUPT endpoints (send report OUT, receive signed report IN); auth
// is over FEATURE reports.
//
// Single-chip model: the device mode (p5general_mode.c) and this host relay share
// one p5general_auth_data_t in RAM (the struct instance is defined in
// p5general_mode.c so it exists even in device-only builds).

#ifndef P5GENERAL_HOST_H
#define P5GENERAL_HOST_H

#include <stdint.h>
#include <stdbool.h>

// Identity + feature report IDs shared by the device mode and the host relay.
#define P5GENERAL_VID                   0x2B81   // PS5-facing device VID + the dongle we match
#define P5GENERAL_PID                   0x0101
#define P5GENERAL_REPORT_ID_INPUT       0x01
#define P5GENERAL_REPORT_ID_OUTPUT      0x02
#define P5GENERAL_REPORT_DEFINITION     0x03     // GET feature: device definition blob
#define P5GENERAL_SET_AUTH_PAYLOAD      0xF0     // SET feature: PS5 -> challenge (F0)
#define P5GENERAL_GET_SIGNATURE_NONCE   0xF1     // GET feature: signature/nonce (F1)
#define P5GENERAL_GET_SIGNING_STATE     0xF2     // GET feature: signing state (F2)

typedef enum {
    P5G_AUTH_IDLE = 0,
    P5G_AUTH_SEND_F0,
    P5G_AUTH_SEND_F0_WAIT,
    P5G_AUTH_RECV_F1,
    P5G_AUTH_RECV_F1_WAIT,
    P5G_AUTH_RECV_F2_DELAY,   // 500ms delay before polling F2
    P5G_AUTH_RECV_F2,
    P5G_AUTH_RECV_F2_WAIT,
} p5general_auth_state_t;

typedef struct {
    uint8_t hash_pending_buffer[64];  // device -> dongle: report to sign
    uint8_t hash_finish_buffer[64];   // dongle -> device: signed report
    uint8_t auth_buffer[64];          // F0/F1/F2 payloads
    uint64_t auth_recv_f2_us;
    bool dongle_ready;
    bool hash_pending;
    bool hash_ready;
    p5general_auth_state_t passthrough_state;
} p5general_auth_data_t;

// Shared instance — DEFINED in p5general_mode.c (always compiled).
extern p5general_auth_data_t p5general_auth_data;

// --- Host side (USB host builds only) ---------------------------------------
// Called unconditionally from hid.c on mount; self-gates on VID/PID 0x2B81/0x0101.
void p5general_host_mount(uint8_t dev_addr, uint8_t instance);
void p5general_host_unmount(uint8_t dev_addr);
// Per-loop task: pushes pending reports to the dongle + drives the F0/F1/F2 SM.
void p5general_host_task(void);
// True if (dev_addr) is the registered dongle — used by hid.c to route its
// interrupt-IN reports here instead of the input pipeline.
bool p5general_host_is_dongle(uint8_t dev_addr);
// The dongle's interrupt-IN report IS the signed report.
void p5general_host_report_received(uint8_t dev_addr, uint8_t instance,
                                    const uint8_t* report, uint16_t len);
// Completion hooks — invoked from the global cbs in sony_ds4.c. Return true if
// consumed.
bool p5general_host_on_get_report_complete(uint8_t dev_addr, uint8_t instance,
                                           uint8_t report_id, uint16_t len);
bool p5general_host_on_set_report_complete(uint8_t dev_addr, uint8_t instance,
                                           uint8_t report_id, uint16_t len);

#endif // P5GENERAL_HOST_H

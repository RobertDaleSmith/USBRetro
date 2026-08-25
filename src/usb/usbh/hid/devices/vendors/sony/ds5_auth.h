// ds5_auth.h - DualSense (PS5) auth passthrough relay
// SPDX-License-Identifier: Apache-2.0
//
// Relays the PS5's controller-authentication challenge to a REAL DualSense
// plugged into the USB host port, and returns its signature — the same
// "controller in the loop" model as ds4_auth (sony_ds4.c), extended for the
// DualSense's auth report set. No keys, no crypto here: a genuine DualSense
// does the signing; we shuttle the feature-report exchange between the PS5
// (our device side, dualsense_mode.c) and the DualSense (our host side).
//
// ============================================================================
// !!! HARDWARE-SNIFF-GATED — NOT YET VERIFIED ON A REAL PS5 !!!
// ============================================================================
// The DualSense auth exchange is NOT publicly documented at the byte level.
// From reverse-engineering notes it reuses the DS4 report family (0xf0 nonce
// SET, 0xf1 signature GET, 0xf2 status GET, 0xf3 reset) and ADDS 0xf4/0xf5,
// with a status code that varies by how many nonce packets were sent. The
// PAGE COUNTS and the exact role of 0xf4/0xf5 below are BEST-EFFORT PLACEHOLDERS
// carried over from the DS4 until a USB capture of a real DualSense<->PS5
// handshake confirms them. Expect to correct DS5_AUTH_*_PAGES and the 0xf4/0xf5
// handling once a trace exists. See .dev/docs/ds5-auth-passthrough-plan.md.
// ============================================================================

#ifndef DS5_AUTH_H
#define DS5_AUTH_H

#include <stdint.h>
#include <stdbool.h>

// DualSense auth feature report IDs (0xf0-0xf3 mirror the DS4; 0xf4/0xf5 are
// the DualSense additions whose exact role is sniff-gated).
#define DS5_AUTH_REPORT_NONCE      0xF0  // SET: console -> controller nonce pages
#define DS5_AUTH_REPORT_SIGNATURE  0xF1  // GET: controller -> console signature pages
#define DS5_AUTH_REPORT_STATUS     0xF2  // GET: controller signing status
#define DS5_AUTH_REPORT_RESET      0xF3  // reset auth state
#define DS5_AUTH_REPORT_EXTRA_SET  0xF4  // SET: DualSense addition (role TBD via sniff)
#define DS5_AUTH_REPORT_EXTRA_GET  0xF5  // GET: DualSense addition (role TBD via sniff)

// !!! PLACEHOLDER page counts — DS4 values, unverified for DualSense !!!
#define DS5_AUTH_PAGE_SIZE         56    // 0x38, same wire page size as DS4 (assumed)
#define DS5_AUTH_NONCE_PAGES       5     // PLACEHOLDER — confirm via sniff
#define DS5_AUTH_SIGNATURE_PAGES   19    // PLACEHOLDER — confirm via sniff
#define DS5_AUTH_REPORT_SIZE       64

typedef enum {
    DS5_AUTH_STATE_IDLE = 0,
    DS5_AUTH_STATE_NONCE_PENDING,
    DS5_AUTH_STATE_SIGNING,
    DS5_AUTH_STATE_READY,
} ds5_auth_state_t;

// --- Host side: register/unregister a real DualSense as the auth device ------
void ds5_auth_register(uint8_t dev_addr, uint8_t instance);
void ds5_auth_unregister(uint8_t dev_addr, uint8_t instance);
bool ds5_auth_is_available(void);
ds5_auth_state_t ds5_auth_get_state(void);

// --- Host-side task: drives the relay to the real DualSense (call each loop) --
void ds5_auth_task(void);

// --- Completion hooks: called from the global tuh_hid_get/set_report_complete
//     callbacks in sony_ds4.c (single global cbs — DS4 and DS5 share them). ---
//     Returns true if the completion was consumed by DS5 auth.
bool ds5_auth_on_get_report_complete(uint8_t dev_addr, uint8_t instance,
                                     uint8_t report_id, uint16_t len);
bool ds5_auth_on_set_report_complete(uint8_t dev_addr, uint8_t instance,
                                     uint8_t report_id, uint16_t len);

// --- Device side: called from dualsense_mode.c auth feature-report dispatch ---
bool ds5_auth_send_nonce(const uint8_t* data, uint16_t len);      // console SET 0xF0
uint16_t ds5_auth_get_next_signature(uint8_t* buffer, uint16_t max_len); // GET 0xF1
uint16_t ds5_auth_get_status(uint8_t* buffer, uint16_t max_len);  // GET 0xF2
void ds5_auth_reset(void);                                        // 0xF3

#endif // DS5_AUTH_H

// ds5_auth.c - DualSense (PS5) auth passthrough relay (see ds5_auth.h banner)
// SPDX-License-Identifier: Apache-2.0
//
// Faithful clone of the ds4_auth proactive cache-and-forward proxy (sony_ds4.c)
// for the DualSense report set. HARDWARE-SNIFF-GATED: page counts and 0xf4/0xf5
// handling are placeholders until a real DualSense<->PS5 trace confirms them.

#include "ds5_auth.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    RELAY_IDLE = 0,
    RELAY_SENDING_RESET,    // GET 0xF3 from the DualSense first
    RELAY_SENDING_NONCE,    // SET 0xF0 nonce pages to the DualSense
    RELAY_WAITING_FOR_SIG,  // poll 0xF2 status
    RELAY_RECEIVING_SIG,    // GET 0xF1 signature pages
} relay_internal_t;

static struct {
    bool     available;
    uint8_t  dev_addr;
    uint8_t  instance;
    ds5_auth_state_t state;
    relay_internal_t internal;
    bool     busy;               // an async host transfer is in flight

    uint8_t  nonce_id;
    uint8_t  nonce_pages_received;
    uint8_t  nonce_page_sending;
    uint8_t  nonce_buffer[DS5_AUTH_PAGE_SIZE * DS5_AUTH_NONCE_PAGES];

    bool     signature_ready;
    uint8_t  signature_pages_fetched;
    uint8_t  signature_page_returning;
    uint8_t  signature_buffer[DS5_AUTH_PAGE_SIZE * DS5_AUTH_SIGNATURE_PAGES];

    uint8_t  report_buffer[DS5_AUTH_REPORT_SIZE];
} s = { 0 };

// ---------------------------------------------------------------------------
// Host side: registration
// ---------------------------------------------------------------------------
void ds5_auth_register(uint8_t dev_addr, uint8_t instance) {
    if (!s.available) {
        s.dev_addr = dev_addr;
        s.instance = instance;
        s.available = true;
        s.state = DS5_AUTH_STATE_IDLE;
        s.internal = RELAY_IDLE;
        printf("[DS5 Auth] Registered DualSense at %d:%d for auth passthrough\n", dev_addr, instance);
    }
}

void ds5_auth_unregister(uint8_t dev_addr, uint8_t instance) {
    if (s.available && s.dev_addr == dev_addr && s.instance == instance) {
        s.available = false;
        s.state = DS5_AUTH_STATE_IDLE;
        s.internal = RELAY_IDLE;
        s.busy = false;
        s.signature_ready = false;
        printf("[DS5 Auth] Unregistered DualSense from auth passthrough\n");
    }
}

bool ds5_auth_is_available(void) { return s.available; }
ds5_auth_state_t ds5_auth_get_state(void) { return s.state; }

// ---------------------------------------------------------------------------
// Device side: console feature-report handlers (called from dualsense_mode.c)
// ---------------------------------------------------------------------------

// Console SET 0xF0: accumulate nonce pages, kick off the relay on the last page.
// Format mirrors the DS4: [nonce_id][page][0][data(56)]...
bool ds5_auth_send_nonce(const uint8_t* data, uint16_t len) {
    if (!s.available) return false;
    if (len < 3 + DS5_AUTH_PAGE_SIZE) return false;

    uint8_t nonce_id = data[0];
    uint8_t page = data[1];
    if (page >= DS5_AUTH_NONCE_PAGES) return false;

    memcpy(&s.nonce_buffer[page * DS5_AUTH_PAGE_SIZE], &data[3], DS5_AUTH_PAGE_SIZE);
    if (page == 0) s.nonce_id = nonce_id;

    if (page == DS5_AUTH_NONCE_PAGES - 1) {
        s.nonce_pages_received = DS5_AUTH_NONCE_PAGES;
        s.signature_ready = false;
        s.signature_pages_fetched = 0;
        s.signature_page_returning = 0;
        s.nonce_page_sending = 0;
        s.internal = RELAY_SENDING_RESET;
        s.state = DS5_AUTH_STATE_NONCE_PENDING;
        printf("[DS5 Auth] All nonce pages received, starting relay to DualSense\n");
    }
    return true;
}

// Console GET 0xF1: return cached signature pages in order.
uint16_t ds5_auth_get_next_signature(uint8_t* buffer, uint16_t max_len) {
    memset(buffer, 0, max_len);
    uint8_t page = s.signature_page_returning;
    if (page >= DS5_AUTH_SIGNATURE_PAGES) return max_len;

    buffer[0] = s.nonce_id;
    buffer[1] = page;
    buffer[2] = 0;
    if (s.signature_ready) {
        memcpy(&buffer[3], &s.signature_buffer[page * DS5_AUTH_PAGE_SIZE], DS5_AUTH_PAGE_SIZE);
    }
    if (s.signature_page_returning < DS5_AUTH_SIGNATURE_PAGES - 1) {
        s.signature_page_returning++;
    }
    return max_len;
}

// Console GET 0xF2: signing status (0 = ready, 16 = still signing).
uint16_t ds5_auth_get_status(uint8_t* buffer, uint16_t max_len) {
    memset(buffer, 0, max_len);
    buffer[0] = s.nonce_id;
    buffer[1] = s.signature_ready ? 0 : 16;
    return max_len;
}

void ds5_auth_reset(void) {
    s.state = DS5_AUTH_STATE_IDLE;
    s.internal = RELAY_IDLE;
    s.busy = false;
    s.signature_ready = false;
    s.signature_page_returning = 0;
}

// ---------------------------------------------------------------------------
// Host side: relay task — drives the async exchange with the real DualSense
// ---------------------------------------------------------------------------
void ds5_auth_task(void) {
    if (!s.available || s.busy) return;

    switch (s.internal) {
        case RELAY_IDLE:
            break;

        case RELAY_SENDING_RESET:
            tuh_hid_get_report(s.dev_addr, s.instance,
                               DS5_AUTH_REPORT_RESET, HID_REPORT_TYPE_FEATURE,
                               s.report_buffer, 8);
            s.busy = true;
            break;

        case RELAY_SENDING_NONCE: {
            uint8_t page = s.nonce_page_sending;
            memset(s.report_buffer, 0, 63);
            s.report_buffer[0] = s.nonce_id;
            s.report_buffer[1] = page;
            s.report_buffer[2] = 0;
            memcpy(&s.report_buffer[3], &s.nonce_buffer[page * DS5_AUTH_PAGE_SIZE], DS5_AUTH_PAGE_SIZE);
            tuh_hid_set_report(s.dev_addr, s.instance,
                               DS5_AUTH_REPORT_NONCE, HID_REPORT_TYPE_FEATURE,
                               s.report_buffer, 63);
            s.busy = true;
            break;
        }

        case RELAY_WAITING_FOR_SIG:
            tuh_hid_get_report(s.dev_addr, s.instance,
                               DS5_AUTH_REPORT_STATUS, HID_REPORT_TYPE_FEATURE,
                               s.report_buffer, 16);
            s.busy = true;
            break;

        case RELAY_RECEIVING_SIG:
            tuh_hid_get_report(s.dev_addr, s.instance,
                               DS5_AUTH_REPORT_SIGNATURE, HID_REPORT_TYPE_FEATURE,
                               s.report_buffer, 64);
            s.busy = true;
            break;
    }
}

// ---------------------------------------------------------------------------
// Completion hooks — invoked from the global cbs in sony_ds4.c
// ---------------------------------------------------------------------------
bool ds5_auth_on_get_report_complete(uint8_t dev_addr, uint8_t instance,
                                     uint8_t report_id, uint16_t len) {
    if (!s.available || dev_addr != s.dev_addr || instance != s.instance) return false;
    s.busy = false;
    if (len == 0) return true;  // transfer failed; task will not advance

    switch (report_id) {
        case DS5_AUTH_REPORT_RESET:
            s.internal = RELAY_SENDING_NONCE;
            break;
        case DS5_AUTH_REPORT_STATUS:
            if (s.report_buffer[1] == 0) {
                s.signature_pages_fetched = 0;
                s.internal = RELAY_RECEIVING_SIG;
            }
            break;
        case DS5_AUTH_REPORT_SIGNATURE:
            memcpy(&s.signature_buffer[s.signature_pages_fetched * DS5_AUTH_PAGE_SIZE],
                   &s.report_buffer[3], DS5_AUTH_PAGE_SIZE);
            s.signature_pages_fetched++;
            if (s.signature_pages_fetched >= DS5_AUTH_SIGNATURE_PAGES) {
                s.internal = RELAY_IDLE;
                s.signature_ready = true;
                s.state = DS5_AUTH_STATE_READY;
                printf("[DS5 Auth] All signature pages received, auth ready\n");
            }
            break;
        default:
            break;
    }
    return true;
}

bool ds5_auth_on_set_report_complete(uint8_t dev_addr, uint8_t instance,
                                     uint8_t report_id, uint16_t len) {
    if (!s.available || dev_addr != s.dev_addr || instance != s.instance) return false;
    s.busy = false;
    if (len == 0) return true;

    if (report_id == DS5_AUTH_REPORT_NONCE) {
        s.nonce_page_sending++;
        if (s.nonce_page_sending >= DS5_AUTH_NONCE_PAGES) {
            s.internal = RELAY_WAITING_FOR_SIG;
            s.state = DS5_AUTH_STATE_SIGNING;
        }
    }
    return true;
}

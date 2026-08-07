// tud_gc_adapter.c - TinyUSB GameCube Adapter class driver
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Custom USB device class driver implementing GameCube Adapter protocol.
// The GC adapter uses vendor class 0xFF with interrupt endpoints.
// Input: 37 bytes (report_id 0x21 + 4 x 9 bytes per port)
// Output: 5 bytes (cmd 0x11 + 4 bytes rumble state)

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_GC_ADAPTER)

#include "tud_gc_adapter.h"
#include <string.h>
#include "../tusb_compat.h"  // usbd_edpt_xfer is_isr shim (0.20.0 vs master)

// ============================================================================
// INTERNAL STATE
// ============================================================================

typedef struct {
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;

    // Endpoint buffers
    CFG_TUD_MEM_ALIGN uint8_t ep_in_buf[CFG_TUD_GC_ADAPTER_EP_BUFSIZE];
    CFG_TUD_MEM_ALIGN uint8_t ep_out_buf[CFG_TUD_GC_ADAPTER_EP_BUFSIZE];

    // Current report data
    gc_adapter_in_report_t in_report;
    gc_adapter_out_report_t out_report;

    // Flags
    bool rumble_available;
} gc_adapter_interface_t;

static gc_adapter_interface_t _gc_adapter_itf;

// ============================================================================
// CLASS DRIVER CALLBACKS
// ============================================================================

static void gc_adapter_init(void)
{
    memset(&_gc_adapter_itf, 0, sizeof(_gc_adapter_itf));
    _gc_adapter_itf.itf_num = 0xFF;
    _gc_adapter_itf.ep_in = 0xFF;
    _gc_adapter_itf.ep_out = 0xFF;

    // Initialize input report
    _gc_adapter_itf.in_report.report_id = GC_ADAPTER_REPORT_ID_INPUT;

    // Initialize all ports as disconnected with neutral analog values
    for (int i = 0; i < 4; i++) {
        _gc_adapter_itf.in_report.port[i].connected = GC_ADAPTER_PORT_NONE;
        _gc_adapter_itf.in_report.port[i].type = GC_ADAPTER_TYPE_NONE;
        _gc_adapter_itf.in_report.port[i].stick_x = 128;
        _gc_adapter_itf.in_report.port[i].stick_y = 128;
        _gc_adapter_itf.in_report.port[i].cstick_x = 128;
        _gc_adapter_itf.in_report.port[i].cstick_y = 128;
        _gc_adapter_itf.in_report.port[i].trigger_l = 0;
        _gc_adapter_itf.in_report.port[i].trigger_r = 0;
    }
}

static bool gc_adapter_deinit(void)
{
    return true;
}

static void gc_adapter_reset(uint8_t rhport)
{
    (void)rhport;
    gc_adapter_init();
}

static uint16_t gc_adapter_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len)
{
    // Verify this is a vendor-specific interface (class 0xFF)
    TU_VERIFY(itf_desc->bInterfaceClass == 0xFF, 0);

    uint16_t const drv_len = (uint16_t)(sizeof(tusb_desc_interface_t) +
                                         itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_VERIFY(max_len >= drv_len, 0);

    _gc_adapter_itf.itf_num = itf_desc->bInterfaceNumber;

    // Parse and open endpoints
    uint8_t const* p_desc = (uint8_t const*)itf_desc;
    p_desc = tu_desc_next(p_desc);  // Move past interface descriptor

    for (uint8_t i = 0; i < itf_desc->bNumEndpoints; i++) {
        tusb_desc_endpoint_t const* ep_desc = (tusb_desc_endpoint_t const*)p_desc;
        TU_VERIFY(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType, 0);
        TU_VERIFY(usbd_edpt_open(rhport, ep_desc), 0);

        if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN) {
            _gc_adapter_itf.ep_in = ep_desc->bEndpointAddress;
        } else {
            _gc_adapter_itf.ep_out = ep_desc->bEndpointAddress;
        }

        p_desc = tu_desc_next(p_desc);
    }

    // Start receiving on OUT endpoint (for rumble commands)
    if (_gc_adapter_itf.ep_out != 0xFF) {
        usbd_edpt_xfer(rhport, _gc_adapter_itf.ep_out, _gc_adapter_itf.ep_out_buf, sizeof(_gc_adapter_itf.ep_out_buf));
    }

    TU_LOG1("[GC_ADAPTER] Opened interface %u, EP IN=0x%02X, EP OUT=0x%02X\r\n",
            _gc_adapter_itf.itf_num, _gc_adapter_itf.ep_in, _gc_adapter_itf.ep_out);

    return drv_len;
}

static bool gc_adapter_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    (void)rhport;
    (void)stage;

    // GC adapter doesn't use control requests - all data goes through interrupt endpoints
    // Log unknown requests for debugging
    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE &&
        request->wIndex == _gc_adapter_itf.itf_num) {
        TU_LOG2("[GC_ADAPTER] Control request: bmReqType=0x%02X bReq=0x%02X wVal=0x%04X wLen=%u\r\n",
                request->bmRequestType, request->bRequest, request->wValue, request->wLength);
    }

    return false;  // STALL unknown requests
}

static bool gc_adapter_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)result;

    if (ep_addr == _gc_adapter_itf.ep_out) {
        // Received rumble command on OUT endpoint
        // Format: 0x11 followed by 4 bytes (one per port)
        if (xferred_bytes >= sizeof(gc_adapter_out_report_t) &&
            _gc_adapter_itf.ep_out_buf[0] == GC_ADAPTER_REPORT_ID_RUMBLE) {
            memcpy(&_gc_adapter_itf.out_report, _gc_adapter_itf.ep_out_buf, sizeof(gc_adapter_out_report_t));
            _gc_adapter_itf.rumble_available = true;
        }

        // Queue next receive
        usbd_edpt_xfer(rhport, _gc_adapter_itf.ep_out, _gc_adapter_itf.ep_out_buf, sizeof(_gc_adapter_itf.ep_out_buf));
    }

    return true;
}

// ============================================================================
// CLASS DRIVER STRUCT
// ============================================================================

static const usbd_class_driver_t _gc_adapter_class_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "GC_ADAPTER",
#else
    .name = NULL,
#endif
    .init             = gc_adapter_init,
    .deinit           = gc_adapter_deinit,
    .reset            = gc_adapter_reset,
    .open             = gc_adapter_open,
    .control_xfer_cb  = gc_adapter_control_xfer_cb,
    .xfer_cb          = gc_adapter_xfer_cb,
    .sof              = NULL,
};

const usbd_class_driver_t* tud_gc_adapter_class_driver(void)
{
    return &_gc_adapter_class_driver;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool tud_gc_adapter_ready(void)
{
    return tud_ready() &&
           (_gc_adapter_itf.ep_in != 0xFF) &&
           !usbd_edpt_busy(0, _gc_adapter_itf.ep_in);
}

bool tud_gc_adapter_send_report(const gc_adapter_in_report_t* report)
{
    TU_VERIFY(report != NULL);
    TU_VERIFY(tud_gc_adapter_ready());

    // Update internal report state
    memcpy(&_gc_adapter_itf.in_report, report, sizeof(gc_adapter_in_report_t));

    // Copy to endpoint buffer
    memcpy(_gc_adapter_itf.ep_in_buf, report, sizeof(gc_adapter_in_report_t));

    // No remote-wakeup call here: TU_VERIFY(tud_gc_adapter_ready()) above already
    // returned while suspended, so anything below it is unreachable in exactly
    // the state that needs waking. Wakeup lives in usbd_try_remote_wakeup().

    return usbd_edpt_xfer(0, _gc_adapter_itf.ep_in, _gc_adapter_itf.ep_in_buf, sizeof(gc_adapter_in_report_t));
}

bool tud_gc_adapter_get_rumble(gc_adapter_out_report_t* rumble)
{
    TU_VERIFY(rumble != NULL);

    if (_gc_adapter_itf.rumble_available) {
        memcpy(rumble, &_gc_adapter_itf.out_report, sizeof(gc_adapter_out_report_t));
        _gc_adapter_itf.rumble_available = false;
        return true;
    }

    return false;
}

#endif // CFG_TUD_ENABLED && CFG_TUD_GC_ADAPTER

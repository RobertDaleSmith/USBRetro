// tud_xinput.c - TinyUSB XInput class driver for Xbox 360
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Custom USB device class driver implementing Xbox 360 XInput protocol.
// XInput uses vendor class 0xFF, subclass 0x5D, protocol 0x01.
//
// Reference: GP2040-CE, OGX-Mini (MIT/BSD-3-Clause)

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_XINPUT)

#include "tud_xinput.h"
#include <string.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

typedef struct {
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;

    // Endpoint buffers
    CFG_TUD_MEM_ALIGN uint8_t ep_in_buf[CFG_TUD_XINPUT_EP_BUFSIZE];
    CFG_TUD_MEM_ALIGN uint8_t ep_out_buf[CFG_TUD_XINPUT_EP_BUFSIZE];

    // Current report data
    xinput_in_report_t in_report;
    xinput_out_report_t out_report;

    // Flags
    bool output_available;
} xinput_interface_t;

static xinput_interface_t _xinput_itf;

// ============================================================================
// CLASS DRIVER CALLBACKS
// ============================================================================

static void xinput_init(void)
{
    memset(&_xinput_itf, 0, sizeof(_xinput_itf));
    _xinput_itf.itf_num = 0xFF;
    _xinput_itf.ep_in = 0xFF;
    _xinput_itf.ep_out = 0xFF;

    // Initialize input report to neutral state
    _xinput_itf.in_report.report_id = 0x00;
    _xinput_itf.in_report.report_size = sizeof(xinput_in_report_t);
}

static bool xinput_deinit(void)
{
    return true;
}

static void xinput_reset(uint8_t rhport)
{
    (void)rhport;
    xinput_init();
}

static uint16_t xinput_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len)
{
    // Verify this is an XInput interface (class 0xFF, subclass 0x5D, protocol 0x01)
    TU_VERIFY(itf_desc->bInterfaceClass == XINPUT_INTERFACE_CLASS, 0);
    TU_VERIFY(itf_desc->bInterfaceSubClass == XINPUT_INTERFACE_SUBCLASS, 0);
    TU_VERIFY(itf_desc->bInterfaceProtocol == XINPUT_INTERFACE_PROTOCOL, 0);

    // Calculate driver length: interface + XInput descriptor (16 bytes) + 2 endpoints
    uint16_t const drv_len = (uint16_t)(sizeof(tusb_desc_interface_t) + 16 +
                                         itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_VERIFY(max_len >= drv_len, 0);

    _xinput_itf.itf_num = itf_desc->bInterfaceNumber;

    // Parse descriptors and open endpoints
    uint8_t const* p_desc = (uint8_t const*)itf_desc;
    p_desc = tu_desc_next(p_desc);  // Move past interface descriptor

    // Skip the XInput proprietary descriptor (type 0x21, length 16)
    if (p_desc[1] == 0x21) {
        p_desc = tu_desc_next(p_desc);
    }

    // Open endpoints
    for (uint8_t i = 0; i < itf_desc->bNumEndpoints; i++) {
        tusb_desc_endpoint_t const* ep_desc = (tusb_desc_endpoint_t const*)p_desc;
        TU_VERIFY(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType, 0);
        TU_VERIFY(usbd_edpt_open(rhport, ep_desc), 0);

        if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN) {
            _xinput_itf.ep_in = ep_desc->bEndpointAddress;
        } else {
            _xinput_itf.ep_out = ep_desc->bEndpointAddress;
        }

        p_desc = tu_desc_next(p_desc);
    }

    // Start receiving on OUT endpoint
    if (_xinput_itf.ep_out != 0xFF) {
        usbd_edpt_xfer(rhport, _xinput_itf.ep_out, _xinput_itf.ep_out_buf, sizeof(_xinput_itf.ep_out_buf));
    }

    TU_LOG1("[XINPUT] Opened interface %u, EP IN=0x%02X, EP OUT=0x%02X\r\n",
            _xinput_itf.itf_num, _xinput_itf.ep_in, _xinput_itf.ep_out);

    return drv_len;
}

static bool xinput_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    (void)rhport;
    (void)stage;

    // XInput doesn't use many control requests - most data goes through interrupt endpoints
    // The host may query vendor-specific requests, but we can STALL them

    if (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) {
        return false;
    }

    if (request->wIndex != _xinput_itf.itf_num) {
        return false;
    }

    // Log unknown requests for debugging
    TU_LOG2("[XINPUT] Control request: bmReqType=0x%02X bReq=0x%02X wVal=0x%04X wLen=%u\r\n",
            request->bmRequestType, request->bRequest, request->wValue, request->wLength);

    return false;  // STALL unknown requests
}

static bool xinput_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)result;

    if (ep_addr == _xinput_itf.ep_out) {
        // Received rumble/LED data on OUT endpoint
        if (xferred_bytes >= sizeof(xinput_out_report_t)) {
            memcpy(&_xinput_itf.out_report, _xinput_itf.ep_out_buf, sizeof(xinput_out_report_t));
            _xinput_itf.output_available = true;
        }

        // Queue next receive
        usbd_edpt_xfer(rhport, _xinput_itf.ep_out, _xinput_itf.ep_out_buf, sizeof(_xinput_itf.ep_out_buf));
    }

    return true;
}

// ============================================================================
// CLASS DRIVER STRUCT
// ============================================================================

static const usbd_class_driver_t _xinput_class_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "XINPUT",
#else
    .name = NULL,
#endif
    .init             = xinput_init,
    .deinit           = xinput_deinit,
    .reset            = xinput_reset,
    .open             = xinput_open,
    .control_xfer_cb  = xinput_control_xfer_cb,
    .xfer_cb          = xinput_xfer_cb,
    .sof              = NULL,
};

const usbd_class_driver_t* tud_xinput_class_driver(void)
{
    return &_xinput_class_driver;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool tud_xinput_ready(void)
{
    return tud_ready() &&
           (_xinput_itf.ep_in != 0xFF) &&
           !usbd_edpt_busy(0, _xinput_itf.ep_in);
}

bool tud_xinput_send_report(const xinput_in_report_t* report)
{
    TU_VERIFY(report != NULL);
    TU_VERIFY(tud_xinput_ready());

    // Update internal report state
    memcpy(&_xinput_itf.in_report, report, sizeof(xinput_in_report_t));

    // Copy to endpoint buffer
    memcpy(_xinput_itf.ep_in_buf, report, sizeof(xinput_in_report_t));

    // Wake host if suspended
    if (tud_suspended()) {
        tud_remote_wakeup();
    }

    return usbd_edpt_xfer(0, _xinput_itf.ep_in, _xinput_itf.ep_in_buf, sizeof(xinput_in_report_t));
}

bool tud_xinput_get_output(xinput_out_report_t* output)
{
    TU_VERIFY(output != NULL);

    if (_xinput_itf.output_available) {
        memcpy(output, &_xinput_itf.out_report, sizeof(xinput_out_report_t));
        _xinput_itf.output_available = false;
        return true;
    }

    return false;
}

#endif // CFG_TUD_ENABLED && CFG_TUD_XINPUT

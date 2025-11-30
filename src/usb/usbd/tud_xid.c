// tud_xid.c - TinyUSB XID class driver for Original Xbox
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Custom USB device class driver implementing Xbox XID protocol.
// The XID protocol uses class 0x58, subclass 0x42 with vendor control requests.
//
// Reference: OGX-Mini (BSD-3-Clause)

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_XID)

#include "tud_xid.h"
#include <string.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

typedef struct {
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;

    // Endpoint buffers
    CFG_TUD_MEM_ALIGN uint8_t ep_in_buf[CFG_TUD_XID_EP_BUFSIZE];
    CFG_TUD_MEM_ALIGN uint8_t ep_out_buf[CFG_TUD_XID_EP_BUFSIZE];

    // Current report data
    xbox_og_in_report_t in_report;
    xbox_og_out_report_t out_report;

    // Flags
    bool rumble_available;
} xid_interface_t;

static xid_interface_t _xid_itf;

// ============================================================================
// CONTROL REQUEST HANDLING
// ============================================================================

// Identify XID-specific control requests
typedef enum {
    XID_REQ_TYPE_GET_REPORT,
    XID_REQ_TYPE_SET_REPORT,
    XID_REQ_TYPE_GET_DESC,
    XID_REQ_TYPE_GET_CAP_IN,
    XID_REQ_TYPE_GET_CAP_OUT,
    XID_REQ_TYPE_UNKNOWN,
} xid_request_type_t;

static xid_request_type_t identify_request(tusb_control_request_t const* request)
{
    // GET_REPORT: Host wants current gamepad state
    if (request->bmRequestType == XID_REQ_GET_REPORT_TYPE &&
        request->bRequest == XID_REQ_GET_REPORT &&
        request->wValue == XID_REQ_GET_REPORT_VAL) {
        return XID_REQ_TYPE_GET_REPORT;
    }

    // SET_REPORT: Host sends rumble command
    if (request->bmRequestType == XID_REQ_SET_REPORT_TYPE &&
        request->bRequest == XID_REQ_SET_REPORT &&
        request->wValue == XID_REQ_SET_REPORT_VAL &&
        request->wLength == sizeof(xbox_og_out_report_t)) {
        return XID_REQ_TYPE_SET_REPORT;
    }

    // GET_DESC: Host wants XID device descriptor
    if (request->bmRequestType == XID_REQ_GET_DESC_TYPE &&
        request->bRequest == XID_REQ_GET_DESC &&
        request->wValue == XID_REQ_GET_DESC_VALUE) {
        return XID_REQ_TYPE_GET_DESC;
    }

    // GET_CAP: Host wants capabilities
    if (request->bmRequestType == XID_REQ_GET_CAP_TYPE &&
        request->bRequest == XID_REQ_GET_CAP) {
        if (request->wValue == XID_REQ_GET_CAP_IN) {
            return XID_REQ_TYPE_GET_CAP_IN;
        }
        if (request->wValue == XID_REQ_GET_CAP_OUT) {
            return XID_REQ_TYPE_GET_CAP_OUT;
        }
    }

    return XID_REQ_TYPE_UNKNOWN;
}

// ============================================================================
// CLASS DRIVER CALLBACKS
// ============================================================================

static void xid_init(void)
{
    memset(&_xid_itf, 0, sizeof(_xid_itf));
    _xid_itf.itf_num = 0xFF;
    _xid_itf.ep_in = 0xFF;
    _xid_itf.ep_out = 0xFF;

    // Initialize input report to neutral state
    _xid_itf.in_report.reserved1 = 0x00;
    _xid_itf.in_report.report_len = sizeof(xbox_og_in_report_t);
}

static bool xid_deinit(void)
{
    return true;
}

static void xid_reset(uint8_t rhport)
{
    (void)rhport;
    xid_init();
}

static uint16_t xid_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len)
{
    // Verify this is an XID interface (class 0x58, subclass 0x42)
    TU_VERIFY(itf_desc->bInterfaceClass == XID_INTERFACE_CLASS, 0);
    TU_VERIFY(itf_desc->bInterfaceSubClass == XID_INTERFACE_SUBCLASS, 0);

    uint16_t const drv_len = (uint16_t)(sizeof(tusb_desc_interface_t) +
                                         itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_VERIFY(max_len >= drv_len, 0);

    _xid_itf.itf_num = itf_desc->bInterfaceNumber;

    // Parse and open endpoints
    uint8_t const* p_desc = (uint8_t const*)itf_desc;
    p_desc = tu_desc_next(p_desc);  // Move past interface descriptor

    for (uint8_t i = 0; i < itf_desc->bNumEndpoints; i++) {
        tusb_desc_endpoint_t const* ep_desc = (tusb_desc_endpoint_t const*)p_desc;
        TU_VERIFY(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType, 0);
        TU_VERIFY(usbd_edpt_open(rhport, ep_desc), 0);

        if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN) {
            _xid_itf.ep_in = ep_desc->bEndpointAddress;
        } else {
            _xid_itf.ep_out = ep_desc->bEndpointAddress;
        }

        p_desc = tu_desc_next(p_desc);
    }

    // Start receiving on OUT endpoint
    if (_xid_itf.ep_out != 0xFF) {
        usbd_edpt_xfer(rhport, _xid_itf.ep_out, _xid_itf.ep_out_buf, sizeof(_xid_itf.ep_out_buf));
    }

    TU_LOG1("[XID] Opened interface %u, EP IN=0x%02X, EP OUT=0x%02X\r\n",
            _xid_itf.itf_num, _xid_itf.ep_in, _xid_itf.ep_out);

    return drv_len;
}

static bool xid_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    // Only handle interface requests
    if (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) {
        return false;
    }

    // Verify interface number
    if (request->wIndex != _xid_itf.itf_num) {
        return false;
    }

    xid_request_type_t req_type = identify_request(request);

    switch (req_type) {
        case XID_REQ_TYPE_GET_REPORT:
            // Host wants current gamepad state via control pipe
            if (stage == CONTROL_STAGE_SETUP) {
                TU_LOG2("[XID] GET_REPORT\r\n");
                uint16_t len = TU_MIN(request->wLength, sizeof(xbox_og_in_report_t));
                tud_control_xfer(rhport, request, &_xid_itf.in_report, len);
            }
            return true;

        case XID_REQ_TYPE_SET_REPORT:
            // Host sends rumble command via control pipe
            if (stage == CONTROL_STAGE_SETUP) {
                TU_LOG2("[XID] SET_REPORT (rumble)\r\n");
                uint16_t len = TU_MIN(request->wLength, sizeof(xbox_og_out_report_t));
                tud_control_xfer(rhport, request, &_xid_itf.out_report, len);
            } else if (stage == CONTROL_STAGE_ACK) {
                // Data received, mark rumble available
                _xid_itf.rumble_available = true;
            }
            return true;

        case XID_REQ_TYPE_GET_DESC:
            // Host wants XID device descriptor
            if (stage == CONTROL_STAGE_SETUP) {
                TU_LOG1("[XID] GET_DESC (XID descriptor)\r\n");
                tud_control_xfer(rhport, request,
                                (void*)xbox_og_xid_descriptor,
                                sizeof(xbox_og_xid_descriptor));
            }
            return true;

        case XID_REQ_TYPE_GET_CAP_IN:
            // Host wants input capabilities
            if (stage == CONTROL_STAGE_SETUP) {
                TU_LOG1("[XID] GET_CAP_IN\r\n");
                tud_control_xfer(rhport, request,
                                (void*)xbox_og_xid_capabilities_in,
                                sizeof(xbox_og_xid_capabilities_in));
            }
            return true;

        case XID_REQ_TYPE_GET_CAP_OUT:
            // Host wants output capabilities
            if (stage == CONTROL_STAGE_SETUP) {
                TU_LOG1("[XID] GET_CAP_OUT\r\n");
                tud_control_xfer(rhport, request,
                                (void*)xbox_og_xid_capabilities_out,
                                sizeof(xbox_og_xid_capabilities_out));
            }
            return true;

        case XID_REQ_TYPE_UNKNOWN:
        default:
            TU_LOG1("[XID] Unknown request: bmReqType=0x%02X bReq=0x%02X wVal=0x%04X\r\n",
                    request->bmRequestType, request->bRequest, request->wValue);
            return false;  // STALL
    }
}

static bool xid_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)result;

    if (ep_addr == _xid_itf.ep_out) {
        // Received rumble data on OUT endpoint
        if (xferred_bytes >= sizeof(xbox_og_out_report_t)) {
            memcpy(&_xid_itf.out_report, _xid_itf.ep_out_buf, sizeof(xbox_og_out_report_t));
            _xid_itf.rumble_available = true;
        }

        // Queue next receive
        usbd_edpt_xfer(rhport, _xid_itf.ep_out, _xid_itf.ep_out_buf, sizeof(_xid_itf.ep_out_buf));
    }

    return true;
}

// ============================================================================
// CLASS DRIVER STRUCT
// ============================================================================

static const usbd_class_driver_t _xid_class_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "XID",
#else
    .name = NULL,
#endif
    .init             = xid_init,
    .deinit           = xid_deinit,
    .reset            = xid_reset,
    .open             = xid_open,
    .control_xfer_cb  = xid_control_xfer_cb,
    .xfer_cb          = xid_xfer_cb,
    .sof              = NULL,
};

const usbd_class_driver_t* tud_xid_class_driver(void)
{
    return &_xid_class_driver;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool tud_xid_ready(void)
{
    return tud_ready() &&
           (_xid_itf.ep_in != 0xFF) &&
           !usbd_edpt_busy(0, _xid_itf.ep_in);
}

bool tud_xid_send_report(const xbox_og_in_report_t* report)
{
    TU_VERIFY(report != NULL);
    TU_VERIFY(tud_xid_ready());

    // Update internal report state
    memcpy(&_xid_itf.in_report, report, sizeof(xbox_og_in_report_t));

    // Copy to endpoint buffer
    memcpy(_xid_itf.ep_in_buf, report, sizeof(xbox_og_in_report_t));

    // Wake host if suspended
    if (tud_suspended()) {
        tud_remote_wakeup();
    }

    return usbd_edpt_xfer(0, _xid_itf.ep_in, _xid_itf.ep_in_buf, sizeof(xbox_og_in_report_t));
}

bool tud_xid_get_rumble(xbox_og_out_report_t* rumble)
{
    TU_VERIFY(rumble != NULL);

    if (_xid_itf.rumble_available) {
        memcpy(rumble, &_xid_itf.out_report, sizeof(xbox_og_out_report_t));
        _xid_itf.rumble_available = false;
        return true;
    }

    return false;
}

#endif // CFG_TUD_ENABLED && CFG_TUD_XID

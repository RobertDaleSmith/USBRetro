// Copyright 2020, Ryan Wendland, usb64
// SPDX-License-Identifier: MIT

#include "tusb_option.h"

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_XINPUT)

#include "host/usbh.h"
#include "host/usbh_classdriver.h"
#include "xinput_host.h"

typedef struct
{
    uint8_t inst_count;
    xinputh_interface_t instances[CFG_TUH_XINPUT];
} xinputh_device_t;

static xinputh_device_t _xinputh_dev[CFG_TUH_DEVICE_MAX];

TU_ATTR_ALWAYS_INLINE static inline xinputh_device_t *get_dev(uint8_t dev_addr)
{
    return &_xinputh_dev[dev_addr - 1];
}

TU_ATTR_ALWAYS_INLINE static inline xinputh_interface_t *get_instance(uint8_t dev_addr, uint8_t instance)
{
    return &_xinputh_dev[dev_addr - 1].instances[instance];
}

static uint8_t get_instance_id_by_epaddr(uint8_t dev_addr, uint8_t ep_addr)
{
    for (uint8_t inst = 0; inst < CFG_TUH_XINPUT; inst++)
    {
        xinputh_interface_t *hid = get_instance(dev_addr, inst);

        if ((ep_addr == hid->ep_in) || (ep_addr == hid->ep_out))
            return inst;
    }

    return 0xff;
}

static uint8_t get_instance_id_by_itfnum(uint8_t dev_addr, uint8_t itf)
{
    for (uint8_t inst = 0; inst < CFG_TUH_XINPUT; inst++)
    {
        xinputh_interface_t *hid = get_instance(dev_addr, inst);

        if ((hid->itf_num == itf) && (hid->ep_in || hid->ep_out))
            return inst;
    }

    return 0xff;
}

static void wait_for_tx_complete(uint8_t dev_addr, uint8_t ep_out)
{
    while (usbh_edpt_busy(dev_addr, ep_out))
        tuh_task();
}

bool tuh_xinput_receive_report(uint8_t dev_addr, uint8_t instance)
{
    xinputh_interface_t *xid_itf = get_instance(dev_addr, instance);
    TU_VERIFY(usbh_edpt_claim(dev_addr, xid_itf->ep_in));

    if ( !usbh_edpt_xfer(dev_addr, xid_itf->ep_in, xid_itf->epin_buf, xid_itf->epin_size) )
    {
        usbh_edpt_claim(dev_addr, xid_itf->ep_in);
        return false;
    }
    return true;
}

bool tuh_xinput_send_report(uint8_t dev_addr, uint8_t instance, const uint8_t *txbuf, uint16_t len)
{
    xinputh_interface_t *xid_itf = get_instance(dev_addr, instance);

    TU_ASSERT(len <= xid_itf->epout_size);
    TU_VERIFY(usbh_edpt_claim(dev_addr, xid_itf->ep_out));

    memcpy(xid_itf->epout_buf, txbuf, len);
    return usbh_edpt_xfer(dev_addr, xid_itf->ep_out, xid_itf->epout_buf, len);
}

bool tuh_xinput_set_led(uint8_t dev_addr, uint8_t instance, uint8_t quadrant, bool block)
{
    xinputh_interface_t *xid_itf = get_instance(dev_addr, instance);
    uint8_t txbuf[32];
    uint16_t len;
    switch (xid_itf->type)
    {
    case XBOX360_WIRELESS:
        memcpy(txbuf, xbox360w_led, sizeof(xbox360w_led));
        txbuf[3] = (quadrant == 0) ? 0x40 : (0x40 | (quadrant + 5));
        len = sizeof(xbox360w_led);
        break;
    case XBOX360_WIRED:
        memcpy(txbuf, xbox360_wired_led, sizeof(xbox360_wired_led));
        txbuf[2] = (quadrant == 0) ? 0 : (quadrant + 5);
        len = sizeof(xbox360_wired_led);
        break;
    default:
        return true;
    }
    bool ret = tuh_xinput_send_report(dev_addr, instance, txbuf, len);
    if (block && ret)
    {
        wait_for_tx_complete(dev_addr, xid_itf->ep_out);
    }
    return ret;
}

bool tuh_xinput_set_rumble(uint8_t dev_addr, uint8_t instance, uint8_t lValue, uint8_t rValue, bool block)
{
    xinputh_interface_t *xid_itf = get_instance(dev_addr, instance);
    uint8_t txbuf[32];
    uint16_t len;

    switch (xid_itf->type)
    {
    case XBOX360_WIRELESS:
        memcpy(txbuf, xbox360w_rumble, sizeof(xbox360w_rumble));
        txbuf[5] = lValue;
        txbuf[6] = rValue;
        len = sizeof(xbox360w_rumble);
        break;
    case XBOX360_WIRED:
        memcpy(txbuf, xbox360_wired_rumble, sizeof(xbox360_wired_rumble));
        txbuf[3] = lValue;
        txbuf[4] = rValue;
        len = sizeof(xbox360_wired_rumble);
        break;
    case XBOXONE:
        memcpy(txbuf, xboxone_rumble, sizeof(xboxone_rumble));
        txbuf[8] = lValue / 2.6f; //Scale is 0 to 100
        txbuf[9] = rValue / 2.6f; //Scale is 0 to 100
        len = sizeof(xboxone_rumble);
        break;
    case XBOXOG:
        memcpy(txbuf, xboxog_rumble, sizeof(xboxog_rumble));
        txbuf[2] = lValue;
        txbuf[3] = lValue;
        txbuf[4] = rValue;
        txbuf[5] = rValue;
        len = sizeof(xboxog_rumble);
        break;
    default:
        return true;
    }
    bool ret = tuh_xinput_send_report(dev_addr, instance, txbuf, len);
    if (block && ret)
    {
        wait_for_tx_complete(dev_addr, xid_itf->ep_out);
    }
    return true;
}

//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
void xinputh_init(void)
{
    tu_memclr(_xinputh_dev, sizeof(_xinputh_dev));
}

bool xinputh_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
    TU_VERIFY(dev_addr <= CFG_TUH_DEVICE_MAX);

    xinput_type_t type = XINPUT_UNKNOWN;
    if (desc_itf->bNumEndpoints < 2)
        type = XINPUT_UNKNOWN;
    else if (desc_itf->bInterfaceSubClass == 0x5D && //Xbox360 wireless bInterfaceSubClass
             desc_itf->bInterfaceProtocol == 0x81)   //Xbox360 wireless bInterfaceProtocol
        type = XBOX360_WIRELESS;
    else if (desc_itf->bInterfaceSubClass == 0x5D && //Xbox360 wired bInterfaceSubClass
             desc_itf->bInterfaceProtocol == 0x01)   //Xbox360 wired bInterfaceProtocol
        type = XBOX360_WIRED;
    else if (desc_itf->bInterfaceSubClass == 0x47 && //Xbone and SX bInterfaceSubClass
             desc_itf->bInterfaceProtocol == 0xD0)   //Xbone and SX bInterfaceProtocol
        type = XBOXONE;
    else if (desc_itf->bInterfaceClass == 0x58 &&  //XboxOG bInterfaceClass
             desc_itf->bInterfaceSubClass == 0x42) //XboxOG bInterfaceSubClass
        type = XBOXOG;

    if (type == XINPUT_UNKNOWN)
    {
        TU_LOG2("XINPUT: Not a valid interface\n");
        return false;
    }

    TU_LOG2("XINPUT opening Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);

    xinputh_device_t *xinput_dev = get_dev(dev_addr);
    TU_ASSERT(xinput_dev->inst_count < CFG_TUH_XINPUT, 0);

    xinputh_interface_t *xid_itf = get_instance(dev_addr, xinput_dev->inst_count);
    xid_itf->itf_num = desc_itf->bInterfaceNumber;
    xid_itf->type = type;

    //Parse descriptor for all endpoints and open them
    uint8_t const *p_desc = (uint8_t const *)desc_itf;
    int endpoint = 0;
    int pos = 0;
    while (endpoint < desc_itf->bNumEndpoints && pos < max_len)
    {
        if (tu_desc_type(p_desc) != TUSB_DESC_ENDPOINT)
        {
            pos += tu_desc_len(p_desc);
            p_desc = tu_desc_next(p_desc);
            continue;
        }
        tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
        TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType);
        TU_ASSERT(tuh_edpt_open(dev_addr, desc_ep));
        if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_OUT)
        {
            xid_itf->ep_out = desc_ep->bEndpointAddress;
            xid_itf->epout_size = tu_edpt_packet_size(desc_ep);
        }
        else
        {
            xid_itf->ep_in = desc_ep->bEndpointAddress;
            xid_itf->epin_size = tu_edpt_packet_size(desc_ep);
        }
        endpoint++;
        pos += tu_desc_len(p_desc);
        p_desc = tu_desc_next(p_desc);
    }

    xinput_dev->inst_count++;
    return true;
}

bool xinputh_set_config(uint8_t dev_addr, uint8_t itf_num)
{
    uint8_t instance = get_instance_id_by_itfnum(dev_addr, itf_num);
    xinputh_interface_t *xid_itf = get_instance(dev_addr, instance);
    xid_itf->connected = true;

    if (xid_itf->type == XBOX360_WIRELESS)
    {
        //Wireless controllers may not be connected yet.
        xid_itf->connected = false;
        tuh_xinput_send_report(dev_addr, instance, xbox360w_inquire_present, sizeof(xbox360w_inquire_present));
        wait_for_tx_complete(dev_addr, xid_itf->ep_out);
    }
    else if (xid_itf->type == XBOX360_WIRED)
    {
    }
    else if (xid_itf->type == XBOXONE)
    {
        uint16_t PID, VID;
        tuh_vid_pid_get(dev_addr, &VID, &PID);

        tuh_xinput_send_report(dev_addr, instance, xboxone_start_input, sizeof(xboxone_start_input));
        wait_for_tx_complete(dev_addr, xid_itf->ep_out);

        //Init packet for XBONE S/Elite controllers (return from bluetooth mode)
        if (VID == 0x045e && (PID == 0x02ea || PID == 0x0b00 || PID == 0x0b12))
        {
            tuh_xinput_send_report(dev_addr, instance, xboxone_s_init, sizeof(xboxone_s_init));
            wait_for_tx_complete(dev_addr, xid_itf->ep_out);
        }

        //Required for PDP aftermarket controllers
        if (VID == 0x0e6f)
        {
            tuh_xinput_send_report(dev_addr, instance, xboxone_pdp_init1, sizeof(xboxone_pdp_init1));
            wait_for_tx_complete(dev_addr, xid_itf->ep_out);
            tuh_xinput_send_report(dev_addr, instance, xboxone_pdp_init2, sizeof(xboxone_pdp_init2));
            wait_for_tx_complete(dev_addr, xid_itf->ep_out);
            tuh_xinput_send_report(dev_addr, instance, xboxone_pdp_init3, sizeof(xboxone_pdp_init3));
            wait_for_tx_complete(dev_addr, xid_itf->ep_out);
        }
    }

    if (tuh_xinput_mount_cb)
    {
        tuh_xinput_mount_cb(dev_addr, instance, xid_itf);
    }

    usbh_driver_set_config_complete(dev_addr, xid_itf->itf_num);
    return true;
}

bool xinputh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    if (result != XFER_RESULT_SUCCESS)
    {
        TU_LOG1("Error: %d\n", result);
        return false;
    }

    uint8_t const dir = tu_edpt_dir(ep_addr);
    uint8_t const instance = get_instance_id_by_epaddr(dev_addr, ep_addr);
    xinputh_interface_t *xid_itf = get_instance(dev_addr, instance);
    xinput_gamepad_t *pad = &xid_itf->pad;
    uint8_t *rdata = xid_itf->epin_buf;

    if (dir == TUSB_DIR_IN)
    {
        TU_LOG2("Get Report callback (%u, %u, %u bytes)\r\n", dev_addr, instance, xferred_bytes);
        TU_LOG2_MEM(xid_itf->epin_buf, xferred_bytes, 2);
        if (xid_itf->type == XBOX360_WIRED)
        {
            #define GET_USHORT(a) (uint16_t)((a)[1] << 8 | (a)[0])
            #define GET_SHORT(a) ((int16_t)GET_USHORT(a))
            if (rdata[1] == 0x14)
            {
                tu_memclr(pad, sizeof(xinput_gamepad_t));
                uint16_t wButtons = rdata[3] << 8 | rdata[2];

                //Map digital buttons
                if (wButtons & (1 << 0)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
                if (wButtons & (1 << 1)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (wButtons & (1 << 2)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (wButtons & (1 << 3)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
                if (wButtons & (1 << 4)) pad->wButtons |= XINPUT_GAMEPAD_START;
                if (wButtons & (1 << 5)) pad->wButtons |= XINPUT_GAMEPAD_BACK;
                if (wButtons & (1 << 6)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
                if (wButtons & (1 << 7)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
                if (wButtons & (1 << 8)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
                if (wButtons & (1 << 9)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
                if (wButtons & (1 << 12)) pad->wButtons |= XINPUT_GAMEPAD_A;
                if (wButtons & (1 << 13)) pad->wButtons |= XINPUT_GAMEPAD_B;
                if (wButtons & (1 << 14)) pad->wButtons |= XINPUT_GAMEPAD_X;
                if (wButtons & (1 << 15)) pad->wButtons |= XINPUT_GAMEPAD_Y;

                //Map the left and right triggers
                pad->bLeftTrigger = rdata[4];
                pad->bRightTrigger = rdata[5];

                //Map analog sticks
                pad->sThumbLX = rdata[7] << 8 | rdata[6];
                pad->sThumbLY = rdata[9] << 8 | rdata[8];
                pad->sThumbRX = rdata[11] << 8 | rdata[10];
                pad->sThumbRY = rdata[13] << 8 | rdata[12];

                xid_itf->new_pad_data = true;
            }           
        }
        else if (xid_itf->type == XBOX360_WIRELESS)
        {
            //Connect/Disconnect packet
            if (rdata[0] & 0x08)
            {
                if (rdata[1] != 0x00 && xid_itf->connected == false)
                {
                    TU_LOG2("XINPUT: WIRELESS CONTROLLER CONNECTED\n");
                    xid_itf->connected = true;
                }
                else if (rdata[1] == 0x00 && xid_itf->connected == true)
                {
                    TU_LOG2("XINPUT: WIRELESS CONTROLLER DISCONNECTED\n");
                    xid_itf->connected = false;
                }
            }

            //Button status packet
            if ((rdata[1] & 1) && rdata[5] == 0x13)
            {
                tu_memclr(pad, sizeof(xinput_gamepad_t));
                uint16_t wButtons = rdata[7] << 8 | rdata[6];

                //Map digital buttons
                if (wButtons & (1 << 0)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
                if (wButtons & (1 << 1)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (wButtons & (1 << 2)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (wButtons & (1 << 3)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
                if (wButtons & (1 << 4)) pad->wButtons |= XINPUT_GAMEPAD_START;
                if (wButtons & (1 << 5)) pad->wButtons |= XINPUT_GAMEPAD_BACK;
                if (wButtons & (1 << 6)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
                if (wButtons & (1 << 7)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
                if (wButtons & (1 << 8)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
                if (wButtons & (1 << 9)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
                if (wButtons & (1 << 12)) pad->wButtons |= XINPUT_GAMEPAD_A;
                if (wButtons & (1 << 13)) pad->wButtons |= XINPUT_GAMEPAD_B;
                if (wButtons & (1 << 14)) pad->wButtons |= XINPUT_GAMEPAD_X;
                if (wButtons & (1 << 15)) pad->wButtons |= XINPUT_GAMEPAD_Y;

                //Map the left and right triggers
                pad->bLeftTrigger = rdata[8];
                pad->bRightTrigger = rdata[9];

                //Map analog sticks
                pad->sThumbLX = rdata[11] << 8 | rdata[10];
                pad->sThumbLY = rdata[13] << 8 | rdata[12];
                pad->sThumbRX = rdata[15] << 8 | rdata[14];
                pad->sThumbRY = rdata[17] << 8 | rdata[16];

                xid_itf->new_pad_data = true;
            }
        }
        else if (xid_itf->type == XBOXONE)
        {
            if (rdata[0] == 0x20)
            {
                tu_memclr(pad, sizeof(xinput_gamepad_t));
                uint16_t wButtons = rdata[5] << 8 | rdata[4];

                //Map digital buttons
                if (wButtons & (1 << 8)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
                if (wButtons & (1 << 9)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (wButtons & (1 << 10)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (wButtons & (1 << 11)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
                if (wButtons & (1 << 2)) pad->wButtons |= XINPUT_GAMEPAD_START;
                if (wButtons & (1 << 3)) pad->wButtons |= XINPUT_GAMEPAD_BACK;
                if (wButtons & (1 << 14)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
                if (wButtons & (1 << 15)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
                if (wButtons & (1 << 12)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
                if (wButtons & (1 << 13)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
                if (wButtons & (1 << 4)) pad->wButtons |= XINPUT_GAMEPAD_A;
                if (wButtons & (1 << 5)) pad->wButtons |= XINPUT_GAMEPAD_B;
                if (wButtons & (1 << 6)) pad->wButtons |= XINPUT_GAMEPAD_X;
                if (wButtons & (1 << 7)) pad->wButtons |= XINPUT_GAMEPAD_Y;

                //Map the left and right triggers
                pad->bLeftTrigger = (rdata[7] << 8 | rdata[6]) >> 2;
                pad->bRightTrigger = (rdata[9] << 8 | rdata[8]) >> 2;

                //Map analog sticks
                pad->sThumbLX = rdata[11] << 8 | rdata[10];
                pad->sThumbLY = rdata[13] << 8 | rdata[12];
                pad->sThumbRX = rdata[15] << 8 | rdata[14];
                pad->sThumbRY = rdata[17] << 8 | rdata[16];

                xid_itf->new_pad_data = true;
            }          
        }
        else if (xid_itf->type == XBOXOG)
        {
            if (rdata[1] == 0x14)
            {
                tu_memclr(pad, sizeof(xinput_gamepad_t));
                uint16_t wButtons = rdata[3] << 8 | rdata[2];

                //Map digital buttons
                if (wButtons & (1 << 0)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
                if (wButtons & (1 << 1)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (wButtons & (1 << 2)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (wButtons & (1 << 3)) pad->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
                if (wButtons & (1 << 4)) pad->wButtons |= XINPUT_GAMEPAD_START;
                if (wButtons & (1 << 5)) pad->wButtons |= XINPUT_GAMEPAD_BACK;
                if (wButtons & (1 << 6)) pad->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
                if (wButtons & (1 << 7)) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

                if (rdata[4] > 0x20) pad->wButtons |= XINPUT_GAMEPAD_A;
                if (rdata[5] > 0x20) pad->wButtons |= XINPUT_GAMEPAD_B;
                if (rdata[6] > 0x20) pad->wButtons |= XINPUT_GAMEPAD_X;
                if (rdata[7] > 0x20) pad->wButtons |= XINPUT_GAMEPAD_Y;
                if (rdata[8] > 0x20) pad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
                if (rdata[9] > 0x20) pad->wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;

                //Map the left and right triggers
                pad->bLeftTrigger = rdata[10];
                pad->bRightTrigger = rdata[11];

                //Map analog sticks
                pad->sThumbLX = rdata[13] << 8 | rdata[12];
                pad->sThumbLY = rdata[15] << 8 | rdata[14];
                pad->sThumbRX = rdata[17] << 8 | rdata[16];
                pad->sThumbRY = rdata[19] << 8 | rdata[18];

                xid_itf->new_pad_data = true;
            }
        } 
        tuh_xinput_report_received_cb(dev_addr, instance, (const uint8_t *)xid_itf, sizeof(xinputh_interface_t));
        xid_itf->new_pad_data = false;
    }
    else
    {
        if (tuh_xinput_report_sent_cb)
        {
            tuh_xinput_report_sent_cb(dev_addr, instance, xid_itf->epout_buf, xferred_bytes);
        }
    }

    return true;
}

void xinputh_close(uint8_t dev_addr)
{
    TU_VERIFY(dev_addr <= CFG_TUH_DEVICE_MAX, );
    xinputh_device_t *xinput_dev = get_dev(dev_addr);

    for (uint8_t inst = 0; inst < xinput_dev->inst_count; inst++)
    {
        if (tuh_xinput_umount_cb)
        {
            tuh_xinput_umount_cb(dev_addr, inst);
        }
    }
    tu_memclr(xinput_dev, sizeof(xinputh_device_t));
}

#endif

// Copyright 2020, Ryan Wendland, usb64
// SPDX-License-Identifier: MIT

#ifndef _TUSB_XINPUT_HOST_H_
#define _TUSB_XINPUT_HOST_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUH_XINPUT_EPIN_BUFSIZE
#define CFG_TUH_XINPUT_EPIN_BUFSIZE 64
#endif

#ifndef CFG_TUH_XINPUT_EPOUT_BUFSIZE
#define CFG_TUH_XINPUT_EPOUT_BUFSIZE 64
#endif

//XINPUT defines and struct format from
//https://docs.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000
#define MAX_PACKET_SIZE 32

typedef struct xinput_gamepad
{
    uint16_t wButtons;
    uint8_t bLeftTrigger;
    uint8_t bRightTrigger;
    int16_t sThumbLX;
    int16_t sThumbLY;
    int16_t sThumbRX;
    int16_t sThumbRY;
} xinput_gamepad_t;

typedef enum
{
    XINPUT_UNKNOWN = 0,
    XBOXONE,
    XBOX360_WIRELESS,
    XBOX360_WIRED,
    XBOXOG
} xinput_type_t;

typedef struct
{
    xinput_type_t type;
    xinput_gamepad_t pad;
    uint8_t connected;
    uint8_t new_pad_data;
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;

    uint16_t epin_size;
    uint16_t epout_size;

    uint8_t epin_buf[CFG_TUH_XINPUT_EPIN_BUFSIZE];
    uint8_t epout_buf[CFG_TUH_XINPUT_EPOUT_BUFSIZE];
} xinputh_interface_t;

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
TU_ATTR_WEAK void tuh_xinput_report_sent_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
TU_ATTR_WEAK void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance);
TU_ATTR_WEAK void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf);
bool tuh_xinput_receive_report(uint8_t dev_addr, uint8_t instance);
bool tuh_xinput_send_report(uint8_t dev_addr, uint8_t instance, const uint8_t *txbuf, uint16_t len);
bool tuh_xinput_set_led(uint8_t dev_addr, uint8_t instance, uint8_t quadrant, bool block);
bool tuh_xinput_set_rumble(uint8_t dev_addr, uint8_t instance, uint8_t lValue, uint8_t rValue, bool block);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void xinputh_init       (void);
bool xinputh_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
bool xinputh_set_config (uint8_t dev_addr, uint8_t itf_num);
bool xinputh_xfer_cb    (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void xinputh_close      (uint8_t dev_addr);

//Wired 360 commands
static const uint8_t xbox360_wired_rumble[] = {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t xbox360_wired_led[] = {0x01, 0x03, 0x00};

//Xbone one
static const uint8_t xboxone_start_input[] = {0x05, 0x20, 0x03, 0x01, 0x00};
static const uint8_t xboxone_s_init[] = {0x05, 0x20, 0x00, 0x0f, 0x06};
static const uint8_t xboxone_pdp_init1[] = {0x0a, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14};
static const uint8_t xboxone_pdp_init2[] = {0x06, 0x30};
static const uint8_t xboxone_pdp_init3[] = {0x06, 0x20, 0x00, 0x02, 0x01, 0x00};
static const uint8_t xboxone_rumble[] = {0x09, 0x00, 0x00, 0x09, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEB};

//Wireless 360 commands
static const uint8_t xbox360w_led[] = {0x00, 0x00, 0x08, 0x40};
//Sending 0x00, 0x00, 0x08, 0x00 will permanently disable rumble until you do this:
static const uint8_t xbox360w_rumble_enable[] = {0x00, 0x00, 0x08, 0x01};
static const uint8_t xbox360w_rumble[] = {0x00, 0x01, 0x0F, 0xC0, 0x00, 0x00};
static const uint8_t xbox360w_inquire_present[] = {0x08, 0x00, 0x0F, 0xC0};
static const uint8_t xbox360w_controller_info[] = {0x00, 0x00, 0x00, 0x40};
static const uint8_t xbox360w_unknown[] = {0x00, 0x00, 0x02, 0x80};
static const uint8_t xbox360w_power_off[] = {0x00, 0x00, 0x08, 0xC0};
static const uint8_t xbox360w_chatpad_init[] = {0x00, 0x00, 0x0C, 0x1B};
static const uint8_t xbox360w_chatpad_keepalive1[] = {0x00, 0x00, 0x0C, 0x1F};
static const uint8_t xbox360w_chatpad_keepalive2[] = {0x00, 0x00, 0x0C, 0x1E};

//Original Xbox
static const uint8_t xboxog_rumble[] = {0x00, 0x06, 0x00, 0x00, 0x00, 0x00};

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_XINPUT_HOST_H_ */

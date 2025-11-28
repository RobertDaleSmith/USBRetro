// usbh.c - USB Host Layer
//
// Provides unified USB host handling across HID, X-input, and Bluetooth protocols.
// Device drivers read per-player feedback state from feedback_get_state().

#include "usbh.h"
#include "tusb.h"
#include "core/services/players/manager.h"
#include "core/services/codes/codes.h"
#include <stdio.h>

// HID protocol handlers
extern void hid_init(void);
extern void hid_task(void);

// X-input protocol handlers
extern void xinput_task(void);

// BTD (Bluetooth Dongle) protocol handlers
#if CFG_TUH_BTD
#include "btd/btd.h"
#endif

void usbh_init(void)
{
    hid_init();
    tusb_init();
}

void usbh_task(void)
{
    // TinyUSB host polling
    tuh_task();

#if CFG_TUH_XINPUT
    xinput_task();
#endif

#if CFG_TUH_HID
    hid_task();
#endif

#if CFG_TUH_BTD
    btd_task();
#endif
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr)
{
    printf("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr)
{
    printf("A device with address %d is unmounted\r\n", dev_addr);

    remove_players_by_address(dev_addr, -1);

    // Reset test mode when device disconnects
    codes_reset_test_mode();
}

//--------------------------------------------------------------------+
// Input Interface
//--------------------------------------------------------------------+

const InputInterface usbh_input_interface = {
    .name = "USB Host",
    .source = INPUT_SOURCE_USB_HOST,
    .init = usbh_init,
    .task = usbh_task,
    .is_connected = NULL,       // TODO: Track connected device count
    .get_device_count = NULL,   // TODO: Return connected device count
};

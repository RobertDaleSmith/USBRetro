// hci_transport_h2_tinyusb.h - BTstack HCI USB Transport for TinyUSB
//
// This implements the BTstack hci_transport_t interface using TinyUSB's
// USB host stack. Allows BTstack to communicate with USB Bluetooth dongles
// on RP2040 and other TinyUSB-supported platforms.

#ifndef HCI_TRANSPORT_H2_TINYUSB_H
#define HCI_TRANSPORT_H2_TINYUSB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration - set USE_BTSTACK=1 for full BTstack integration
#ifndef USE_BTSTACK
#define USE_BTSTACK 0
#endif

#if USE_BTSTACK
// Include BTstack header for hci_transport_t definition
#include "hci_transport.h"

// Get the TinyUSB HCI transport instance (only available with BTstack)
const hci_transport_t* hci_transport_h2_tinyusb_instance(void);
#endif

// USB Bluetooth dongle identification
#define USB_CLASS_WIRELESS_CTRL     0xE0
#define USB_SUBCLASS_RF             0x01
#define USB_PROTOCOL_BLUETOOTH      0x01

// Buffer sizes
#define HCI_USB_CMD_BUF_SIZE        264     // HCI command buffer
#define HCI_USB_EVT_BUF_SIZE        264     // HCI event buffer
#define HCI_USB_ACL_BUF_SIZE        1024    // ACL data buffer (larger for GATT)

// TinyUSB class driver interface - register with usbh_app_driver_get_cb()
#include "tusb.h"
#include "host/usbh_pvt.h"

extern const usbh_class_driver_t usbh_btstack_driver;

// Driver callbacks (called by TinyUSB)
bool btstack_driver_init(void);
bool btstack_driver_deinit(void);
bool btstack_driver_open(uint8_t rhport, uint8_t dev_addr,
                         tusb_desc_interface_t const* desc_itf, uint16_t max_len);
bool btstack_driver_set_config(uint8_t dev_addr, uint8_t itf_num);
bool btstack_driver_xfer_cb(uint8_t dev_addr, uint8_t ep_addr,
                            xfer_result_t result, uint32_t xferred_bytes);
void btstack_driver_close(uint8_t dev_addr);

// Must be called from main loop to process USB events
void hci_transport_h2_tinyusb_process(void);

// Check if a Bluetooth dongle is connected
bool hci_transport_h2_tinyusb_is_connected(void);

// Standalone test mode (USE_BTSTACK=0)
#if !USE_BTSTACK
void hci_transport_h2_tinyusb_test_init(void);
void hci_transport_h2_tinyusb_test_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // HCI_TRANSPORT_H2_TINYUSB_H

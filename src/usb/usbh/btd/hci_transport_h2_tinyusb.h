// hci_transport_h2_tinyusb.h - BTstack HCI USB Transport for TinyUSB
//
// This implements the BTstack hci_transport_t interface using TinyUSB's
// USB host stack. Allows BTstack to communicate with USB Bluetooth dongles
// on RP2040 and other TinyUSB-supported platforms.
//
// ============================================================================
// BLUETOOTH DONGLE COMPATIBILITY GUIDE
// ============================================================================
//
// Not all USB Bluetooth dongles work on embedded systems. The key factor is
// whether the dongle has firmware in ROM or requires host-side firmware loading.
//
// CHIPSET COMPATIBILITY:
// ----------------------
// ✅ BROADCOM (Manufacturer ID 0x000F)
//    - Firmware in ROM, works out of the box
//    - Common chips: BCM20702A0 (BT 4.0)
//    - Recommended for embedded use
//
// ✅ CSR/Cambridge Silicon Radio (Manufacturer ID 0x000A)
//    - Firmware in ROM, should work out of the box
//    - Common chips: CSR8510 A10 (BT 4.0)
//    - WARNING: Many cheap Amazon CSR8510 dongles are COUNTERFEIT CLONES
//      that may have pairing/discovery issues. Buy from reputable sources
//      like Adafruit. Linux kernel 5.9+ added workarounds for clones.
//
// ❌ REALTEK (Manufacturer ID 0x005D)
//    - Firmware must be loaded by host at every boot
//    - Common chips: RTL8761B, RTL8761BU, RTL8761BUV (BT 5.0)
//    - Dominates the BT 5.0+ market - almost all cheap BT 5.0 dongles are Realtek
//    - Works on Linux/Windows (kernel/driver loads firmware automatically)
//    - Does NOT work on embedded without implementing firmware loading
//    - BTstack has btstack_chipset_realtek module but requires ~50KB firmware blob
//
// TESTED DONGLES:
// ---------------
// ✅ WORKS:
//    - Amazon Basics (VID 0x33FA, PID 0x0010)
//      Class 0xE0 (standard), Chip 0x08E7 (unknown Chinese), BT 4.0
//
//    - Kinivo BTD-400 (VID 0x0A5C, PID 0x21E8)
//      Class 0xFF (vendor), Chip 0x000F (Broadcom BCM20702A0), BT 4.0
//
//    - Panda PBU40 (VID 0x0A5C, PID 0x21E8) - same as Kinivo
//      Broadcom BCM20702A0, BT 4.0, explicitly supports Linux/Raspberry Pi
//
//    - ASUS USB-BT400 (Broadcom BCM20702, BT 4.0) - should work (untested)
//
//    - Adafruit Bluetooth 4.0 USB Module #1327 (CSR8510 A10)
//      Genuine CSR from reputable source, should work (untested)
//
// ❌ DOES NOT WORK (needs firmware loading):
//    - TP-Link UB400/UB500 (VID 0x2357, PID 0x0604)
//      Chip 0x005D (Realtek RTL8761B), BT 5.0
//
//    - ASUS USB-BT500 (Realtek RTL8761B, BT 5.0)
//
//    - UGREEN BT 5.0 adapters (Realtek RTL8761BUV)
//
//    - Maxuni BT 5.3 adapters (Realtek)
//
//    - Avantree DG45 (Realtek RTL8761BW)
//
//    - Zexmte BT 5.0 (Realtek RTL8761B)
//
//    - Basically ALL Bluetooth 5.0+ dongles are Realtek and won't work
//
// BUYING RECOMMENDATIONS:
// -----------------------
// 1. Look for BT 4.0 dongles with Broadcom or genuine CSR chips
// 2. Kinivo BTD-400 and Panda PBU40 are safe choices (~$12)
// 3. Adafruit #1327 is a trustworthy CSR8510 source (~$13)
// 4. Avoid random "CSR8510" listings on Amazon - likely counterfeits
// 5. BT 5.0+ dongles are almost all Realtek - avoid for embedded use
//
// USB CLASS IDENTIFICATION:
// -------------------------
// Standard BT dongles: Class 0xE0 (Wireless Controller), SubClass 0x01, Protocol 0x01
// Broadcom dongles:    Class 0xFF (Vendor Specific), SubClass 0x01, Protocol 0x01
//
// ============================================================================

#ifndef HCI_TRANSPORT_H2_TINYUSB_H
#define HCI_TRANSPORT_H2_TINYUSB_H

#include <stdint.h>
#include <stdbool.h>
#include "hci_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

// Get the TinyUSB HCI transport instance
const hci_transport_t* hci_transport_h2_tinyusb_instance(void);

// USB Bluetooth dongle identification
#define USB_CLASS_WIRELESS_CTRL     0xE0
#define USB_CLASS_VENDOR_SPECIFIC   0xFF
#define USB_SUBCLASS_RF             0x01
#define USB_PROTOCOL_BLUETOOTH      0x01

// Vendor IDs for dongles that use vendor-specific class instead of standard BT class
#define USB_VID_BROADCOM            0x0A5C

// Bluetooth chip manufacturer IDs (from hci_get_manufacturer())
#define BT_MANUFACTURER_CSR         0x000A  // Cambridge Silicon Radio
#define BT_MANUFACTURER_BROADCOM    0x000F  // Broadcom
#define BT_MANUFACTURER_REALTEK     0x005D  // Realtek (needs firmware loading)
#define BT_MANUFACTURER_INTEL       0x0002  // Intel
#define BT_MANUFACTURER_QUALCOMM    0x001D  // Qualcomm
#define BT_MANUFACTURER_TI          0x000D  // Texas Instruments
#define BT_MANUFACTURER_MEDIATEK    0x0046  // MediaTek

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

#ifdef __cplusplus
}
#endif

#endif // HCI_TRANSPORT_H2_TINYUSB_H

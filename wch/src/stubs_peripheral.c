// stubs_peripheral.c - Weak stubs for the CH32V307 usb2usb build
//
// usb2usb on CH32 has no Bluetooth and no BT HID. Shared code (router.c,
// cdc_commands.c, sinput_mode.c) references symbols from those modules; these
// weak stubs satisfy the linker. Mirrors esp/main/stubs_peripheral.c. A real
// implementation (if BT is ever added to this target) overrides any of these.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// bthid_get_device (router.c, sinput_mode.c)
typedef struct { int dummy; } bthid_device_t;
__attribute__((weak)) bthid_device_t* bthid_get_device(uint8_t conn_index)
{
    (void)conn_index;
    return NULL;
}

// wiimote (cdc_commands.c)
__attribute__((weak)) uint8_t wiimote_get_orient_mode(void) { return 0; }
__attribute__((weak)) const char* wiimote_get_orient_mode_name(uint8_t mode) { (void)mode; return "N/A"; }
__attribute__((weak)) void wiimote_set_orient_mode(uint8_t mode) { (void)mode; }

// btstack_host (cdc_commands.c)
__attribute__((weak)) void btstack_host_delete_all_bonds(void) {}
__attribute__((weak)) bool btstack_host_is_initialized(void) { return false; }
__attribute__((weak)) void btstack_host_ble_drop_all(uint32_t holdoff_ms) { (void)holdoff_ms; }
__attribute__((weak)) bool btstack_host_is_scanning(void) { return false; }
__attribute__((weak)) uint8_t btstack_classic_get_connection_count(void) { return 0; }
__attribute__((weak)) bool btstack_host_get_last_connected(uint8_t bd_addr_out[6], char name_out[48])
{
    (void)bd_addr_out; (void)name_out;
    return false;
}
__attribute__((weak)) int btstack_host_list_classic_bonds(uint8_t addrs_out[][6], int max_count)
{
    (void)addrs_out; (void)max_count;
    return 0;
}
__attribute__((weak)) void btstack_host_forget_device(const uint8_t bd_addr[6]) { (void)bd_addr; }
__attribute__((weak)) void btstack_host_suppress_scan(bool suppress) { (void)suppress; }

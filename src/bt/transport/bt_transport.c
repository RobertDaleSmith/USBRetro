// bt_transport.c - Bluetooth Transport Manager
// Manages the active transport and provides weak callback defaults

#include "bt_transport.h"
#include <stdio.h>

// Whether the BTHID host registry is compiled into this build. Set by the build
// system from the source list itself (see the CONFIG_BT_HOST blocks in
// src/CMakeLists.txt, esp/main/CMakeLists.txt and nrf/CMakeLists.txt), so it
// cannot drift out of sync with whether bthid_registry.c is actually built.
//
// This used to be a weak stub defined right here, which was subtly broken: the
// weak definition and its call site sat in the same translation unit, so a
// linker running --gc-sections (ESP-IDF does) resolved the call locally and
// never pulled in bthid_registry.c's strong definition. The firmware linked
// clean and silently registered zero BT HID drivers.
#ifdef CONFIG_BT_HOST
#include "bt/bthid/bthid_registry.h"
#endif

// ============================================================================
// ACTIVE TRANSPORT
// ============================================================================

const bt_transport_t* bt_transport = NULL;

void bt_init(const bt_transport_t* transport)
{
    printf("[BT] bt_init called, transport=%p\n", (void*)transport);
    if (transport) {
        printf("[BT] transport name=%s task=%p\n", transport->name, (void*)transport->task);
    }
    fflush(stdout);
    bt_transport = transport;

    // Initialize BTHID registry (registers all drivers)
#ifdef CONFIG_BT_HOST
    bthid_registry_init();
#else
    // Peripheral-only build: no host-side drivers to register. Logged rather
    // than silent so an accidentally-missing registry is visible on the console.
    printf("[BT] BTHID registry not compiled in (peripheral-only build)\n");
#endif

    if (bt_transport && bt_transport->init) {
        printf("[BT] Initializing transport: %s\n", bt_transport->name);
        bt_transport->init();
    } else {
        printf("[BT] No transport init function!\n");
    }
}

// ============================================================================
// WEAK CALLBACK IMPLEMENTATIONS
// Override in BTHID layer
// ============================================================================

__attribute__((weak)) void bt_on_hid_ready(uint8_t conn_index)
{
    printf("[BT] HID ready on connection %d (weak handler)\n", conn_index);
}

__attribute__((weak)) void bt_on_disconnect(uint8_t conn_index)
{
    printf("[BT] Disconnected connection %d (weak handler)\n", conn_index);
}

__attribute__((weak)) void bt_on_hid_report(uint8_t conn_index, const uint8_t* data, uint16_t len)
{
    printf("[BT] HID report on connection %d: %d bytes (weak handler)\n", conn_index, len);
}

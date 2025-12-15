// bt_transport.c - Bluetooth Transport Manager
// Manages the active transport and provides weak callback defaults

#include "bt_transport.h"
#include "bt/bthid/bthid_registry.h"
#include <stdio.h>

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
    bthid_registry_init();

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

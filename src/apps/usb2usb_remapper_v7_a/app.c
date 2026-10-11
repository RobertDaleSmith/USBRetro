// app.c - usb2usb dual: DEVICE/CONSUMER side (A)
//
// The inter-MCU UART link feeds input events into the router under
// INPUT_SOURCE_UART_PEER; the router drives the USB device output (all output
// modes). Feedback (rumble/LED the host PC requested) is sent back to the host
// side (B) over the link so it can drive the controllers.

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "core/services/players/feedback.h"
#include "core/services/button/button.h"
#include "usb/usbd/usbd.h"
#include "usb/usbd/cdc/cdc_commands.h"
#include "usb/usbd/cdc/cdc_protocol.h"
#include "uart_peer/uart_peer.h"
#include "uart_peer/p5general_link.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>
#ifdef APP_CAN_FLASH_B
#include "flash_b_app.h"   // SWD-flash B from this app (reliable, no boot relay)
#endif

// ============================================================================
// INTERFACES — UART link in, USB device out
// ============================================================================

static const InputInterface* input_interfaces[] = {
    &uart_peer_input_interface,
};

const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    *count = sizeof(input_interfaces) / sizeof(input_interfaces[0]);
    return input_interfaces;
}

static const OutputInterface* output_interfaces[] = {
    &usbd_output_interface,
};

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    *count = sizeof(output_interfaces) / sizeof(output_interfaces[0]);
    return output_interfaces;
}

// ============================================================================
// BUTTON — BOOT button cycles the USB output mode (device side owns usbd)
// ============================================================================

static void on_button_event(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_CLICK:
            // Single click: report the current mode (no side effect).
            printf("[app:usb2usb_remapper_v7_a] current mode: %s\n",
                   usbd_get_mode_name(usbd_get_mode()));
            break;
        case BUTTON_EVENT_DOUBLE_CLICK: {
            // Double click: cycle to the next USB output mode (saved to flash).
            usb_output_mode_t next = usbd_get_next_mode();
            printf("[app:usb2usb_remapper_v7_a] Double-click - switching USB mode -> %s\n",
                   usbd_get_mode_name(next));
            usbd_set_mode(next);
            break;
        }
        case BUTTON_EVENT_TRIPLE_CLICK:
            // Triple click: reset to the default HID mode.
            printf("[app:usb2usb_remapper_v7_a] Triple-click - resetting to HID mode\n");
            usbd_reset_to_hid();
            break;
        default:
            break;
    }
}

// ============================================================================
// APP INIT / TASK
// ============================================================================

void app_init(void)
{
    printf("[app:usb2usb_remapper_v7_a] Initializing %s v%s\n", APP_NAME, JOYPAD_VERSION);

    feedback_init();
    button_init();
    button_set_callback(on_button_event);

    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_USB_DEVICE] = USB_OUTPUT_PORTS,
        },
        .merge_all_inputs = true,
        .transform_flags = TRANSFORM_FLAGS,
    };
    router_init(&router_cfg);

    // UART link -> USB device.
    router_add_route(INPUT_SOURCE_UART_PEER, OUTPUT_TARGET_USB_DEVICE, 0);

    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    // Bring up the inter-MCU link (consumer side).
    uart_peer_config_t cfg = {
        .uart_inst = LINK_UART_INST,
        .tx_pin = LINK_TX_PIN,
        .rx_pin = LINK_RX_PIN,
        .cts_pin = LINK_CTS_PIN,
        .rts_pin = LINK_RTS_PIN,
        .baud = LINK_BAUD,
        .flow_control = LINK_FLOW_CTRL,
    };
    uart_peer_init(&cfg);

    printf("[app:usb2usb_remapper_v7_a] Routing: UART peer link -> USB device\n");
}

void app_task(void)
{
    // Pump the link (RX events were submitted to the router via the input
    // interface task; this also drains TX). Then push feedback back to B.
    uart_peer_task();

    // BOOT button: double-click cycles the USB output mode (see on_button_event).
    button_task();

    // P5General (PS5) auth bridge: forward this device's report-to-sign / F0 /
    // F1-poll to the host side (B), which owns the auth dongle. No-op in other
    // output modes (the device mode only writes these fields in PS5 mode).
    p5general_link_device_task();

    // Latch the host PC's requested feedback (rumble + player LED + RGB lightbar).
    // get_feedback() is change-gated (returns true only when a value changed and
    // led_player/RGB are non-zero only on the changing frame), so hold the last
    // seen values here and stream them continuously to B. get_player_led on the
    // usbd output interface is NULL, and RGB has no scalar getter — get_feedback
    // is the only path that carries the lightbar colour, so this is also what
    // makes DualSense/P5General lightbar + player LED reach the pad on the link.
    static uint8_t fb_ml = 0, fb_mr = 0, fb_player = 0, fb_r = 0, fb_g = 0, fb_b = 0;
    if (usbd_output_interface.get_feedback) {
        output_feedback_t fb;
        if (usbd_output_interface.get_feedback(&fb)) {
            fb_ml = fb.rumble_left;
            fb_mr = fb.rumble_right;
            if (fb.led_player > 0) fb_player = fb.led_player;
            if (fb.led_r || fb.led_g || fb.led_b) { fb_r = fb.led_r; fb_g = fb.led_g; fb_b = fb.led_b; }
        }
    }

    // Send feedback to the host side at ~125 Hz so it can drive the controllers.
    static uint32_t last_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_ms >= 8) {
        last_ms = now;

        uart_peer_status_t st;
        memset(&st, 0, sizeof(st));
        st.flags = UART_PEER_STATUS_FLAG_CONNECTED;
        st.player_number = 1;
        st.rumble_left = fb_ml;
        st.rumble_right = fb_mr;
        st.led_player = fb_player;
        st.led_color[0] = fb_r;
        st.led_color[1] = fb_g;
        st.led_color[2] = fb_b;
        uart_peer_send_status(&st);
    }

    // Surface B's diagnostic heartbeat over CDC so bring-up tooling can see
    // whether B is alive, the link is up, and how many USB host devices B sees.
    static bool b_is_current = false;   // B has reported the new-firmware magic
    uart_peer_debug_t dbg;
    if (uart_peer_get_debug(&dbg)) {
        if (dbg.magic == 0xE4) b_is_current = true;  // 0xE4 = B carries the BLE diagnostics
        // crcd = EVENT frames this side threw away on a CRC mismatch. Input
        // travels B->A, so A's receive counter is the one that explains a stale
        // input state; non-zero means the link is dropping real frames and the
        // periodic refresh in uart_peer_task is what is covering for it.
        uint32_t crc_drops = 0, ring_drops = 0;
        uart_peer_get_link_stats(&crc_drops, &ring_drops);
        char buf[260];
        // rpts = BLE HID reports B has received. Zero while bt reports a
        // connection means the data path is dead, not the link -- the single
        // most useful number for telling those apart. peer = who B thinks it is
        // connected to; disc = the last disconnect reason B saw.
        char pname[13];
        for (size_t i = 0; i < sizeof(dbg.peer_name) && i < sizeof(pname) - 1; i++) {
            char ch = dbg.peer_name[i];
            pname[i] = (ch >= 0x20 && ch < 0x7f) ? ch : '\0';
            if (!pname[i]) break;
        }
        pname[sizeof(pname) - 1] = '\0';
        snprintf(buf, sizeof(buf),
                 "{\"type\":\"peer\",\"magic\":%u,\"devs\":%u,\"vid\":\"%04X\",\"pid\":\"%04X\",\"up\":%lu,\"bt\":%u,\"crcd\":%lu,\"ringd\":%lu,\"rpts\":%lu,\"bflags\":%u,\"disc\":\"%02X\",\"peer\":\"%s\"}",
                 dbg.magic, dbg.dev_count, dbg.last_vid, dbg.last_pid,
                 (unsigned long)dbg.uptime_ms, dbg.bt_status,
                 (unsigned long)crc_drops, (unsigned long)ring_drops,
                 (unsigned long)dbg.ble_reports, dbg.ble_flags,
                 dbg.last_disc_reason, pname);
        cdc_protocol_send_event(cdc_commands_get_protocol(), buf);
    }

#ifdef APP_CAN_FLASH_B
    // Auto-flash B if it isn't running current firmware. combine_uf2 "stage"
    // mode drops B's image into A's flash but doesn't program B (no boot relay),
    // so on the first boot after a stage flash B is stale. If B hasn't reported
    // the current heartbeat magic ~6s after boot and a valid image is staged,
    // flash it once from here — reliable (stable clock) and safe (writes B's
    // flash over SWD, never A's own). B then needs one power-cycle to run it;
    // after that it reports magic 0xDC and this never fires again.
    static bool b_autoflash_done = false;
    // 20s, not 6s: B's boot plus its first accepted heartbeat has to fit inside
    // this window, or A reflashes a B that was perfectly fine -- and an SWD
    // flash halts B until the next power cycle, so a too-eager window turns one
    // slow boot into a dead peer that needs a cable pull to recover.
    if (!b_autoflash_done && !b_is_current && now > 20000) {
        const volatile uint32_t* bimg = (const volatile uint32_t*)(0x10040000u);
        if (bimg[0] == 0x42494D47u) {   // "BIMG" staged image present
            b_autoflash_done = true;
            printf("[remapper_a] B not current -> auto-flashing from app...\n");
            int rc = flash_b_app(NULL);
            printf("[remapper_a] auto-flash B rc=%d (power-cycle for B to boot it)\n", rc);
        } else {
            b_autoflash_done = true;   // no staged image; nothing to do
        }
    }
#endif
}

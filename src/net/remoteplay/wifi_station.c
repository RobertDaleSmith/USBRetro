// wifi_station.c - CYW43 WiFi station-mode bring-up for usb2wifi
// SPDX-License-Identifier: Apache-2.0
#include "wifi_station.h"
#include "rp_config.h"

#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include <stdio.h>
#include <string.h>

static bool                 inited = false;
static wifi_station_state_t state = WIFI_STA_IDLE;
static uint32_t             last_check_ms = 0;
static uint32_t             retry_at_ms = 0;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

bool wifi_station_init(void)
{
    if (inited) return true;
    if (cyw43_arch_init() != 0) {
        printf("[wifi_sta] cyw43_arch_init failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();
    inited = true;
    state = WIFI_STA_IDLE;
    printf("[wifi_sta] station mode enabled\n");
    return true;
}

void wifi_station_connect(void)
{
    if (!inited) return;
    rp_config_t* cfg = rp_config_get();
    if (!cfg->have_wifi) {
        state = WIFI_STA_IDLE;
        return;
    }
    printf("[wifi_sta] connecting to '%s'...\n", cfg->wifi_ssid);
    // Async connect; link status is polled in the task.
    int r = cyw43_arch_wifi_connect_async(cfg->wifi_ssid, cfg->wifi_pass,
                                          CYW43_AUTH_WPA2_AES_PSK);
    state = (r == 0) ? WIFI_STA_CONNECTING : WIFI_STA_FAILED;
    if (r != 0) {
        printf("[wifi_sta] connect_async failed (%d)\n", r);
        retry_at_ms = now_ms() + 5000;
    }
}

void wifi_station_task(void)
{
    if (!inited) return;
#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
#endif
    uint32_t t = now_ms();
    if (t - last_check_ms < 250) return;
    last_check_ms = t;

    rp_config_t* cfg = rp_config_get();

    switch (state) {
        case WIFI_STA_IDLE:
            if (cfg->have_wifi) wifi_station_connect();
            break;
        case WIFI_STA_CONNECTING: {
            int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            if (link == CYW43_LINK_UP) {
                state = WIFI_STA_CONNECTED;
                char ip[16]; wifi_station_get_ip(ip, sizeof(ip));
                printf("[wifi_sta] connected, ip=%s\n", ip);
            } else if (link < 0) {
                printf("[wifi_sta] link failed (%d), retrying\n", link);
                state = WIFI_STA_FAILED;
                retry_at_ms = t + 5000;
            }
            break;
        }
        case WIFI_STA_CONNECTED: {
            int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            if (link != CYW43_LINK_UP) {
                printf("[wifi_sta] link dropped, reconnecting\n");
                state = WIFI_STA_FAILED;
                retry_at_ms = t + 2000;
            }
            break;
        }
        case WIFI_STA_FAILED:
            if (t >= retry_at_ms && cfg->have_wifi) wifi_station_connect();
            break;
    }
}

wifi_station_state_t wifi_station_get_state(void) { return state; }
bool wifi_station_is_connected(void) { return state == WIFI_STA_CONNECTED; }

void wifi_station_get_ip(char* buf, int buflen)
{
    if (buflen < 1) return;
    buf[0] = '\0';
    if (state != WIFI_STA_CONNECTED) return;
    struct netif* nif = &cyw43_state.netif[CYW43_ITF_STA];
    const ip4_addr_t* ip = netif_ip4_addr(nif);
    if (ip) {
        snprintf(buf, buflen, "%s", ip4addr_ntoa(ip));
    }
}

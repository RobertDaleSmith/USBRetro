// rp_discovery.c - PS5/PS4 LAN discovery via raw-LWIP UDP broadcast
// SPDX-License-Identifier: Apache-2.0
#include "rp_discovery.h"
#include "wifi_station.h"

#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include <stdio.h>
#include <string.h>

#define DISCOVERY_PORT_PS5 9302
#define DISCOVERY_PORT_PS4 987
#define SCAN_DURATION_MS   3000

static const char PROBE_PS5[] = "SRCH * HTTP/1.1\ndevice-discovery-protocol-version:00030010\n";
static const char PROBE_PS4[] = "SRCH * HTTP/1.1\ndevice-discovery-protocol-version:00020020\n";

static struct udp_pcb* pcb = NULL;
static rp_discovery_host_t hosts[RP_DISCOVERY_MAX_HOSTS];
static uint8_t host_count = 0;
static uint32_t scan_end_ms = 0;
static bool scanning = false;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// Pull a "key:value" line's value out of the HTTP-ish discovery response.
static void extract(const char* body, const char* key, char* out, int outlen)
{
    out[0] = '\0';
    const char* p = strstr(body, key);
    if (!p) return;
    p += strlen(key);
    if (*p == ':') p++;
    while (*p == ' ') p++;
    int i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < outlen - 1) out[i++] = *p++;
    out[i] = '\0';
}

static void recv_cb(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                    const ip_addr_t* addr, u16_t port)
{
    (void)arg; (void)upcb; (void)port;
    if (!p) return;
    char body[512];
    int n = p->tot_len < (int)sizeof(body) - 1 ? p->tot_len : (int)sizeof(body) - 1;
    pbuf_copy_partial(p, body, n, 0);
    body[n] = '\0';
    pbuf_free(p);

    char htype[16], hname[32];
    extract(body, "host-type", htype, sizeof(htype));
    extract(body, "host-name", hname, sizeof(hname));
    // Status: first line "HTTP/1.1 200 Ok" = ready, "620 Server Standby" = standby.
    bool ready = (strstr(body, "200") != NULL);

    const char* ipstr = ipaddr_ntoa(addr);
    // Dedup by IP.
    for (uint8_t i = 0; i < host_count; i++)
        if (strcmp(hosts[i].ip, ipstr) == 0) return;
    if (host_count >= RP_DISCOVERY_MAX_HOSTS) return;

    rp_discovery_host_t* h = &hosts[host_count++];
    snprintf(h->ip, sizeof(h->ip), "%s", ipstr);
    snprintf(h->name, sizeof(h->name), "%s", hname[0] ? hname : "PlayStation");
    h->is_ps5 = (strstr(htype, "PS5") != NULL);
    h->ready = ready;
    printf("[rp_disc] found %s (%s) %s state=%s\n", h->name, htype, h->ip,
           ready ? "ready" : "standby");
}

static void send_probe(const char* probe, uint16_t dport)
{
    uint16_t len = strlen(probe);
    struct pbuf* pb = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!pb) return;
    memcpy(pb->payload, probe, len);
    ip_addr_t bcast;
    IP_ADDR4(&bcast, 255, 255, 255, 255);
    udp_sendto(pcb, pb, &bcast, dport);
    pbuf_free(pb);
}

void rp_discovery_start(void)
{
    if (!wifi_station_is_connected()) return;
    if (!pcb) {
        pcb = udp_new();
        if (!pcb) return;
        udp_bind(pcb, IP_ANY_TYPE, 0);
        pcb->so_options |= SOF_BROADCAST;
        udp_recv(pcb, recv_cb, NULL);
    }
    host_count = 0;
    scanning = true;
    scan_end_ms = now_ms() + SCAN_DURATION_MS;
    cyw43_arch_lwip_begin();
    send_probe(PROBE_PS5, DISCOVERY_PORT_PS5);
    send_probe(PROBE_PS4, DISCOVERY_PORT_PS4);
    cyw43_arch_lwip_end();
    printf("[rp_disc] scan started\n");
}

void rp_discovery_task(void)
{
    if (scanning && now_ms() >= scan_end_ms) {
        scanning = false;
        printf("[rp_disc] scan done: %u host(s)\n", host_count);
    }
}

bool rp_discovery_in_progress(void) { return scanning; }

uint8_t rp_discovery_get_hosts(rp_discovery_host_t* out, uint8_t max)
{
    uint8_t n = host_count < max ? host_count : max;
    memcpy(out, hosts, n * sizeof(rp_discovery_host_t));
    return n;
}

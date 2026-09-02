// rp_session_stream.c - Remote Play streaming session engine (poll-mode, no RTOS)
// SPDX-License-Identifier: Apache-2.0
//
// Reimplements chiaki's session flow as a bare-metal raw-lwip state machine:
//   session-request (TCP :9295 -> RP-Nonce)  -> rpcrypt_init_auth
//   -> ctrl (TCP :9295 -> SESSION_ID)
//   -> takion INIT/COOKIE handshake (UDP :9296)
//   -> BIG (launch spec + ecdh pub) -> BANG -> ecdh secret -> gkcrypt init
//   -> controller-connection + feedback loop (inputs -> console)
//
// Crypto reused from chiaki (rpcrypt, gkcrypt) + rp_ecdh; serialization reused
// from chiaki (feedback, launchspec, base64). Byte layouts per
// .dev/docs/ps5-remoteplay-session-engine.md. Implements rp_session.h; selected
// over rp_session_stub.c when chiaki is vendored (CMake).

#include "rp_session.h"
#include "rp_config.h"
#include "wifi_station.h"
#include "rp_session_crypto.h"
#include "core/input_event.h"
#include "core/buttons.h"
#include "platform/platform.h"

#include <chiaki/rpcrypt.h>
#include <chiaki/gkcrypt.h>
#include <chiaki/feedback.h>
#include <chiaki/launchspec.h>
#include <chiaki/base64.h>
#include <chiaki/controller.h>
#include "rp_proto.h"

#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SESSION_PORT   9295
#define TAKION_PORT    9296
#define RP_TARGET      CHIAKI_TARGET_PS5_1
#define RP_VERSION_STR "1.0"
#define CLIENT_VERSION 12

// Takion constants
#define TK_TYPE_CONTROL   0
#define TK_TYPE_FBHISTORY 1
#define TK_TYPE_FBSTATE   6
#define TK_TYPE_CONGESTION 5
#define TK_HDR_SIZE       0x10
#define TK_A_RWND         0x19000
#define TK_STREAMS        0x64
#define TK_COOKIE_SIZE    0x20
#define CHUNK_DATA        0
#define CHUNK_INIT        1
#define CHUNK_INIT_ACK    2
#define CHUNK_DATA_ACK    3
#define CHUNK_COOKIE      0xa
#define CHUNK_COOKIE_ACK  0xb

typedef enum {
    S_IDLE = 0,
    S_SESSREQ_CONNECT, S_SESSREQ_WAIT,
    S_CTRL_CONNECT, S_CTRL_WAIT,
    S_TAKION_INIT, S_TAKION_COOKIE,
    S_BIG, S_BANG_WAIT,
    S_STREAM,
    S_DONE, S_ERROR,
} sstate_t;

static rp_session_state_t s_pub = RP_SESS_IDLE;
static sstate_t   s_state = S_IDLE;
static char       s_err[80];
// Streaming is OPT-IN: the session does NOT auto-connect. Enable via the web
// config / {stream:1}. Auto-connecting would put the PS5 into Remote Play mode
// (blanking its local TV) unasked, which is surprising and disruptive.
static bool       s_stream_enabled = false;

// identity / keys
static ChiakiRPCrypt s_rpcrypt;
static rp_ecdh_t     s_ecdh;
static ChiakiGKCrypt s_gk_local, s_gk_remote;
static bool          s_crypt_ready;
static uint8_t       s_handshake_key[RP_HANDSHAKE_KEY_SIZE];
static uint8_t       s_did[32];
static uint8_t       s_nonce[16];
static char          s_session_id[80];
static size_t        s_session_id_len;

// ctrl per-message counters
static uint32_t s_ctrl_ctr_local, s_ctrl_ctr_remote;

// takion
static uint32_t s_tag_local, s_tag_remote, s_seq_local;
static uint8_t  s_cookie[TK_COOKIE_SIZE];

// feedback
static ChiakiControllerState s_cstate;
static uint16_t s_fb_seq;
static uint64_t s_key_pos;
static uint32_t s_last_fb_ms, s_last_hb_ms, s_last_cong_ms;
static uint16_t s_recv_count;   // takion packets received since last congestion report

// net
static struct tcp_pcb* s_tcp;         // sessreq then ctrl (reused sequentially)
static struct udp_pcb* s_udp;         // takion
static ip_addr_t       s_ip;
static uint32_t        s_deadline;

// receive accumulation
static uint8_t  s_rx[2048];
static uint16_t s_rx_len;
// BIG/BANG reassembly
static uint8_t  s_reasm[2048];
static uint16_t s_reasm_len;

static uint32_t now_ms(void) { return platform_time_ms(); }

// forward decl: send a clean DISCONNECT over takion (defined after takion helpers)
static void stream_send_disconnect(void);

static void fail(const char* m)
{
    // If we were in a live stream, tell the console we're leaving so it releases
    // the Remote Play slot (otherwise it stays "in use" and refuses reconnects).
    if (s_state==S_STREAM && s_crypt_ready) stream_send_disconnect();
    snprintf(s_err, sizeof(s_err), "%s", m);
    s_state = S_ERROR; s_pub = RP_SESS_ERROR;
    printf("[rp_stream] error: %s\n", m);
}

static void rand_bytes(uint8_t* b, size_t n)
{ for (size_t i = 0; i < n; i += 4) { uint32_t r = get_rand_32(); size_t k = n-i<4?n-i:4; memcpy(b+i,&r,k); } }

// ============================ wakeup (rest-mode consoles) ===================
// Wake a rest-mode PS5 so it opens the session port. Credential = the PS5
// registration key as a big-endian integer (chiaki: "regist key as hex").
static int hexv(int ch){ if(ch>='0'&&ch<='9')return ch-'0'; if(ch>='a'&&ch<='f')return ch-'a'+10;
                         if(ch>='A'&&ch<='F')return ch-'A'+10; return -1; }
static void send_wakeup(void)
{
    rp_config_t* c = rp_config_get();
    // The stored regist_key bytes are ASCII hex chars (zero-padded). The wakeup
    // credential (pyremoteplay format_regist_key) = big-endian integer of
    // hex-decoding that ASCII string. e.g. bytes "12345678" -> 0x12345678.
    uint64_t cred = 0;
    for (int i = 0; i+1 < RP_REGIST_LEN && c->regist_key[i] && c->regist_key[i+1]; i += 2) {
        int hi = hexv(c->regist_key[i]), lo = hexv(c->regist_key[i+1]);
        if (hi < 0 || lo < 0) break;
        cred = (cred << 8) | ((hi << 4) | lo);
    }
    char pkt[256];
    int n = snprintf(pkt, sizeof(pkt),
        "WAKEUP * HTTP/1.1\n"
        "client-type:vr\nauth-type:R\nmodel:w\napp-type:r\n"
        "user-credential:%llu\n"
        "device-discovery-protocol-version:00030010\n",
        (unsigned long long)cred);
    struct udp_pcb* u = udp_new();
    if (!u) return;
    udp_bind(u, IP_ANY_TYPE, 0);
    struct pbuf* pb = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
    if (pb) {
        memcpy(pb->payload, pkt, n);
        cyw43_arch_lwip_begin(); udp_sendto(u, pb, &s_ip, 9302); cyw43_arch_lwip_end();
        pbuf_free(pb);
    }
    udp_remove(u);
    printf("[rp_stream] sent wakeup (cred=%llu)\n", (unsigned long long)cred);
}

// ============================ session request (RP-Nonce) ====================
static void tcp_close_safe(void)
{
    if (s_tcp) { tcp_arg(s_tcp,NULL); tcp_recv(s_tcp,NULL); tcp_err(s_tcp,NULL); tcp_sent(s_tcp,NULL);
                 tcp_close(s_tcp); s_tcp=NULL; }
}

static bool header_val(const char* buf, int len, const char* key, char* out, int outmax)
{
    int kl=(int)strlen(key);
    for (int i=0;i+kl<len;i++) if (!strncasecmp(buf+i,key,kl) && buf[i+kl]==':') {
        const char* p=buf+i+kl+1; const char* e=buf+len;
        while (p<e && (*p==' '||*p=='\t')) p++;
        int o=0; while (p<e && *p!='\r' && *p!='\n' && o<outmax-1) out[o++]=*p++;
        out[o]=0; return true;
    }
    return false;
}

static err_t sessreq_recv(void* a, struct tcp_pcb* pcb, struct pbuf* p, err_t e)
{
    (void)a;(void)e;
    if (!p) { fail("sessreq closed early"); tcp_close_safe(); return ERR_OK; }
    for (struct pbuf* q=p;q;q=q->next){ uint16_t sp=s_rx_len<sizeof(s_rx)?sizeof(s_rx)-s_rx_len:0;
        uint16_t t=q->len<sp?q->len:sp; if(t){memcpy(s_rx+s_rx_len,q->payload,t); s_rx_len+=t;} }
    tcp_recved(pcb,p->tot_len); pbuf_free(p);
    // wait for full header
    char* he=NULL; for (uint16_t i=0;i+3<s_rx_len;i++) if(!memcmp(s_rx+i,"\r\n\r\n",4)){he=(char*)s_rx+i+4;break;}
    if (!he) return ERR_OK;
    int hlen=(int)(he-(char*)s_rx);
    if (s_rx[9] != '2') {
        // capture the HTTP status code + RP-Application-Reason for diagnosis
        char code[8]={0}; for(int i=0;i<7 && 9+i<hlen && s_rx[9+i]!=' ' && s_rx[9+i]!='\r';i++) code[i]=s_rx[9+i];
        char reason[24]; if(!header_val((char*)s_rx,hlen,"RP-Application-Reason",reason,sizeof(reason))) reason[0]=0;
        tcp_close_safe();
        // 0x80108b10 = Remote Play already IN USE (a prior session slot is stuck).
        // Stop retrying so we don't keep the console busy — it clears on its own
        // (~2 min) or on a PS5 restart. Otherwise our 5s retries never let it free.
        if (strstr(reason,"80108b10")) {
            s_stream_enabled=false;
            fail("PS5 says Remote Play is in use — wait ~2 min or restart the PS5, then Start again");
        } else {
            char m[80]; snprintf(m,sizeof(m),"sessreq HTTP %s%s%s",code, reason[0]?" reason=":"", reason);
            fail(m);
        }
        return ERR_OK;
    }
    char nb[64];
    if (!header_val((char*)s_rx,hlen,"RP-Nonce",nb,sizeof(nb))) { fail("no RP-Nonce"); tcp_close_safe(); return ERR_OK; }
    size_t nlen=sizeof(s_nonce);
    if (chiaki_base64_decode(nb,strlen(nb),s_nonce,&nlen)!=CHIAKI_ERR_SUCCESS || nlen!=16) { fail("bad nonce"); tcp_close_safe(); return ERR_OK; }
    // derive auth crypt from nonce + morning(rp_key)
    rp_config_t* c=rp_config_get();
    chiaki_rpcrypt_init_auth(&s_rpcrypt, RP_TARGET, s_nonce, c->rp_key);
    tcp_close_safe();
    printf("[rp_stream] got nonce, auth crypt ready\n");
    s_state = S_CTRL_CONNECT;
    return ERR_OK;
}

static err_t sessreq_connected(void* a, struct tcp_pcb* pcb, err_t e)
{
    (void)a;
    if (e!=ERR_OK){ fail("sessreq connect failed"); return ERR_OK; }
    rp_config_t* c=rp_config_get();
    // regist_key hex, trimmed at first NUL
    char rkhex[RP_REGIST_LEN*2+1]; int rl=0;
    for (int i=0;i<RP_REGIST_LEN && c->regist_key[i];i++){ sprintf(rkhex+i*2,"%02x",c->regist_key[i]); rl=i+1; }
    rkhex[rl*2]=0;
    char req[512];
    int n=snprintf(req,sizeof(req),
        "GET /sie/ps5/rp/sess/init HTTP/1.1\r\nHost: %s:%d\r\n"
        "User-Agent: remoteplay Windows\r\nConnection: close\r\nContent-Length: 0\r\n"
        "RP-Registkey: %s\r\nRp-Version: %s\r\n\r\n",
        ipaddr_ntoa(&s_ip), SESSION_PORT, rkhex, RP_VERSION_STR);
    s_rx_len=0;
    if (tcp_write(pcb,req,n,TCP_WRITE_FLAG_COPY)!=ERR_OK || tcp_output(pcb)!=ERR_OK){ fail("sessreq send"); return ERR_OK; }
    s_state=S_SESSREQ_WAIT; s_deadline=now_ms()+5000;
    return ERR_OK;
}

static void tcp_err_cb(void* a, err_t e){ (void)a; s_tcp=NULL;
    if (s_state!=S_DONE && s_state!=S_ERROR){ char m[56];
        snprintf(m,sizeof(m),"tcp reset (err %d) in %s — is the PS5 awake?",(int)e,rp_session_state_str()); fail(m);} }

static void start_sessreq(void)
{
    s_tcp=tcp_new(); if(!s_tcp){fail("tcp alloc");return;}
    tcp_arg(s_tcp,NULL); tcp_err(s_tcp,tcp_err_cb); tcp_recv(s_tcp,sessreq_recv);
    s_state=S_SESSREQ_CONNECT; s_deadline=now_ms()+5000;
    if (tcp_connect(s_tcp,&s_ip,SESSION_PORT,sessreq_connected)!=ERR_OK) fail("sessreq connect start");
}

// ============================ ctrl (SESSION_ID) =============================
// per-header encrypted value -> base64
static bool enc_hdr_b64(uint64_t counter, const uint8_t* pt, size_t len, char* out, size_t outmax)
{
    uint8_t enc[64]; if (len>sizeof(enc)) return false;
    if (chiaki_rpcrypt_encrypt(&s_rpcrypt, counter, pt, enc, len)!=CHIAKI_ERR_SUCCESS) return false;
    return chiaki_base64_encode(enc, len, out, outmax)==CHIAKI_ERR_SUCCESS;
}

static err_t ctrl_recv(void* a, struct tcp_pcb* pcb, struct pbuf* p, err_t e);

static err_t ctrl_connected(void* a, struct tcp_pcb* pcb, err_t e)
{
    (void)a;
    if (e!=ERR_OK){ fail("ctrl connect failed"); return ERR_OK; }
    rp_config_t* c=rp_config_get();
    char auth[64], did[96], os[32], sbr[16], st[16];
    static const uint8_t ostype[]="Win10.0.0"; // + NUL
    uint8_t sbr_pt[4]={0,0,0,0};
    uint8_t st_pt[4]={1,0,0,0}; // streaming type 1 (H264) LE
    if (!enc_hdr_b64(0, c->regist_key, RP_REGIST_LEN, auth, sizeof(auth)) ||
        !enc_hdr_b64(1, s_did, sizeof(s_did), did, sizeof(did)) ||
        !enc_hdr_b64(2, ostype, sizeof(ostype), os, sizeof(os)) ||
        !enc_hdr_b64(3, sbr_pt, 4, sbr, sizeof(sbr)) ||
        !enc_hdr_b64(4, st_pt, 4, st, sizeof(st))) { fail("ctrl hdr enc"); return ERR_OK; }
    s_ctrl_ctr_local=5; s_ctrl_ctr_remote=0;
    char req[768];
    int n=snprintf(req,sizeof(req),
        "GET /sie/ps5/rp/sess/ctrl HTTP/1.1\r\nHost: %s:%d\r\n"
        "User-Agent: remoteplay Windows\r\nConnection: keep-alive\r\nContent-Length: 0\r\n"
        "RP-Auth: %s\r\nRP-Version: %s\r\nRP-Did: %s\r\nRP-ControllerType: 3\r\n"
        "RP-ClientType: 11\r\nRP-OSType: %s\r\nRP-ConPath: 1\r\n"
        "RP-StartBitrate: %s\r\nRP-StreamingType: %s\r\n\r\n",
        ipaddr_ntoa(&s_ip), SESSION_PORT, auth, RP_VERSION_STR, did, os, sbr, st);
    s_rx_len=0;
    if (tcp_write(pcb,req,n,TCP_WRITE_FLAG_COPY)!=ERR_OK || tcp_output(pcb)!=ERR_OK){ fail("ctrl send"); return ERR_OK; }
    s_state=S_CTRL_WAIT; s_deadline=now_ms()+10000;
    return ERR_OK;
}

static void start_ctrl(void)
{
    s_tcp=tcp_new(); if(!s_tcp){fail("tcp alloc2");return;}
    tcp_arg(s_tcp,NULL); tcp_err(s_tcp,tcp_err_cb); tcp_recv(s_tcp,ctrl_recv);
    s_state=S_CTRL_CONNECT; s_deadline=now_ms()+10000;
    if (tcp_connect(s_tcp,&s_ip,SESSION_PORT,ctrl_connected)!=ERR_OK) fail("ctrl connect start");
}

// A ctrl message: [size:4 BE][type:2 BE][pad:2][payload(enc)]. Handle SESSION_ID(0x33).
static void ctrl_process_messages(void)
{
    // find header end first time (skip HTTP response)
    static bool http_done=false;
    if (!http_done) {
        char* he=NULL; for (uint16_t i=0;i+3<s_rx_len;i++) if(!memcmp(s_rx+i,"\r\n\r\n",4)){he=(char*)s_rx+i+4;break;}
        if (!he) return;
        if (s_rx[9]!='2'){ fail("ctrl rejected"); tcp_close_safe(); return; }
        int hlen=(int)(he-(char*)s_rx);
        // RP-Server-Type consumes remote counter 0 (decrypt not required for us)
        char stbuf[64];
        if (header_val((char*)s_rx,hlen,"RP-Server-Type",stbuf,sizeof(stbuf))) s_ctrl_ctr_remote=1;
        // drop the http header from the buffer
        uint16_t rest=s_rx_len-hlen; memmove(s_rx,s_rx+hlen,rest); s_rx_len=rest;
        http_done=true;
        printf("[rp_stream] ctrl connected, awaiting session id\n");
    }
    // parse framed ctrl messages
    while (s_rx_len>=8) {
        uint32_t sz=(s_rx[0]<<24)|(s_rx[1]<<16)|(s_rx[2]<<8)|s_rx[3];
        uint16_t type=(s_rx[4]<<8)|s_rx[5];
        if (8+sz>s_rx_len) break; // wait for full message
        uint8_t* pl=s_rx+8;
        // decrypt payload with remote counter
        if (sz) chiaki_rpcrypt_decrypt(&s_rpcrypt, s_ctrl_ctr_remote, pl, pl, sz);
        s_ctrl_ctr_remote++;
        if (type==0x33) { // SESSION_ID
            // first byte 0x4a length marker, then alnum id
            size_t off=(sz>0 && pl[0]==0x4a)?1:0;
            size_t idl=sz-off; if(idl>=sizeof(s_session_id)) idl=sizeof(s_session_id)-1;
            memcpy(s_session_id, pl+off, idl); s_session_id[idl]=0; s_session_id_len=idl;
            printf("[rp_stream] session id received (%u)\n",(unsigned)idl);
            s_state=S_TAKION_INIT;
        } else if (type==0xfe) { // HEARTBEAT_REQ -> reply HEARTBEAT_REP(0x1fe)
            uint8_t hb[8]={0,0,0,0, (0x1fe>>8)&0xff, 0x1fe&0xff, 0,0};
            if (s_tcp) { tcp_write(s_tcp,hb,8,TCP_WRITE_FLAG_COPY); tcp_output(s_tcp); }
        }
        uint16_t consumed=8+sz; memmove(s_rx,s_rx+consumed,s_rx_len-consumed); s_rx_len-=consumed;
    }
}

static err_t ctrl_recv(void* a, struct tcp_pcb* pcb, struct pbuf* p, err_t e)
{
    (void)a;(void)e;
    if (!p) { if (s_state<S_STREAM) fail("ctrl closed"); return ERR_OK; }
    for (struct pbuf* q=p;q;q=q->next){ uint16_t sp=s_rx_len<sizeof(s_rx)?sizeof(s_rx)-s_rx_len:0;
        uint16_t t=q->len<sp?q->len:sp; if(t){memcpy(s_rx+s_rx_len,q->payload,t); s_rx_len+=t;} }
    tcp_recved(pcb,p->tot_len); pbuf_free(p);
    ctrl_process_messages();
    return ERR_OK;
}

// ============================ takion (UDP) ==================================
static void tk_wr32(uint8_t* b, uint32_t v){ b[0]=v>>24; b[1]=v>>16; b[2]=v>>8; b[3]=v; }
static void tk_wr16(uint8_t* b, uint16_t v){ b[0]=v>>8; b[1]=v; }
static uint32_t tk_rd32(const uint8_t* b){ return (b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3]; }

// write the 0x10 message header at buf: tag, gmac(0), key_pos, chunk_type, flags, len+4
static void tk_hdr(uint8_t* buf, uint32_t tag, uint32_t key_pos, uint8_t ctype, uint8_t flags, uint16_t payload_size)
{
    tk_wr32(buf+0, tag);
    memset(buf+4,0,4);           // gmac slot
    tk_wr32(buf+8, key_pos);
    buf[0xc]=ctype; buf[0xd]=flags; tk_wr16(buf+0xe, payload_size+4);
}

static void tk_send(uint8_t* pkt, uint16_t len)
{
    struct pbuf* pb=pbuf_alloc(PBUF_TRANSPORT,len,PBUF_RAM); if(!pb) return;
    memcpy(pb->payload,pkt,len);
    cyw43_arch_lwip_begin(); udp_sendto(s_udp,pb,&s_ip,TAKION_PORT); cyw43_arch_lwip_end();
    pbuf_free(pb);
}

static void takion_send_init(void)
{
    uint8_t pkt[1+TK_HDR_SIZE+0x10]; pkt[0]=TK_TYPE_CONTROL;
    tk_hdr(pkt+1, s_tag_remote /*0 now*/, 0, CHUNK_INIT, 0, 0x10);
    uint8_t* pl=pkt+1+TK_HDR_SIZE;
    tk_wr32(pl+0, s_tag_local);
    tk_wr32(pl+4, TK_A_RWND);
    tk_wr16(pl+8, TK_STREAMS);
    tk_wr16(pl+0xa, TK_STREAMS);
    tk_wr32(pl+0xc, s_seq_local);
    tk_send(pkt,sizeof(pkt));
}

static void takion_send_cookie(void)
{
    uint8_t pkt[1+TK_HDR_SIZE+TK_COOKIE_SIZE]; pkt[0]=TK_TYPE_CONTROL;
    tk_hdr(pkt+1, s_tag_remote, 0, CHUNK_COOKIE, 0, TK_COOKIE_SIZE);
    memcpy(pkt+1+TK_HDR_SIZE, s_cookie, TK_COOKIE_SIZE);
    tk_send(pkt,sizeof(pkt));
}

static void takion_send_data_ack(uint32_t seq)
{
    uint8_t pkt[1+TK_HDR_SIZE+0xc]; pkt[0]=TK_TYPE_CONTROL;
    tk_hdr(pkt+1, s_tag_remote, 0, CHUNK_DATA_ACK, 0, 0xc);
    uint8_t* b=pkt+1+TK_HDR_SIZE; memset(b,0,0xc);
    tk_wr32(b+0, seq); tk_wr32(b+4, TK_A_RWND);
    tk_send(pkt,sizeof(pkt));
}

// Send a DATA message (channel 1). If crypt ready, compute GMAC. first=9-byte subhdr.
static void takion_send_data(const uint8_t* payload, uint16_t plen, uint8_t flags, bool cont)
{
    uint16_t sub = cont?8:9;
    uint16_t payload_size = sub+plen;
    uint16_t total = 1+TK_HDR_SIZE+payload_size;
    static uint8_t pkt[1600];
    if (total>sizeof(pkt)) return;
    pkt[0]=TK_TYPE_CONTROL;
    uint64_t key_pos = s_crypt_ready ? s_key_pos : 0;
    tk_hdr(pkt+1, s_tag_remote, (uint32_t)key_pos, CHUNK_DATA, flags, payload_size);
    uint8_t* b=pkt+1+TK_HDR_SIZE;
    tk_wr32(b+0, s_seq_local++); tk_wr16(b+4, 1 /*channel*/); tk_wr16(b+6,0);
    if (!cont) b[8]=0;
    memcpy(b+sub, payload, plen);
    if (s_crypt_ready) {
        // zero key_pos (offset 9 in datagram = header offset 8) already written; GMAC over whole
        uint32_t kp=(uint32_t)key_pos; memset(pkt+9,0,4);   // zero key_pos for MAC
        chiaki_gkcrypt_gmac(&s_gk_local, key_pos, pkt, total, pkt+5);
        tk_wr32(pkt+9, kp);          // restore key_pos
        s_key_pos += total;
    }
    tk_send(pkt,total);
}

// ============================ BIG / BANG ====================================
static void send_big(void)
{
    rp_config_t* c=rp_config_get(); (void)c;
    // launchspec JSON -> rpcrypt keystream XOR -> base64
    ChiakiLaunchSpec ls; memset(&ls,0,sizeof(ls));
    ls.target=RP_TARGET; ls.mtu=1454; ls.rtt=1; ls.handshake_key=s_handshake_key;
    ls.width=1280; ls.height=720; ls.max_fps=60; ls.codec=CHIAKI_CODEC_H264; ls.bw_kbps_sent=10000;
    char json[1024];
    int jn=chiaki_launchspec_format(json,sizeof(json),&ls);
    if (jn<=0){ fail("launchspec"); return; }
    size_t jsz=(size_t)jn+1; // include NUL
    static uint8_t enc[1024]; memset(enc,0,jsz);
    chiaki_rpcrypt_encrypt(&s_rpcrypt,0,enc,enc,jsz);
    for (size_t i=0;i<jsz;i++) enc[i]^=(uint8_t)json[i];
    char b64[1400];
    if (chiaki_base64_encode(enc,jsz,b64,sizeof(b64))!=CHIAKI_ERR_SUCCESS){ fail("ls b64"); return; }
    // ecdh pub + sig
    uint8_t pub[RP_ECDH_PUBKEY_MAX]; size_t publen=sizeof(pub);
    uint8_t sig[32]; size_t siglen=sizeof(sig);
    if (!rp_ecdh_get_local_pub_key(&s_ecdh,pub,&publen,s_handshake_key,sig,&siglen)){ fail("ecdh pub"); return; }
    // encode BIG
    static uint8_t big[2048];
    size_t bl=rp_proto_encode_big(big,sizeof(big),CLIENT_VERSION,s_session_id,b64,pub,publen,sig,siglen);
    if (!bl){ fail("big encode"); return; }
    // fragment over takion DATA (mtu ~1454-50)
    int mtu=1454-50; size_t total=bl, pos=0; bool first=true;
    while ((int)(total+26)>mtu || (!first && (int)(total+25)>mtu)) {
        int bs = first?(mtu-26):(mtu-25);
        takion_send_data(big+pos,(uint16_t)bs,0,!first); pos+=bs; total-=bs; first=false;
    }
    takion_send_data(big+pos,(uint16_t)total,1,!first);
    printf("[rp_stream] BIG sent (%u bytes)\n",(unsigned)bl);
    s_reasm_len=0;
    s_state=S_BANG_WAIT; s_deadline=now_ms()+8000;
}

static void handle_bang(const uint8_t* proto, size_t len)
{
    rp_bang_t bang;
    int type=rp_proto_parse_message(proto,len,&bang);
    if (type==RP_TKMSG_BANG && bang.found) {
        if (!bang.version_accepted || !bang.encrypted_key_accepted){ fail("BANG rejected (ver/key)"); return; }
        uint8_t secret[RP_ECDH_SECRET_SIZE];
        if (!rp_ecdh_derive_secret(&s_ecdh,secret,bang.ecdh_pub,bang.ecdh_pub_len)){ fail("ecdh derive"); return; }
        if (chiaki_gkcrypt_init(&s_gk_local,NULL,0,2,s_handshake_key,secret)!=CHIAKI_ERR_SUCCESS ||
            chiaki_gkcrypt_init(&s_gk_remote,NULL,0,3,s_handshake_key,secret)!=CHIAKI_ERR_SUCCESS){ fail("gkcrypt init"); return; }
        s_crypt_ready=true; s_key_pos=0;
        printf("[rp_stream] BANG accepted, crypt established\n");
        // send controller-connection
        static uint8_t cc[32]; size_t cl=rp_proto_encode_controller_connection(cc,sizeof(cc),true);
        takion_send_data(cc,(uint16_t)cl,1,false);
        memset(&s_cstate, 0, sizeof(s_cstate));
        s_recv_count=0;
        s_state=S_STREAM; s_pub=RP_SESS_READY; s_last_fb_ms=s_last_hb_ms=s_last_cong_ms=now_ms();
        printf("[rp_stream] STREAMING\n");
    } else if (type==RP_TKMSG_DISCONNECT) {
        fail("console disconnected");
    }
    // STREAMINFO / others ignored (we ack via data-ack already)
}

// takion UDP receive
static void takion_recv(void* a, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port)
{
    (void)a;(void)pcb;(void)addr;(void)port;
    if (!p) return;
    static uint8_t buf[1600];
    int n=p->tot_len<(int)sizeof(buf)?p->tot_len:(int)sizeof(buf);
    pbuf_copy_partial(p,buf,n,0); pbuf_free(p);
    if (n<1) return;
    if (s_recv_count<0xffff) s_recv_count++;   // for congestion feedback
    uint8_t base=buf[0]&0xf;
    if (base!=TK_TYPE_CONTROL) return; // ignore AV/feedback-history from console for now
    if (n < 1+TK_HDR_SIZE) return;
    uint8_t* hdr=buf+1;
    uint8_t ctype=hdr[0xc];
    uint16_t plsz=((hdr[0xe]<<8)|hdr[0xf]); if(plsz>=4) plsz-=4;
    uint8_t* pl=buf+1+TK_HDR_SIZE;
    if (ctype==CHUNK_INIT_ACK && s_state==S_TAKION_INIT) {
        // pl: tag(4), a_rwnd(4), out(2), in(2), seq(4), cookie(0x20)
        s_tag_remote=tk_rd32(pl+0);
        if (n>=1+TK_HDR_SIZE+0x10+TK_COOKIE_SIZE) memcpy(s_cookie,pl+0x10,TK_COOKIE_SIZE);
        printf("[rp_stream] INIT_ACK, tag_remote set\n");
        takion_send_cookie();
        s_state=S_TAKION_COOKIE; s_deadline=now_ms()+5000;
    } else if (ctype==CHUNK_COOKIE_ACK && s_state==S_TAKION_COOKIE) {
        printf("[rp_stream] COOKIE_ACK, takion up\n");
        s_state=S_BIG;
    } else if (ctype==CHUNK_DATA) {
        // 9-byte subheader: seq(4), channel(2), 0(2), 0(1)
        uint32_t seq=tk_rd32(pl+0);
        takion_send_data_ack(seq);
        uint8_t* data=pl+9; int dlen=plsz-9; if(dlen<0) dlen=0;
        uint8_t flags=hdr[0xd];
        // during BANG_WAIT the payload is the BANG protobuf (unencrypted)
        if (s_state==S_BANG_WAIT) {
            if (s_reasm_len+dlen<=(int)sizeof(s_reasm)){ memcpy(s_reasm+s_reasm_len,data,dlen); s_reasm_len+=dlen; }
            if (flags&1) { handle_bang(s_reasm,s_reasm_len); s_reasm_len=0; }
        }
        // streaming: incoming DATA is gkcrypt-encrypted. Decrypt, reassemble, and
        // if it's STREAMINFO reply STREAMINFOACK so the console fully starts the
        // stream (otherwise it can sit on a black screen — chiaki does this).
        else if (s_state==S_STREAM && s_crypt_ready) {
            uint32_t kp=tk_rd32(hdr+8);
            if (dlen>0) chiaki_gkcrypt_decrypt(&s_gk_remote, kp, data, dlen);
            if (s_reasm_len+dlen<=(int)sizeof(s_reasm)){ memcpy(s_reasm+s_reasm_len,data,dlen); s_reasm_len+=dlen; }
            if (flags&1) {
                int mt=rp_proto_parse_message(s_reasm,s_reasm_len,NULL);
                if (mt==RP_TKMSG_STREAMINFO) {
                    uint8_t ack[16]; size_t al=rp_proto_encode_type_only(ack,sizeof(ack),RP_TKMSG_STREAMINFOACK);
                    takion_send_data(ack,(uint16_t)al,1,false);
                    printf("[rp_stream] STREAMINFOACK sent\n");
                } else if (mt==RP_TKMSG_DISCONNECT) {
                    fail("console disconnected");
                }
                s_reasm_len=0;
            }
        }
    }
}

static void start_takion(void)
{
    if (!s_udp) {
        s_udp=udp_new(); if(!s_udp){fail("udp alloc");return;}
        udp_bind(s_udp,IP_ANY_TYPE,0); udp_recv(s_udp,takion_recv,NULL);
    }
    s_tag_local=get_rand_32(); s_seq_local=s_tag_local; s_tag_remote=0;
    takion_send_init();
    s_state=S_TAKION_INIT; s_deadline=now_ms()+5000;
    printf("[rp_stream] takion INIT sent\n");
}

// Send a DISCONNECT (with reason payload) so the console releases the Remote Play
// slot. UDP, so send it a few times to beat packet loss.
static void stream_send_disconnect(void)
{
    uint8_t dis[48]; size_t dl=rp_proto_encode_disconnect(dis,sizeof(dis),"Client Disconnecting");
    for (int i=0;i<3;i++) takion_send_data(dis,(uint16_t)dl,1,false);
    printf("[rp_stream] sent DISCONNECT\n");
}

// ============================ keep-alive ====================================
// Stream heartbeat: type-only HEARTBEAT protobuf as a reliable DATA message.
static void send_heartbeat(void)
{
    uint8_t hb[8]; size_t hl=rp_proto_encode_type_only(hb,sizeof(hb),RP_TKMSG_HEARTBEAT);
    takion_send_data(hb,(uint16_t)hl,1,false);
}

// Congestion feedback (base type 5): GMAC-authenticated, not encrypted. Reports
// how many packets we received; keeps the console streaming (it drops us without).
static void send_congestion(void)
{
    uint8_t pkt[0xf]; memset(pkt,0,sizeof(pkt));
    pkt[0]=TK_TYPE_CONGESTION;
    tk_wr16(pkt+1,0);              // word_0
    tk_wr16(pkt+3,s_recv_count);   // received
    tk_wr16(pkt+5,0);              // lost
    if (s_crypt_ready) {
        uint64_t kp=s_key_pos;
        tk_wr32(pkt+0xb,(uint32_t)kp);
        uint32_t saved=tk_rd32(pkt+0xb); memset(pkt+0xb,0,4);  // zero key_pos for GMAC
        chiaki_gkcrypt_gmac(&s_gk_local,kp,pkt,sizeof(pkt),pkt+7);
        tk_wr32(pkt+0xb,saved);
        s_key_pos+=sizeof(pkt);
    }
    tk_send(pkt,sizeof(pkt));
    s_recv_count=0;
}

// ============================ feedback loop =================================
static void send_feedback_state(void)
{
    ChiakiFeedbackState fs; memset(&fs,0,sizeof(fs));
    fs.left_x=s_cstate.left_x; fs.left_y=s_cstate.left_y;
    fs.right_x=s_cstate.right_x; fs.right_y=s_cstate.right_y;
    fs.gyro_x=s_cstate.gyro_x; fs.gyro_y=s_cstate.gyro_y; fs.gyro_z=s_cstate.gyro_z;
    fs.accel_x=s_cstate.accel_x; fs.accel_y=s_cstate.accel_y; fs.accel_z=s_cstate.accel_z;
    fs.orient_x=s_cstate.orient_x; fs.orient_y=s_cstate.orient_y;
    fs.orient_z=s_cstate.orient_z; fs.orient_w=s_cstate.orient_w;
    uint8_t pkt[0xc+CHIAKI_FEEDBACK_STATE_BUF_SIZE_V12];
    memset(pkt,0,sizeof(pkt));
    pkt[0]=TK_TYPE_FBSTATE; tk_wr16(pkt+1,s_fb_seq++); pkt[3]=0;
    chiaki_feedback_state_format_v12(pkt+0xc,&fs);
    uint16_t payload_size=CHIAKI_FEEDBACK_STATE_BUF_SIZE_V12;
    uint64_t kp=s_key_pos;
    chiaki_gkcrypt_encrypt(&s_gk_local, kp+0x10, pkt+0xc, payload_size);
    tk_wr32(pkt+4,(uint32_t)kp);
    chiaki_gkcrypt_gmac(&s_gk_local, kp, pkt, sizeof(pkt), pkt+8);
    s_key_pos += sizeof(pkt);
    tk_send(pkt,sizeof(pkt));
}

// ============================ public API ====================================
void rp_session_init(void){ s_state=S_IDLE; s_pub=RP_SESS_IDLE; s_err[0]=0; }

void rp_session_start(void)
{
    rp_config_t* c=rp_config_get();
    if (!c->have_registration){ s_pub=RP_SESS_IDLE; return; }
    if (s_state!=S_IDLE && s_state!=S_ERROR && s_state!=S_DONE) return;
    if (!ipaddr_aton(c->ps5_ip,&s_ip)){ fail("bad ps5 ip"); return; }
    // fresh session identity
    rand_bytes(s_handshake_key,sizeof(s_handshake_key));
    static const uint8_t did_pre[10]={0x00,0x18,0x00,0x00,0x00,0x07,0x00,0x40,0x00,0x80};
    memcpy(s_did,did_pre,10); rand_bytes(s_did+10,16); memset(s_did+26,0,6);
    if (s_ecdh.ready) rp_ecdh_fini(&s_ecdh);
    if (!rp_ecdh_init(&s_ecdh)){ fail("ecdh init"); return; }
    s_crypt_ready=false; s_fb_seq=0; s_key_pos=0; s_session_id_len=0;
    printf("[rp_stream] starting session to %s\n",c->ps5_ip);
    send_wakeup();     // in case the console is in rest mode; harmless if already awake
    start_sessreq();
}

void rp_session_stop(void)
{
    // If a session is up, send a Takion DISCONNECT so the console cleanly exits
    // Remote Play and restores its local TV output.
    if (s_state==S_STREAM && s_udp && s_crypt_ready) stream_send_disconnect();
    tcp_close_safe();
    if (s_udp){ udp_recv(s_udp,NULL,NULL); udp_remove(s_udp); s_udp=NULL; }
    if (s_ecdh.ready) rp_ecdh_fini(&s_ecdh);
    s_crypt_ready=false;
    s_state=S_IDLE; s_pub=RP_SESS_IDLE;
}

// Enable/disable streaming. Disabling tears down any active session (with a clean
// DISCONNECT) so the console's TV comes back.
void rp_session_set_enabled(bool en)
{
    s_stream_enabled = en;
    if (!en) rp_session_stop();
    printf("[rp_stream] streaming %s\n", en?"ENABLED":"disabled");
}
bool rp_session_is_enabled(void){ return s_stream_enabled; }

void rp_session_task(void)
{
    uint32_t t=now_ms();
    switch (s_state) {
        case S_IDLE: {
            rp_config_t* c=rp_config_get();
            static uint32_t retry=0;
            if (s_stream_enabled && c->have_registration && wifi_station_is_connected() && t>retry) {
                retry=t+3000; rp_session_start();
            }
            break;
        }
        // one-shot action states (guarded so they fire exactly once): a callback
        // sets the state, the task kicks off the connect/send, which advances it.
        case S_CTRL_CONNECT: if (!s_tcp) start_ctrl(); break;
        case S_TAKION_INIT:  if (!s_udp) start_takion(); break;
        case S_BIG:          send_big(); break;   // send_big advances to S_BANG_WAIT
        case S_STREAM:
            if (t-s_last_fb_ms>=16)    { s_last_fb_ms=t;   send_feedback_state(); }
            if (t-s_last_cong_ms>=200) { s_last_cong_ms=t; send_congestion(); }
            if (t-s_last_hb_ms>=1000)  { s_last_hb_ms=t;   send_heartbeat(); }
            break;
        case S_ERROR: {
            // keep retrying the whole session (e.g. while the console wakes up),
            // but only while streaming is enabled.
            static uint32_t eretry=0;
            if (s_stream_enabled && t>eretry) { eretry=t+5000; tcp_close_safe(); rp_session_start(); }
            break;
        }
        case S_DONE: break;
        default: break;
    }
    // timeouts
    if ((s_state>=S_SESSREQ_CONNECT && s_state<=S_BANG_WAIT) && (int32_t)(t-s_deadline)>=0) {
        char m[48]; snprintf(m,sizeof(m),"timeout in state %d",(int)s_state); fail(m);
        tcp_close_safe();
    }
}


void rp_session_set_controller_state(const input_event_t* ev, uint32_t buttons)
{
    if (!ev) return;
    // map JP_BUTTON_* -> ChiakiControllerButton bitmask + sticks/triggers
    uint32_t b=0;
    #define M(jp,ck) do{ if(buttons&(jp)) b|=(ck);}while(0)
    M(JP_BUTTON_B1, CHIAKI_CONTROLLER_BUTTON_CROSS);
    M(JP_BUTTON_B2, CHIAKI_CONTROLLER_BUTTON_MOON);
    M(JP_BUTTON_B3, CHIAKI_CONTROLLER_BUTTON_BOX);
    M(JP_BUTTON_B4, CHIAKI_CONTROLLER_BUTTON_PYRAMID);
    M(JP_BUTTON_DL, CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT);
    M(JP_BUTTON_DR, CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT);
    M(JP_BUTTON_DU, CHIAKI_CONTROLLER_BUTTON_DPAD_UP);
    M(JP_BUTTON_DD, CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN);
    M(JP_BUTTON_L1, CHIAKI_CONTROLLER_BUTTON_L1);
    M(JP_BUTTON_R1, CHIAKI_CONTROLLER_BUTTON_R1);
    M(JP_BUTTON_L3, CHIAKI_CONTROLLER_BUTTON_L3);
    M(JP_BUTTON_R3, CHIAKI_CONTROLLER_BUTTON_R3);
    M(JP_BUTTON_S1, CHIAKI_CONTROLLER_BUTTON_SHARE);
    M(JP_BUTTON_S2, CHIAKI_CONTROLLER_BUTTON_OPTIONS);
    M(JP_BUTTON_A1, CHIAKI_CONTROLLER_BUTTON_PS);
    #undef M
    s_cstate.buttons=b;
    s_cstate.l2_state=(buttons&JP_BUTTON_L2)?0xff:0;
    s_cstate.r2_state=(buttons&JP_BUTTON_R2)?0xff:0;
    // analog[] 0..255 center 128 -> int16 -0x8000..0x7fff
    s_cstate.left_x =(int16_t)((ev->analog[ANALOG_LX]-128)*258);
    s_cstate.left_y =(int16_t)((ev->analog[ANALOG_LY]-128)*258);
    s_cstate.right_x=(int16_t)((ev->analog[ANALOG_RX]-128)*258);
    s_cstate.right_y=(int16_t)((ev->analog[ANALOG_RY]-128)*258);
}

bool rp_session_get_feedback(output_feedback_t* fb){ (void)fb; return false; }
rp_session_state_t rp_session_get_state(void){ return s_pub; }
bool rp_session_is_ready(void){ return s_pub==RP_SESS_READY; }

const char* rp_session_state_str(void)
{
    switch (s_state) {
        case S_IDLE: return "idle";
        case S_SESSREQ_CONNECT: case S_SESSREQ_WAIT: return "session-request";
        case S_CTRL_CONNECT: case S_CTRL_WAIT: return "ctrl";
        case S_TAKION_INIT: case S_TAKION_COOKIE: return "takion";
        case S_BIG: case S_BANG_WAIT: return "handshake";
        case S_STREAM: return "streaming";
        case S_ERROR: return s_err[0]?s_err:"error";
        case S_DONE: return "done";
    }
    return "?";
}

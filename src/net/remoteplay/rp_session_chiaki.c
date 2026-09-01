// rp_session_chiaki.c - real PS Remote Play session engine (chiaki-ng backed)
// SPDX-License-Identifier: AGPL-3.0  (links libchiaki, which is AGPL)
//
// Implements the same rp_session.h interface as rp_session_stub.c, but backed by
// the vendored chiaki library (src/lib/chiaki). Opens a headless Remote Play
// session to the configured PS5 and forwards controller input; no A/V decode
// (ci.audio_video_disabled). Modeled on jfedor2/remote-play-controller app/src/
// session.c.
//
// NOT YET BUILT: this file is compiled in only once the chiaki runtime is up
// (FreeRTOS + LWIP sockets + mbedTLS + the CHIAKI_PICO platform shim). Until
// then the usb2wifi build uses rp_session_stub.c. This is the integration seam
// so the wiring is settled ahead of the runtime work. See ps5-remoteplay-output.md.

#include "rp_session.h"
#include "rp_config.h"
#include "wifi_station.h"
#include "core/buttons.h"
#include "core/input_event.h"
#include "platform/platform.h"

#include <chiaki/session.h>
#include <chiaki/controller.h>
#include <chiaki/log.h>
#include <stdio.h>
#include <string.h>

static rp_session_state_t s_state = RP_SESS_IDLE;
static bool               s_started = false;
static ChiakiSession      s_session;
static ChiakiLog          s_log;
static ChiakiControllerState s_ctrl;
static uint8_t            s_rumble_l, s_rumble_r;
static bool              s_rumble_dirty;

// joypad-os buttons -> chiaki buttons
static uint32_t map_buttons(uint32_t b)
{
    uint32_t o = 0;
    if (b & JP_BUTTON_B1) o |= CHIAKI_CONTROLLER_BUTTON_CROSS;
    if (b & JP_BUTTON_B2) o |= CHIAKI_CONTROLLER_BUTTON_MOON;
    if (b & JP_BUTTON_B3) o |= CHIAKI_CONTROLLER_BUTTON_BOX;
    if (b & JP_BUTTON_B4) o |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
    if (b & JP_BUTTON_DL) o |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
    if (b & JP_BUTTON_DR) o |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
    if (b & JP_BUTTON_DU) o |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP;
    if (b & JP_BUTTON_DD) o |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN;
    if (b & JP_BUTTON_L1) o |= CHIAKI_CONTROLLER_BUTTON_L1;
    if (b & JP_BUTTON_R1) o |= CHIAKI_CONTROLLER_BUTTON_R1;
    if (b & JP_BUTTON_L3) o |= CHIAKI_CONTROLLER_BUTTON_L3;
    if (b & JP_BUTTON_R3) o |= CHIAKI_CONTROLLER_BUTTON_R3;
    if (b & JP_BUTTON_S2) o |= CHIAKI_CONTROLLER_BUTTON_OPTIONS;
    if (b & JP_BUTTON_S1) o |= CHIAKI_CONTROLLER_BUTTON_SHARE;
    if (b & JP_BUTTON_A2) o |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
    if (b & JP_BUTTON_A1) o |= CHIAKI_CONTROLLER_BUTTON_PS;
    return o;
}

static void event_cb(ChiakiEvent* event, void* user)
{
    (void)user;
    switch (event->type) {
        case CHIAKI_EVENT_CONNECTED:
            s_state = RP_SESS_READY;
            printf("[rp_chiaki] session CONNECTED\n");
            break;
        case CHIAKI_EVENT_QUIT:
            s_state = RP_SESS_ERROR;
            printf("[rp_chiaki] session QUIT: %d\n", event->quit.reason);
            break;
        case CHIAKI_EVENT_RUMBLE:
            s_rumble_l = event->rumble.left;
            s_rumble_r = event->rumble.right;
            s_rumble_dirty = true;
            break;
        default:
            break;
    }
}

void rp_session_init(void)
{
    s_state = RP_SESS_IDLE;
    s_started = false;
    chiaki_controller_state_set_idle(&s_ctrl);
}

void rp_session_start(void)
{
    if (s_started) return;
    rp_config_t* cfg = rp_config_get();
    if (!wifi_station_is_connected() || !cfg->have_registration) { s_state = RP_SESS_IDLE; return; }

    ChiakiConnectInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.ps5 = true;
    ci.host = cfg->ps5_ip;
    memcpy(ci.regist_key, cfg->regist_key, sizeof(ci.regist_key));
    memcpy(ci.morning, cfg->rp_key, sizeof(ci.morning));
    // chiaki expects the account id byte-reversed vs how we store/display it.
    for (int i = 0; i < RP_ACCOUNT_LEN; i++)
        ci.psn_account_id[i] = cfg->account_id[RP_ACCOUNT_LEN - 1 - i];
    ci.audio_video_disabled = CHIAKI_AUDIO_VIDEO_DISABLED;
    ci.enable_keyboard = false;
    ci.enable_dualsense = true;
    ci.auto_regist = false;
    chiaki_connect_video_profile_preset(&ci.video_profile,
        CHIAKI_VIDEO_RESOLUTION_PRESET_720p, CHIAKI_VIDEO_FPS_PRESET_60);

    chiaki_log_init(&s_log, CHIAKI_LOG_WARNING | CHIAKI_LOG_ERROR, chiaki_log_cb_print, NULL);
    memset(&s_session, 0, sizeof(s_session));

    ChiakiErrorCode err = chiaki_session_init(&s_session, &ci, &s_log);
    if (err != CHIAKI_ERR_SUCCESS) { s_state = RP_SESS_ERROR; return; }
    chiaki_session_set_event_cb(&s_session, event_cb, NULL);
    err = chiaki_session_start(&s_session);
    if (err != CHIAKI_ERR_SUCCESS) { chiaki_session_fini(&s_session); s_state = RP_SESS_ERROR; return; }

    s_started = true;
    s_state = RP_SESS_CONNECTING;
    printf("[rp_chiaki] session started -> %s\n", cfg->ps5_ip);
}

void rp_session_stop(void)
{
    if (s_started) {
        chiaki_session_stop(&s_session);
        chiaki_session_join(&s_session);
        chiaki_session_fini(&s_session);
        s_started = false;
    }
    s_state = RP_SESS_IDLE;
}

void rp_session_task(void)
{
    if (!s_started) {
        rp_config_t* cfg = rp_config_get();
        if (wifi_station_is_connected() && cfg->have_registration) rp_session_start();
    }
}

rp_session_state_t rp_session_get_state(void) { return s_state; }

const char* rp_session_state_str(void)
{
    switch (s_state) {
        case RP_SESS_IDLE:       return "idle";
        case RP_SESS_CONNECTING: return "connecting";
        case RP_SESS_READY:      return "ready";
        case RP_SESS_ERROR:      return "error";
        default:                 return "?";
    }
}

bool rp_session_is_ready(void) { return s_state == RP_SESS_READY; }

void rp_session_set_controller_state(const input_event_t* ev, uint32_t buttons)
{
    if (s_state != RP_SESS_READY) return;
    chiaki_controller_state_set_idle(&s_ctrl);
    s_ctrl.buttons = map_buttons(buttons);
    s_ctrl.l2_state = ev->analog[ANALOG_L2];
    s_ctrl.r2_state = ev->analog[ANALOG_R2];
    s_ctrl.left_x  = (int16_t)(((int)ev->analog[ANALOG_LX] - 128) * 256);
    s_ctrl.left_y  = (int16_t)(((int)ev->analog[ANALOG_LY] - 128) * 256);
    s_ctrl.right_x = (int16_t)(((int)ev->analog[ANALOG_RX] - 128) * 256);
    s_ctrl.right_y = (int16_t)(((int)ev->analog[ANALOG_RY] - 128) * 256);
    if (ev->has_motion) {
        // TODO: proper unit conversion (chiaki gyro=rad/s, accel=g) + axis tuning.
        s_ctrl.gyro_x = ev->gyro[0] / 32768.0f;
        s_ctrl.gyro_y = ev->gyro[1] / 32768.0f;
        s_ctrl.gyro_z = ev->gyro[2] / 32768.0f;
        s_ctrl.accel_x = ev->accel[0] / 8192.0f;
        s_ctrl.accel_y = ev->accel[1] / 8192.0f;
        s_ctrl.accel_z = ev->accel[2] / 8192.0f;
    }
    chiaki_session_set_controller_state(&s_session, &s_ctrl);
}

bool rp_session_get_feedback(output_feedback_t* fb)
{
    if (!s_rumble_dirty) return false;
    memset(fb, 0, sizeof(*fb));
    fb->rumble_left = s_rumble_l;
    fb->rumble_right = s_rumble_r;
    fb->dirty = true;
    s_rumble_dirty = false;
    return true;
}

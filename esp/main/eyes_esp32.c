// eyes_esp32.c - Procedural face on the LilyGo T-Display S3 AMOLED.
//
// Provides the display.h backend the face engine needs (display_clear +
// display_pixel + display_set_color over an 8-bit color-class canvas in
// PSRAM), plus a FreeRTOS task that ticks face_anim and blits it to the
// AMOLED. The canvas is rendered at ~2x the panel resolution and the blit
// box-downsamples 2x2, so edges come out anti-aliased and the accent class
// (Taby's red mouth) blends properly. Only built for the LilyGo board (see
// main/CMakeLists.txt), which sets EYES_SCALE.
#ifdef BOARD_LILYGO_TDISPLAY_S3_AMOLED

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "display.h"
#include "face_anim.h"
#include "rm67162_amoled.h"
#include "platform/platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

// Canvas matches the panel aspect (536:240) so nothing gets stretched by the
// blit; ~1.43x supersample for anti-aliasing. 768x344 = 264KB in PSRAM.
#define EYES_W 768
#define EYES_H 344

// 8-bit color-class canvas (0=black, 1=main, 2=accent), row-major, in PSRAM.
static uint8_t* s_fb = NULL;
static uint8_t s_color = FACE_COLOR_MAIN;

// --- display.h backend ---
void display_clear(void) { if (s_fb) memset(s_fb, 0, (size_t)EYES_W * EYES_H); }

void display_set_color(uint8_t color_index) { s_color = color_index; }

void display_pixel(int16_t x, int16_t y, bool on)
{
    if (!s_fb || (unsigned)x >= EYES_W || (unsigned)y >= EYES_H) return;
    s_fb[(size_t)x * EYES_H + y] = on ? s_color : 0;   // column-major (blit-friendly)
}

// Per-style main + accent colors (RGB565). A runtime tint (FACE.COLOR)
// overrides main and derives the accent at the style's stock main:accent
// brightness ratio, so pupils/glow keep their relationship to the new hue.
static uint16_t s_tint = 0;             // RGB565 override; 0 = style default

void eyes_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    s_tint = c ? c : 0x0841;            // near-black asked for: darkest visible
}

void eyes_reset_color(void) { s_tint = 0; }

static uint16_t scale565(uint16_t c, float k)
{
    int r = (int)(((c >> 11) & 31) * k);
    int g = (int)(((c >> 5) & 63) * k);
    int b = (int)((c & 31) * k);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t style_color(face_style_id s)
{
    if (s_tint) return s_tint;
    switch (s) {
        case FACE_STYLE_FACE:  return 0xFFFF;   // white (real Taby)
        case FACE_STYLE_ASTRO: return 0x5EBF;   // Astro core: light cyan-blue
        case FACE_STYLE_EYES:
        default:               return 0x07FF;   // cyan
    }
}

static uint16_t style_accent(face_style_id s)
{
    if (s_tint) {
        // astro's glow is a faint spill (~18% of main); other styles run
        // their accent (pupil / mouth interior) at ~55%
        return scale565(s_tint, s == FACE_STYLE_ASTRO ? 0.18f : 0.55f);
    }
    switch (s) {
        case FACE_STYLE_FACE:  return 0xE288;   // coral-red mouth interior
        case FACE_STYLE_ASTRO: return 0x0917;   // Astro glow: faint navy —
                                                // the reference's spill-glow
                                                // barely reads on the panel
        case FACE_STYLE_EYES:
        default:               return 0x0471;   // dark cyan pupil (~55% of main — visible on AMOLED)
    }
}

// ---- remote control (CDC FACE.* commands, see cdc_commands.c) ----
// While the companion drives the face, the self-demo pauses; it resumes
// after a quiet period so an unplugged bridge doesn't leave a frozen face.
static volatile uint32_t s_remote_until = 0;
#define REMOTE_HOLD_MS 15000

void face_track_cancel(void);
void face_remote_speak(int level)
{
    face_track_cancel();                 // live streaming wins over a track
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    face_set_speaking((float)level / 100.0f);
    s_remote_until = platform_time_ms() + REMOTE_HOLD_MS;
}

// ---- pre-shipped lip-sync track (FACE.TRACK) ----
// The bridge knows a reply's whole mouth envelope before playback starts, so
// it ships it once (chunked) and the face plays it on ITS OWN clock — zero
// radio traffic during speech and no per-frame link jitter. A live
// FACE.SPEAK (legacy streaming) cancels the track: live always wins.
#define FACE_TRACK_MAX 2048
static uint8_t  s_trk[FACE_TRACK_MAX];
static volatile int      s_trk_len = 0;     // playing length (0 = idle)
static int               s_trk_fill = 0;    // load cursor
static volatile uint32_t s_trk_step = 64;   // ms per envelope value
static volatile uint32_t s_trk_start = 0;   // playback start (platform ms)

void face_track_reset(void)
{
    s_trk_len = 0; s_trk_start = 0; s_trk_fill = 0;
}

bool face_track_append(const uint8_t* d, int n)
{
    if (n <= 0 || s_trk_fill + n > FACE_TRACK_MAX) return false;
    memcpy(s_trk + s_trk_fill, d, (size_t)n);
    s_trk_fill += n;
    return true;
}

void face_track_go(int step_ms, int delay_ms)
{
    if (s_trk_fill <= 0) return;
    s_trk_step = (step_ms > 0) ? (uint32_t)step_ms : 64;
    s_trk_start = platform_time_ms() + (uint32_t)(delay_ms > 0 ? delay_ms : 0);
    s_trk_len = s_trk_fill;
}

void face_track_cancel(void)
{
    s_trk_len = 0; s_trk_start = 0;
}

// Called every render tick; returns true while the track owns the mouth.
static bool face_track_tick(uint32_t now)
{
    if (s_trk_len <= 0 || s_trk_start == 0) return false;
    if ((int32_t)(now - s_trk_start) < 0) return true;   // armed, waiting
    uint32_t idx = (now - s_trk_start) / s_trk_step;
    if (idx >= (uint32_t)s_trk_len) {
        face_set_speaking(0.0f);
        s_trk_len = 0; s_trk_start = 0;
        return false;
    }
    int v = s_trk[idx];
    face_set_speaking((float)(v > 100 ? 100 : v) / 100.0f);
    return true;
}

void face_remote_state(const char* state)
{
    if (strcmp(state, "think") == 0) {
        face_set_emotion(FACE_EMO_SUSPICIOUS);   // narrowed, pondering
        face_look(0.45f, -0.55f);                // glance up-and-away
    } else {                                      // "idle" / "speak"
        face_set_emotion(FACE_EMO_NEUTRAL);
        face_look(0.0f, 0.0f);
    }
    s_remote_until = platform_time_ms() + REMOTE_HOLD_MS;
}

bool face_remote_emotion(const char* name)
{
    static const struct { const char* n; face_emotion e; } M[] = {
        {"neutral", FACE_EMO_NEUTRAL},   {"happy", FACE_EMO_HAPPY},
        {"sad", FACE_EMO_SAD},           {"angry", FACE_EMO_ANGRY},
        {"surprised", FACE_EMO_SURPRISED}, {"sleepy", FACE_EMO_SLEEPY},
        {"suspicious", FACE_EMO_SUSPICIOUS}, {"excited", FACE_EMO_EXCITED},
        {"love", FACE_EMO_LOVE},         {"wink", FACE_EMO_WINK},
        {"frustrated", FACE_EMO_FRUSTRATED},
    };
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
        if (strcmp(name, M[i].n) == 0) {
            face_set_emotion(M[i].e);
            s_remote_until = platform_time_ms() + REMOTE_HOLD_MS;
            return true;
        }
    }
    return false;
}

bool face_remote_style(const char* name)
{
    static const struct { const char* n; face_style_id st; } M[] = {
        {"eyes", FACE_STYLE_EYES},       {"face", FACE_STYLE_FACE},
        {"astro", FACE_STYLE_ASTRO},
        // legacy aliases
        {"lil", FACE_STYLE_EYES},        {"tab", FACE_STYLE_FACE},
        {"classic", FACE_STYLE_EYES},    {"taby", FACE_STYLE_FACE},
    };
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
        if (strcmp(name, M[i].n) == 0) {
            face_set_style(M[i].st);
            s_remote_until = platform_time_ms() + REMOTE_HOLD_MS;
            return true;
        }
    }
    return false;
}

void face_remote_look(int x_pct, int y_pct)
{
    face_look((float)x_pct / 100.0f, (float)y_pct / 100.0f);
    s_remote_until = platform_time_ms() + REMOTE_HOLD_MS;
}

static void eyes_task(void* arg)
{
    (void)arg;
    // Face role: BLE controller input is unused on this board — suppress the
    // perpetual scan (radio power + keeps 2.4GHz quiet near the controller).
    extern void btstack_host_suppress_scan(bool suppress);
    btstack_host_suppress_scan(true);

    amoled_init();
    amoled_set_shift(-15);   // center the face on the physical glass (the
                             // touch-circle strip offsets the active area)
    amoled_brightness(0xC8); // ~78%: plenty on AMOLED, meaningfully less battery
    extern bool pmu_init(void);
    pmu_init();   // battery telemetry + small-LiPo-safe charge config

    s_fb = heap_caps_malloc((size_t)EYES_W * EYES_H, MALLOC_CAP_SPIRAM);
    if (!s_fb) {
        ESP_LOGE("eyes", "canvas alloc failed (%d bytes)", EYES_W * EYES_H);
        vTaskDelete(NULL);
        return;
    }

    face_init(EYES_W, EYES_H);
    face_set_style(FACE_STYLE_FACE);

    // Self-driving demo until wired to real events: cycle emotions to show the
    // interrupt-anytime spring transitions, and rotate through the styles.
    // Mostly neutral (idle wander + blinks make it feel alive), with a short
    // emotion burst every few seconds — closer to how a companion behaves.
    static const face_emotion bursts[] = {
        FACE_EMO_HAPPY, FACE_EMO_SURPRISED, FACE_EMO_SUSPICIOUS,
        FACE_EMO_EXCITED, FACE_EMO_SAD, FACE_EMO_SLEEPY, FACE_EMO_ANGRY,
    };
    uint32_t next_emo = 4000, next_style = 0;
    int burst_i = 0, style = FACE_STYLE_FACE;
    bool in_burst = false;

    for (;;) {
        uint32_t now = platform_time_ms();
        if (face_track_tick(now)) {
            s_remote_until = now + REMOTE_HOLD_MS;   // pause the idle demo
        }
        if (now < s_remote_until) {
            next_emo = now + 2000;   // demo paused: companion is driving
        } else if (now >= next_emo) {
            if (in_burst) {
                face_set_emotion(FACE_EMO_NEUTRAL);
                next_emo = now + 3800;
            } else {
                face_set_emotion(bursts[burst_i]);
                burst_i = (burst_i + 1) % (int)(sizeof(bursts) / sizeof(bursts[0]));
                next_emo = now + 2200;
            }
            in_burst = !in_burst;
        }
        (void)style; (void)next_style;   // style rotation off while tuning Taby
        face_tick(now);
        // Idle throttle: while the face is settled (only the breathing bob
        // moves), render/blit at ~1/8 rate — the panel keeps its last frame.
        static uint8_t idle_skip = 0;
        if (!face_settled() || (++idle_skip & 7) == 0) {
            face_render();
            amoled_blit_idx8(s_fb, EYES_W, EYES_H,
                             style_color(face_get_style()),
                             style_accent(face_get_style()));
        }
        // ~30fps cap: the full render+blit hammers PSRAM/cache hard enough
        // to disturb the BLE controller's timing (supervision timeouts mid-
        // morph); a 3x lower duty cycle keeps the radio fed
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

void eyes_start(void)
{
    // Pin to core 1 (APP): the BT controller lives on core 0 — keep the
    // heavy render workload off its core entirely.
    xTaskCreatePinnedToCore(eyes_task, "eyes", 8192, NULL, 3, NULL, 1);
}

#endif // BOARD_LILYGO_TDISPLAY_S3_AMOLED

// ws2812_esp32.c - NeoPixel driver for ESP32-S3 via RMT
//
// Uses ESP-IDF RMT peripheral with bytes encoder for hardware-timed WS2812
// waveform generation. No CPU involvement during transmission.
//
// Feather ESP32-S3: NeoPixel on GPIO33, power on GPIO21 (active high)

#include "core/services/leds/neopixel/ws2812.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

#ifdef BOARD_FEATHER_ESP32S3

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"

#define NEOPIXEL_PIN       33
#define NEOPIXEL_POWER_PIN 21

// RMT resolution: 10 MHz → 100ns per tick
#define RMT_RESOLUTION_HZ  10000000

// WS2812 timing (in 100ns ticks):
// T0H = 300ns (3 ticks), T0L = 900ns (9 ticks)
// T1H = 900ns (9 ticks), T1L = 300ns (3 ticks)

static rmt_channel_handle_t rmt_chan = NULL;
static rmt_encoder_handle_t rmt_encoder = NULL;
static bool neopixel_ready = false;

// Override color (set by app for mode indication)
static uint8_t override_r = 0, override_g = 0, override_b = 0;
static bool has_override_color = false;

// Current pixel color (avoid redundant sends)
static uint8_t cur_r = 0, cur_g = 0, cur_b = 0;
static bool cur_valid = false;

// Timing
static uint32_t last_update_ms = 0;
static int tic = 0;

// Profile indicator state
typedef enum {
    LED_IDLE,
    LED_BLINK_ON,
    LED_BLINK_OFF,
} led_state_t;

static led_state_t led_state = LED_IDLE;
static uint8_t blinks_remaining = 0;
static uint32_t state_change_ms = 0;

#define BLINK_OFF_MS    200
#define BLINK_ON_MS     100

// Breathing brightness scale (0-255) — triangle wave with quadratic easing
static inline uint8_t breathing_scale(int t)
{
    int phase = t % 150;  // ~3s cycle at 20ms per tic
    int ramp = phase < 75 ? phase : (150 - phase);
    return 4 + (uint8_t)((uint32_t)ramp * ramp * 251 / 5625);
}

static void ws2812_send_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    if (!rmt_chan || !rmt_encoder) return;

    // WS2812 expects GRB byte order
    uint8_t grb[3] = { g, r, b };

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags.eot_level = 0,
    };

    rmt_transmit(rmt_chan, rmt_encoder, grb, sizeof(grb), &tx_cfg);
    rmt_tx_wait_all_done(rmt_chan, 100);
}

// Global brightness scale (255 = unscaled, the default).
static uint8_t global_brightness = 255;

// Within 1/255 of round(c * scale / 255), exactly c at 255 and exactly 0 at 0.
static inline uint8_t scale_channel(uint8_t c, uint8_t scale)
{
    return (uint8_t)(((uint32_t) c * (scale + 1u)) >> 8);
}

static void neo_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    // Cache the *requested* color, not the scaled one, so a brightness change
    // isn't swallowed by the dedupe — neopixel_set_brightness() drops the
    // cache instead.
    if (cur_valid && cur_r == r && cur_g == g && cur_b == b) return;
    ws2812_send_pixel(scale_channel(r, global_brightness),
                      scale_channel(g, global_brightness),
                      scale_channel(b, global_brightness));
    cur_r = r; cur_g = g; cur_b = b;
    cur_valid = true;
}

void neopixel_set_brightness(uint8_t brightness)
{
    if (global_brightness == brightness) return;
    global_brightness = brightness;
    cur_valid = false;  // force a re-send at the new scale
}

uint8_t neopixel_get_brightness(void)
{
    return global_brightness;
}

static void neo_set_off(void)
{
    neo_set_color(0, 0, 0);
}

void neopixel_init(void)
{
    // Enable NeoPixel power
    gpio_config_t pwr_cfg = {
        .pin_bit_mask = (1ULL << NEOPIXEL_POWER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_cfg);
    gpio_set_level(NEOPIXEL_POWER_PIN, 1);

    // Small delay for power stabilization
    platform_sleep_ms(10);

    // Configure RMT TX channel
    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = NEOPIXEL_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t err = rmt_new_tx_channel(&chan_cfg, &rmt_chan);
    if (err != ESP_OK) {
        printf("[neopixel] RMT channel init failed: %d\n", err);
        return;
    }

    // Configure bytes encoder with WS2812 timing
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = {
            .duration0 = 3,   // T0H = 300ns
            .level0 = 1,
            .duration1 = 9,   // T0L = 900ns
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = 9,   // T1H = 900ns
            .level0 = 1,
            .duration1 = 3,   // T1L = 300ns
            .level1 = 0,
        },
        .flags.msb_first = 1,
    };

    err = rmt_new_bytes_encoder(&enc_cfg, &rmt_encoder);
    if (err != ESP_OK) {
        printf("[neopixel] RMT encoder init failed: %d\n", err);
        rmt_del_channel(rmt_chan);
        rmt_chan = NULL;
        return;
    }

    err = rmt_enable(rmt_chan);
    if (err != ESP_OK) {
        printf("[neopixel] RMT enable failed: %d\n", err);
        return;
    }

    neopixel_ready = true;
    printf("[neopixel] NeoPixel ready (RMT on GPIO%d, power GPIO%d)\n",
           NEOPIXEL_PIN, NEOPIXEL_POWER_PIN);

    // Start with LED off
    ws2812_send_pixel(0, 0, 0);
}

void neopixel_set_override_color(uint8_t r, uint8_t g, uint8_t b)
{
    override_r = r;
    override_g = g;
    override_b = b;
    has_override_color = true;
}

void neopixel_indicate_profile(uint8_t profile_index)
{
    if (led_state == LED_IDLE) {
        blinks_remaining = profile_index + 1;
        led_state = LED_BLINK_OFF;
        state_change_ms = platform_time_ms();
    }
}

bool neopixel_is_indicating(void)
{
    return led_state != LED_IDLE;
}

void neopixel_task(int pat)
{
    if (!neopixel_ready) return;

    uint32_t now = platform_time_ms();

    // Profile indicator state machine
    if (led_state != LED_IDLE) {
        uint32_t elapsed = now - state_change_ms;

        switch (led_state) {
            case LED_BLINK_OFF:
                neo_set_off();
                if (elapsed >= BLINK_OFF_MS) {
                    blinks_remaining--;
                    if (blinks_remaining > 0) {
                        led_state = LED_BLINK_ON;
                    } else {
                        led_state = LED_IDLE;
                    }
                    state_change_ms = now;
                }
                break;

            case LED_BLINK_ON:
                if (has_override_color) {
                    neo_set_color(override_r, override_g, override_b);
                } else {
                    neo_set_color(64, 64, 64);
                }
                if (elapsed >= BLINK_ON_MS) {
                    led_state = LED_BLINK_OFF;
                    state_change_ms = now;
                }
                break;

            default:
                led_state = LED_IDLE;
                break;
        }
        return;
    }

    // Rate limit updates (~50Hz)
    if (now - last_update_ms < 20) return;
    last_update_ms = now;
    tic++;

    if (has_override_color) {
        if (pat == 0) {
            // Not connected: breathing pulse with override color
            uint8_t s = breathing_scale(tic);
            neo_set_color(
                (override_r * s) / 255,
                (override_g * s) / 255,
                (override_b * s) / 255);
        } else {
            // Connected: solid override color
            neo_set_color(override_r, override_g, override_b);
        }
        return;
    }

    if (pat == 0) {
        // No connection: breathing blue pulse
        uint8_t s = breathing_scale(tic);
        neo_set_color(0, 0, s);
    } else {
        // Connected: solid blue
        neo_set_color(0, 0, 64);
    }
}

void neopixel_set_custom_colors(const uint8_t colors[][3], uint8_t count)
{
    (void)colors;
    (void)count;
}

bool neopixel_has_custom_colors(void)
{
    return false;
}

void neopixel_set_pulse_mask(uint16_t mask)
{
    (void)mask;
}

void neopixel_set_press_mask(uint16_t mask)
{
    (void)mask;
}

#else
// ============================================================================
// Non-Feather ESP32 boards: stub (no NeoPixel)
// ============================================================================

void neopixel_init(void)
{
    printf("[neopixel] ESP32 stub initialized (no LED)\n");
}

void neopixel_task(int pat)
{
    (void)pat;
}

void neopixel_indicate_profile(uint8_t profile_index)
{
    (void)profile_index;
}

bool neopixel_is_indicating(void)
{
    return false;
}

void neopixel_set_custom_colors(const uint8_t colors[][3], uint8_t count)
{
    (void)colors;
    (void)count;
}

bool neopixel_has_custom_colors(void)
{
    return false;
}

void neopixel_set_pulse_mask(uint16_t mask)
{
    (void)mask;
}

void neopixel_set_press_mask(uint16_t mask)
{
    (void)mask;
}

void neopixel_set_override_color(uint8_t r, uint8_t g, uint8_t b)
{
    (void)r; (void)g; (void)b;
}

// Boards in this branch have no addressable LED, but the shared CDC layer
// links against these, so they still have to exist. Keep the value so a
// LED.BRIGHT round-trip reads back what was written.
static uint8_t global_brightness = 255;

void neopixel_set_brightness(uint8_t brightness)
{
    global_brightness = brightness;
}

uint8_t neopixel_get_brightness(void)
{
    return global_brightness;
}

#endif // BOARD_FEATHER_ESP32S3

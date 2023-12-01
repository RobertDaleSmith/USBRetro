/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "ws2812.pio.h"

#define NUM_PIXELS 1

#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
// default to pin 2 if the board doesn't have a default WS2812 pin defined
#define WS2812_PIN 2
#endif

#define IS_RGBW true

extern bool is_fun;

PIO pio;
uint sm;

static absolute_time_t init_time;
static absolute_time_t current_time;
static absolute_time_t loop_time;
static const int64_t reset_period = 10000;
int dir = 1; // direction
int tic = 0; // ticker

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void pattern_snakes(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

void pattern_random(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand());
}

void pattern_sparkle(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand() % 16 ? 0 : 0xffffffff);
}

void pattern_greys(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
        if (++t >= max) t = 0;
    }
}

void pattern_blues(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x00001);
        if (++t >= max) t = 0;
    }
}

void pattern_purples(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        uint8_t intensity = t; // Adjust the intensity value for a darker effect
        put_pixel(urgb_u32(intensity / 10, 0, intensity / 1)); // Dark purple color (red + blue)
        if (++t >= max) t = 0;
    }
}

void pattern_reds(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x00100);
        if (++t >= max) t = 0;
    }
}

void pattern_greens(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        uint8_t intensity = t; // Adjust the intensity value for a darker effect
        put_pixel(urgb_u32(0, intensity / 10, 0));
        if (++t >= max) t = 0;
    }
}

void pattern_blue(uint len, uint t) {
    int max = 100;
    put_pixel(max * 0x00001);
}

void pattern_red(uint len, uint t) {
    put_pixel(urgb_u32(64, 0, 0)); // red
}

void pattern_green(uint len, uint t) {
    put_pixel(urgb_u32(0, 64, 0)); // green
}

void pattern_purple(uint len, uint t) {
    put_pixel(urgb_u32(6, 0, 64)); // purple
}

void pattern_yellow(uint len, uint t) {
    put_pixel(urgb_u32(64, 64, 0)); // yellow
}

void pattern_br(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0, 0xff));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0xff, 0, 0));
        else
            put_pixel(urgb_u32(0xff, 0, 0));
    }
}

void pattern_brg(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0, 0xff));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0xff, 0, 0));
        else
            put_pixel(urgb_u32(0, 0xff, 0));
    }
}

void pattern_brgp(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0, 0, 0xff)); // blue
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0xff, 0, 0)); // red
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0xff, 0)); // green
        else
            put_pixel(urgb_u32(20, 0, 40)); // purple
    }
}

void pattern_brgpy(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0, 0, 0xff)); // blue
        else if (x >= 10 && x < 20)
            put_pixel(urgb_u32(0xff, 0, 0)); // red
        else if (x >= 20 && x < 30)
            put_pixel(urgb_u32(0, 0xff, 0)); // green
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(20, 0, 40)); // purple
        else
            put_pixel(urgb_u32(0xff, 0xff, 0)); // yellow
    }
}

typedef void (*pattern)(uint len, uint t);
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
#ifdef CONFIG_XB1
        {pattern_greens,  "Greens"},     // 0 controllers
        {pattern_green,   "Green"},      // 1 controller
        {pattern_blue,    "Blue"},       // 2 controllers
        {pattern_red,     "Red"},        // 3 controllers
        {pattern_purple,  "Purple"},     // 4 controllers
        {pattern_yellow,  "Yellow"},     // 5 controllers
#else
#ifdef CONFIG_NGC
        {pattern_purples, "Purples"},    // 0 controllers
        {pattern_purple,  "Purple"},     // 1 controller
        {pattern_blue,    "Blue"},       // 2 controllers
        {pattern_red,     "Red"},        // 3 controllers
        {pattern_green,   "Green"},      // 4 controllers
        {pattern_yellow,  "Yellow"},     // 5 controllers
#else
#ifdef CONFIG_NUON
        {pattern_reds,    "Reds"},       // 0 controllers
        {pattern_red,     "Red"},        // 1 controller
        {pattern_blue,    "Blue"},       // 2 controllers
        {pattern_green,   "Green"},      // 3 controllers
        {pattern_purple,  "Purple"},     // 4 controllers
        {pattern_yellow,  "Yellow"},     // 5 controllers
#else//CONFIG_PCE
        {pattern_blues,   "Blues"},      // 0 controllers
        {pattern_blue,    "Blue"},       // 1 controller
        {pattern_red,     "Red"},        // 2 controllers
        {pattern_green,   "Green"},      // 3 controllers
        {pattern_purple,  "Purple"},     // 4 controllers
        {pattern_yellow,  "Yellow"},     // 5 controllers
#endif
#endif
#endif
        {pattern_random,  "Random data"},// fun
        {pattern_sparkle, "Sparkles"},
        {pattern_snakes,  "Snakes!"},
        {pattern_greys,   "Greys"},
        {pattern_br,      "B R"},        // 2 controllers alt
        {pattern_brg,     "B R G"},      // 3 controllers alt
        {pattern_brgp,    "B R G P"},    // 4 controllers alt
        {pattern_brgpy,   "B R G P Y"},  // 5 controllers alt
};

void neopixel_init()
{
    pio = pio0;

    // Load neopixel program and config state machine to run it.
    uint offset = pio_add_program(pio, &ws2812_program);
    sm = pio_claim_unused_sm(pio, true);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    put_pixel(urgb_u32(0x40, 0x20, 0x00)); // init color value (holds color on auto sel boot)
}

void neopixel_task(int pat)
{
    if (pat > 5) pat = 5;
    if (pat && is_fun) pat = 6;

    current_time = get_absolute_time();

    if (absolute_time_diff_us(init_time, current_time) > reset_period) {
        pattern_table[pat].pat(NUM_PIXELS, tic);

        tic += dir;

        init_time = get_absolute_time();
    }
}

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

void pattern_blue(uint len, uint t) {
    int max = 100;
    put_pixel(max * 0x00001);
}

void pattern_gb(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0, 0, 0xff));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(urgb_u32(0, 0, 0xff));
    }
}

void pattern_rgb(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0, 0, 0xff));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0xff, 0));
        else
            put_pixel(urgb_u32(0, 0, 0xff));
    }
}

void pattern_rgby(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(urgb_u32(0xff, 0xff, 0));
    }
}

void pattern_rgbyp(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 10 && x < 20)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 20 && x < 30)
            put_pixel(urgb_u32(0, 0, 0xff));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0xff, 0xff, 0));
        else
            put_pixel(urgb_u32(20, 0, 40));
    }
}

typedef void (*pattern)(uint len, uint t);
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        {pattern_blues,   "Blues"},                 // 0 controllers
        {pattern_blue,    "Blue"},                  // 1 controller
        {pattern_gb,      "Green Blue"},            // 2 controllers
        {pattern_rgb,     "Red Green Blue"},        // 3 controllers
        {pattern_rgby,    "Red Green Blue Yellow"}, // 4 controllers
        {pattern_rgbyp,   "Red Green Blue Yellow Purple"}, // 5 controllers
        {pattern_random,  "Random data"},           
        {pattern_sparkle, "Sparkles"},
        {pattern_snakes,  "Snakes!"},
        {pattern_greys,   "Greys"},
};

void neopixel_init()
{
    pio = pio0;

    // Load neopixel program and config state machine to run it.
    uint offset = pio_add_program(pio, &ws2812_program);
    sm = pio_claim_unused_sm(pio, true);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
}

void neopixel_task(int pat)
{
    if (pat > 5) pat = 5;

    current_time = get_absolute_time();

    if (absolute_time_diff_us(init_time, current_time) > reset_period) {
        pattern_table[pat].pat(NUM_PIXELS, tic);

        tic += dir;

        init_time = get_absolute_time();
    }
}

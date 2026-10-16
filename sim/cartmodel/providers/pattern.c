/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * "pattern" provider: test patterns for checking the stream end to end.
 * Options: "0".."3" (default "0"): 0 vertical bars, 1 animated checkerboard,
 * 2 scrolling bars, 3 frame counter strip over diagonal stripes.
 */
#include <stdlib.h>
#include <string.h>

#include "../fcpico_cart.h"
#include "draw.h"

typedef struct {
    fcbus_core_t *core;
    int pattern;
} pattern_t;

static const uint8_t palette[FCBUS_PAL_LEN] = {
    0x0F, 0x00, 0x10, 0x20, 0x0F, 0x06, 0x16, 0x26, 0x0F, 0x09, 0x19, 0x29, 0x0F, 0x01, 0x21, 0x31,
    0x0F, 0x00, 0x10, 0x20, 0x0F, 0x06, 0x16, 0x26, 0x0F, 0x09, 0x19, 0x29, 0x0F, 0x01, 0x21, 0x31,
};

static uint8_t pixel(int pattern, uint32_t frame, int x, int y) {
    switch (pattern) {
    case 0:
        return (uint8_t)((x / 32) & 3);
    case 1:
        return (uint8_t)(((x / 8) ^ (y / 8) ^ (int)(frame / 30)) & 3);
    case 2:
        return (uint8_t)(((x / 16) + (int)(frame / 15)) & 3);
    default:
        if (y < 16) {
            return (uint8_t)((frame >> ((x / 16) & 7)) & 3);
        }
        return (uint8_t)(((x + y + (int)frame) >> 3) & 3);
    }
}

static void draw(pattern_t *p, uint32_t frame) {
    uint16_t *back = fcbus_core_stream_back(p->core);
    uint8_t line[256];
    for (int y = 0; y < VRAM_LINES; y++) {
        for (int x = 0; x < 256; x++) {
            line[x] = pixel(p->pattern, frame, x, y);
        }
        fcpico_draw_line(back, y, line);
    }
    fcbus_core_publish(p->core);
}

static void *create(fcbus_core_t *core, const char *options) {
    int pattern = 0;
    if (options != NULL && options[0] != '\0') {
        if (options[1] != '\0' || options[0] < '0' || options[0] > '3') {
            return NULL;
        }
        pattern = options[0] - '0';
    }
    pattern_t *p = calloc(1, sizeof *p);
    if (p != NULL) {
        p->core = core;
        p->pattern = pattern;
    }
    return p;
}

static void destroy(void *state) {
    free(state);
}

static void console_init(void *state, uint8_t stage) {
    pattern_t *p = state;
    uint8_t attr[FCBUS_ATTR_LEN];
    (void)stage;
    for (int i = 0; i < FCBUS_ATTR_LEN; i++) {
        attr[i] = (uint8_t)((i & 1) ? 0x1B : 0xE4);
    }
    fcbus_core_set_palette(p->core, palette);
    fcbus_core_set_attr_table(p->core, attr);
    fcbus_core_request_data_mode(p->core);
    draw(p, 0);
}

static void frame(void *state, uint8_t pad, uint32_t frame_number) {
    pattern_t *p = state;
    (void)pad;
    if (p->pattern != 0 && fcbus_core_back_is_free(p->core)) {
        draw(p, frame_number);
    }
}

const fcpico_provider_t fcpico_provider_pattern = {
    FCPICO_CART_ABI_VERSION, "pattern", create, destroy, console_init, frame,
};

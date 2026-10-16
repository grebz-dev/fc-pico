/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * "demo" provider: a minimal live application. The D-pad moves a 16x16 square
 * over a grid, A changes its colour with a single palette poke, and B plays a
 * short pulse-channel tone. It exercises dynamic frames, controller input,
 * per-frame table updates and APU writes without any external assets.
 */
#include <stdlib.h>

#include "../fcpico_cart.h"
#include "draw.h"

#define SQUARE 16

typedef struct {
    fcbus_core_t *core;
    int x, y;
    uint8_t colour;
    uint8_t last_pad;
} demo_t;

static const uint8_t palette[FCBUS_PAL_LEN] = {
    0x0F, 0x00, 0x10, 0x16, 0x0F, 0x00, 0x10, 0x16, 0x0F, 0x00, 0x10, 0x16, 0x0F, 0x00, 0x10, 0x16,
    0x0F, 0x00, 0x10, 0x16, 0x0F, 0x00, 0x10, 0x16, 0x0F, 0x00, 0x10, 0x16, 0x0F, 0x00, 0x10, 0x16,
};
static const uint8_t square_colours[4] = {0x16, 0x2A, 0x12, 0x27};

static void draw(demo_t *d) {
    uint16_t *back = fcbus_core_stream_back(d->core);
    uint8_t line[256];
    for (int y = 0; y < VRAM_LINES; y++) {
        for (int x = 0; x < 256; x++) {
            uint8_t grid = ((x & 31) == 0 || (y & 31) == 0) ? 2 : 1;
            bool inside = x >= d->x && x < d->x + SQUARE && y >= d->y && y < d->y + SQUARE;
            line[x] = inside ? 3 : grid;
        }
        fcpico_draw_line(back, y, line);
    }
    fcbus_core_publish(d->core);
}

static void *create(fcbus_core_t *core, const char *options) {
    if (options != NULL && options[0] != '\0') {
        return NULL;
    }
    demo_t *d = calloc(1, sizeof *d);
    if (d != NULL) {
        d->core = core;
        d->x = (256 - SQUARE) / 2;
        d->y = (240 - SQUARE) / 2;
    }
    return d;
}

static void destroy(void *state) {
    free(state);
}

static void console_init(void *state, uint8_t stage) {
    demo_t *d = state;
    uint8_t attr[FCBUS_ATTR_LEN] = {0};
    (void)stage;
    fcbus_core_set_palette(d->core, palette);
    fcbus_core_set_attr_table(d->core, attr);
    fcbus_core_request_data_mode(d->core);
    draw(d);
}

static void frame(void *state, uint8_t pad, uint32_t frame_number) {
    demo_t *d = state;
    uint8_t pressed = (uint8_t)(pad & ~d->last_pad);
    (void)frame_number;
    d->last_pad = pad;

    if (pad & KEY_LEFT) d->x -= 2;
    if (pad & KEY_RIGHT) d->x += 2;
    if (pad & KEY_UP) d->y -= 2;
    if (pad & KEY_DOWN) d->y += 2;
    d->x = d->x < 0 ? 0 : d->x > 256 - SQUARE ? 256 - SQUARE : d->x;
    d->y = d->y < 0 ? 0 : d->y > 240 - SQUARE ? 240 - SQUARE : d->y;

    if (pressed & KEY_A) {
        d->colour = (uint8_t)((d->colour + 1) & 3);
        fcbus_core_set_pal(d->core, 3, square_colours[d->colour]);
    }
    if (pressed & KEY_B) {
        fcbus_core_apu_write(d->core, 0x15, 0x01); /* enable pulse 1 */
        fcbus_core_apu_write(d->core, 0x00, 0x9F); /* duty 50%, constant volume 15 */
        fcbus_core_apu_write(d->core, 0x02, 0xFD); /* period low: about 440 Hz */
        fcbus_core_apu_write(d->core, 0x03, 0x08); /* period high, length counter */
    }
    if (fcbus_core_back_is_free(d->core)) {
        draw(d);
    }
}

const fcpico_provider_t fcpico_provider_demo = {
    FCPICO_CART_ABI_VERSION, "demo", create, destroy, console_init, frame,
};

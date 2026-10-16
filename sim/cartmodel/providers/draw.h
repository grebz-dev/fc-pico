/* SPDX-License-Identifier: BSD-3-Clause */
/* Shared drawing helper for the built-in providers. */
#ifndef FCPICO_PROVIDER_DRAW_H
#define FCPICO_PROVIDER_DRAW_H

#include <stdint.h>

#include "fcbus_core.h"

/* Writes 256 2-bit pixels of one line in the stream format: each word holds 8
 * pixels, plane 0 in the low byte and plane 1 in the high byte, MSB first. */
static inline void fcpico_draw_line(uint16_t *stream, int line, const uint8_t pixels[256]) {
    for (int tile = 0; tile < VRAM_TILE_COLS; tile++) {
        uint16_t word = 0;
        for (int x = 0; x < 8; x++) {
            uint8_t c = pixels[tile * 8 + x];
            word = (uint16_t)((word << 1) | (c & 1u) | ((c & 2u) << 7));
        }
        *fcbus_stream_word(stream, line, tile) = word;
    }
}

#endif

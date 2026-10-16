/**
 * @file fcbus_pack.c
 * @brief Pixel encoding of the picture stream.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcbus_pack.h"

#include "fcbus_core.h"

void fcbus_stream_put_line(uint16_t *stream, int line, const uint8_t pixels[256]) {
    for (int tile = 0; tile < VRAM_TILE_COLS; tile++) {
        uint16_t word = 0;
        for (int x = 0; x < 8; x++) {
            uint8_t c = pixels[tile * 8 + x];
            word = (uint16_t)((word << 1) | (c & 1u) | ((c & 2u) << 7));
        }
        *fcbus_stream_word(stream, line, tile) = word;
    }
}

void fcbus_stream_get_line(const uint16_t *stream, int line, uint8_t pixels[256]) {
    for (int tile = 0; tile < VRAM_TILE_COLS; tile++) {
        uint16_t word = stream[VRAM_HEAD_WORDS + line * VRAM_TILE_COLS + tile];
        for (int x = 0; x < 8; x++) {
            int bit = 7 - x;
            pixels[tile * 8 + x] = (uint8_t)(((word >> bit) & 1u) | (((word >> (8 + bit)) & 1u) << 1));
        }
    }
}

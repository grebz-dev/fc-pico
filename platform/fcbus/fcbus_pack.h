/**
 * @file fcbus_pack.h
 * @brief Pixel encoding of the picture stream.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The PPU fetches each 8-pixel tile row as two bit planes. In the stream a tile row
 * is one 16-bit word: plane 0 in the low byte and plane 1 in the high byte, leftmost
 * pixel in the most significant bit. Words are placed with fcbus_stream_word().
 */
#ifndef FCBUS_PACK_H
#define FCBUS_PACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Writes 256 2-bit pixels (values 0..3) as stream line `line` (0..239). */
void fcbus_stream_put_line(uint16_t *stream, int line, const uint8_t pixels[256]);

/** Reads stream line `line` back into 256 2-bit pixels. */
void fcbus_stream_get_line(const uint16_t *stream, int line, uint8_t pixels[256]);

#ifdef __cplusplus
}
#endif

#endif /* FCBUS_PACK_H */

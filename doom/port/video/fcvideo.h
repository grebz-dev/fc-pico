/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef FCVIDEO_H
#define FCVIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fcbus_protocol.h"

#define FCVIDEO_SRC_WIDTH 320
#define FCVIDEO_SRC_HEIGHT 200
#define FCVIDEO_WIDTH (VRAM_TILE_COLS * 8)
#define FCVIDEO_FRAME_BYTES (VRAM_LINES * FCVIDEO_WIDTH)

/* err[p][idx] and lut[p][idx][bayer] are generated from PLAYPAL and the
 * selected NES preset by the host reference. They remain immutable for a
 * game palette; console palette flashes replace only the 16 mailbox bytes. */
typedef struct {
    const uint8_t *err; /* 4 * 256 bytes */
    const uint8_t *lut; /* 4 * 256 * 16 bytes */
    uint8_t palette[MBX_PAL_LEN];
} fcvideo_tables_t;

typedef struct {
    fcvideo_tables_t tables;
    uint8_t frame[FCVIDEO_FRAME_BYTES + 16]; /* prefetch past line 239 is zero */
    uint8_t previous_attr[MBX_ATTR_LEN];
    bool have_previous;
} fcvideo_t;

void fcvideo_init(fcvideo_t *video, const fcvideo_tables_t *tables);
size_t fcvideo_sizeof(void);
void fcvideo_set_palette(fcvideo_t *video, const uint8_t palette[MBX_PAL_LEN]);
/* Writes a full v2 buffer and returns the selected attribute table in attr.
 * Pass reset_hysteresis=true for the first frame after a palette preset change. */
void fcvideo_convert(fcvideo_t *video,
                     const uint8_t source[FCVIDEO_SRC_HEIGHT * FCVIDEO_SRC_WIDTH],
                     uint8_t stream[VRAM_BUF_BYTES_V2],
                     uint8_t attr[MBX_ATTR_LEN], bool reset_hysteresis);

#endif

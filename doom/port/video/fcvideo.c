/* SPDX-License-Identifier: BSD-3-Clause */
#include "fcvideo.h"

#include <string.h>

void fcvideo_init(fcvideo_t *video, const fcvideo_tables_t *tables) {
    memset(video, 0, sizeof(*video));
    video->tables = *tables;
}

size_t fcvideo_sizeof(void) { return sizeof(fcvideo_t); }

void fcvideo_set_palette(fcvideo_t *video, const uint8_t palette[MBX_PAL_LEN]) {
    memcpy(video->tables.palette, palette, MBX_PAL_LEN);
}

static uint8_t attr_get(const uint8_t attr[MBX_ATTR_LEN], int bx, int by) {
    int index = (bx >> 1) + ((by >> 1) << 3);
    int shift = ((bx & 1) + ((by & 1) << 1)) * 2;
    return (attr[index] >> shift) & 3u;
}

static void attr_set(uint8_t attr[MBX_ATTR_LEN], int bx, int by, uint8_t palette) {
    int index = (bx >> 1) + ((by >> 1) << 3);
    int shift = ((bx & 1) + ((by & 1) << 1)) * 2;
    attr[index] = (uint8_t)((attr[index] & ~(3u << shift)) | (palette << shift));
}

static void choose_attributes(fcvideo_t *video, uint8_t attr[MBX_ATTR_LEN]) {
    memset(attr, 0, MBX_ATTR_LEN);
    for (int by = 0; by < VRAM_LINES / 16; by++) {
        for (int bx = 0; bx < FCVIDEO_WIDTH / 16; bx++) {
            uint32_t costs[4] = {0};
            for (int y = by * 16; y < by * 16 + 16; y++) {
                for (int x = bx * 16; x < bx * 16 + 16; x++) {
                    uint8_t index = video->frame[y * FCVIDEO_WIDTH + x];
                    for (int p = 0; p < 4; p++) costs[p] += video->tables.err[p * 256 + index];
                }
            }
            uint8_t best = 0;
            for (uint8_t p = 1; p < 4; p++) {
                if (costs[p] < costs[best]) best = p;
            }
            if (video->have_previous) {
                uint8_t old = attr_get(video->previous_attr, bx, by);
                /* Python reference: retain old unless best is strictly more
                 * than 12% cheaper. Integer comparison avoids floating point. */
                if (best != old && costs[best] * 100u >= costs[old] * 88u) best = old;
            }
            attr_set(attr, bx, by, best);
        }
    }
}

void fcvideo_convert(fcvideo_t *video,
                     const uint8_t source[FCVIDEO_SRC_HEIGHT * FCVIDEO_SRC_WIDTH],
                     uint8_t stream[VRAM_BUF_BYTES_V2],
                     uint8_t attr[MBX_ATTR_LEN], bool reset_hysteresis) {
    memset(video->frame, 0, sizeof(video->frame));
    for (int y = 0; y < FCVIDEO_SRC_HEIGHT; y++) {
        uint8_t *dst = video->frame + (y + 16) * FCVIDEO_WIDTH;
        const uint8_t *src = source + y * FCVIDEO_SRC_WIDTH;
        for (int x = 0; x < FCVIDEO_WIDTH; x++) dst[x] = src[(x / 4) * 5 + (x & 3)];
    }
    if (reset_hysteresis) video->have_previous = false;
    choose_attributes(video, attr);
    memcpy(video->previous_attr, attr, MBX_ATTR_LEN);
    video->have_previous = true;

    memset(stream, 0, VRAM_BUF_BYTES_V2);
    for (int y = 0; y < VRAM_LINES; y++) {
        for (int tile = 0; tile < VRAM_LINE_WORDS; tile++) {
            uint8_t lo = 0, hi = 0;
            for (int bit = 0; bit < 8; bit++) {
                int flat = y * FCVIDEO_WIDTH + tile * 8 + bit;
                if (flat >= FCVIDEO_FRAME_BYTES) continue;
                int px = flat % FCVIDEO_WIDTH;
                int py = flat / FCVIDEO_WIDTH;
                uint8_t palette = attr_get(attr, px / 16, py / 16);
                uint8_t index = video->frame[flat];
                uint8_t position = (uint8_t)((px & 3) | ((py & 3) << 2));
                uint8_t pixel = video->tables.lut[(palette * 256 + index) * 16 + position] & 3u;
                lo |= (pixel & 1u) << (7 - bit);
                hi |= (pixel >> 1) << (7 - bit);
            }
            size_t offset = (size_t)(VRAM_HEAD_WORDS + y * VRAM_LINE_WORDS + tile) * 2;
            stream[offset] = lo;
            stream[offset + 1] = hi;
        }
    }

    uint8_t *mailbox = stream + VRAM_MAILBOX_OFF_V2;
    mailbox[MBX_FLAGS] = MBX_FLAG_V2 | MBX_FLAG_ATTR_VALID | MBX_FLAG_PAL_VALID;
    mailbox[MBX_MAGIC] = PF_MAGIC_NO;
    memcpy(mailbox + MBX_PAL, video->tables.palette, MBX_PAL_LEN);
    memcpy(mailbox + MBX_ATTR, attr, MBX_ATTR_LEN);
}

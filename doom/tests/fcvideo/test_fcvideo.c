/* SPDX-License-Identifier: BSD-3-Clause */
#include "fcvideo.h"
#include "ctest_lite.h"

#include <string.h>

static fcvideo_t video;
static uint8_t source[FCVIDEO_SRC_HEIGHT * FCVIDEO_SRC_WIDTH];
static uint8_t stream[VRAM_BUF_BYTES_V2];
static uint8_t attr[MBX_ATTR_LEN];
static uint8_t err[4 * 256];
static uint8_t lut[4 * 256 * 16];

static void test_solid_frame_and_linear_stride(void) {
    memset(source, 42, sizeof(source));
    memset(err, 100, sizeof(err));
    for (int p = 0; p < 4; p++) {
        err[p * 256] = p == 0 ? 0 : 20;
        err[p * 256 + 42] = p == 2 ? 0 : 100;
        for (int index = 0; index < 256; index++)
            for (int phase = 0; phase < 16; phase++)
                lut[(p * 256 + index) * 16 + phase] = (uint8_t)p;
    }
    fcvideo_tables_t tables = {.err = err, .lut = lut};
    for (int i = 0; i < MBX_PAL_LEN; i++) tables.palette[i] = (uint8_t)i;
    fcvideo_init(&video, &tables);
    CHECK_EQ(fcvideo_sizeof(), sizeof(video));
    fcvideo_convert(&video, source, stream, attr, false);

    CHECK_EQ(attr[0] & 3, 2);              /* First mixed margin/image block. */
    size_t first = (VRAM_HEAD_WORDS + FCVIDEO_TOP_MARGIN * VRAM_TILE_COLS) * 2;
    CHECK_EQ(stream[first], 0x00);
    CHECK_EQ(stream[first + 1], 0xff);
    size_t prior_last = (VRAM_HEAD_WORDS + (FCVIDEO_TOP_MARGIN - 1) * VRAM_TILE_COLS + 31) * 2;
    CHECK_EQ(first, prior_last + 2);
    CHECK_EQ(stream[prior_last], 0);
    size_t last = (VRAM_HEAD_WORDS + 231 * VRAM_TILE_COLS) * 2;
    CHECK_EQ(stream[last + 1], 0xff);
    size_t after = (VRAM_HEAD_WORDS + 232 * VRAM_TILE_COLS) * 2;
    CHECK_EQ(stream[after], 0);
    CHECK_EQ(stream[after + 1], 0);
    uint8_t *mailbox = stream + VRAM_MAILBOX_OFF_V2;
    CHECK_EQ(mailbox[MBX_MAGIC], PF_MAGIC_NO);
    CHECK_EQ(mailbox[MBX_FLAGS], MBX_FLAG_V2 | MBX_FLAG_ATTR_VALID | MBX_FLAG_PAL_VALID);
    CHECK_MEM(mailbox + MBX_ATTR, attr, MBX_ATTR_LEN);
    CHECK_MEM(mailbox + MBX_PAL, tables.palette, MBX_PAL_LEN);

    uint8_t first_stream[VRAM_BUF_BYTES_V2];
    memcpy(first_stream, stream, sizeof(stream));
    fcvideo_convert(&video, source, stream, attr, false);
    CHECK_MEM(stream, first_stream, sizeof(stream));

    /* Device scanlines must produce the same public NES stream without a
     * second 320x200 source buffer. */
    fcvideo_frame_begin(&video);
    for (int y = 0; y < FCVIDEO_SRC_HEIGHT; y++) {
        fcvideo_push_line(&video, y, source + y * FCVIDEO_SRC_WIDTH);
    }
    fcvideo_convert_staged(&video, stream, attr, false);
    CHECK_MEM(stream, first_stream, sizeof(stream));
}

static void test_vertical_source_rows(void) {
    fcvideo_frame_begin(&video);
    for (int y = 0; y < FCVIDEO_SRC_HEIGHT; y++) {
        memset(source + y * FCVIDEO_SRC_WIDTH, y, FCVIDEO_SRC_WIDTH);
        fcvideo_push_line(&video, y, source + y * FCVIDEO_SRC_WIDTH);
    }
    for (int row = 0; row < VRAM_LINES; row++) {
        uint8_t expected = row < FCVIDEO_TOP_MARGIN || row >= 232
                         ? 0 : (uint8_t)((row - FCVIDEO_TOP_MARGIN)
                             * FCVIDEO_SRC_HEIGHT / FCVIDEO_SCALED_HEIGHT);
        CHECK_EQ(video.frame[row * FCVIDEO_WIDTH], expected);
    }
}

int main(void) {
    test_solid_frame_and_linear_stride();
    test_vertical_source_rows();
    return ctest_lite_result();
}

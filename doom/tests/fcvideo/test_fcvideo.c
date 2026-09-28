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
    fcvideo_blank_status(&video);
    CHECK_EQ(video.frame[(FCVIDEO_STATUS_START - 1) * FCVIDEO_WIDTH],
             (uint8_t)((FCVIDEO_STATUS_START - 1 - FCVIDEO_TOP_MARGIN)
             * FCVIDEO_SRC_HEIGHT / FCVIDEO_SCALED_HEIGHT));
    for (int row = FCVIDEO_STATUS_START; row < VRAM_LINES; row++) {
        CHECK_EQ(video.frame[row * FCVIDEO_WIDTH], 97);
        CHECK_EQ(video.frame[(row + 1) * FCVIDEO_WIDTH - 1], 97);
    }
}

static void test_native_status_gray_palette(void) {
    fcvideo_frame_begin(&video);
    fcvideo_blank_status(&video);
    fcvideo_set_native_status(&video, true);
    fcvideo_convert_staged(&video, stream, attr, true);
    for (int by = 12; by < 15; ++by) {
        for (int bx = 0; bx < 16; ++bx) {
            int index = (bx >> 1) + ((by >> 1) << 3);
            int shift = ((bx & 1) + ((by & 1) << 1)) * 2;
            CHECK_EQ((attr[index] >> shift) & 3, 3);
        }
    }
    size_t first = (VRAM_HEAD_WORDS + FCVIDEO_STATUS_START * VRAM_TILE_COLS) * 2;
    CHECK_EQ(stream[first], 0);
    CHECK_EQ(stream[first + 1], 0);
    fcvideo_set_native_status(&video, false);
}

static void test_native_text_compaction(void) {
    static uint8_t compact[VRAM_BUF_BYTES_V4];
    for (size_t word = 0; word < PPU_PICTURE_COUNT / 2; word++) {
        stream[word * 2] = (uint8_t)word;
        stream[word * 2 + 1] = (uint8_t)(word >> 8);
    }
    memcpy(compact, stream, PPU_PICTURE_COUNT);
    fcvideo_compact_native_text(compact);
    size_t output_word = 0;
    for (size_t input_word = 0; input_word < PPU_PICTURE_COUNT / 2; input_word++) {
        size_t visible = input_word - VRAM_HEAD_WORDS;
        bool text = input_word >= VRAM_HEAD_WORDS &&
            visible / VRAM_TILE_COLS >= NATIVE_TEXT_ROW * 8 &&
            visible / VRAM_TILE_COLS < (NATIVE_TEXT_ROW + 1) * 8 &&
            visible % VRAM_TILE_COLS >= NATIVE_TEXT_COL &&
            visible % VRAM_TILE_COLS < NATIVE_TEXT_COL + NATIVE_TEXT_TILES;
        if (text) continue;
        CHECK_EQ(compact[output_word * 2], (uint8_t)input_word);
        CHECK_EQ(compact[output_word * 2 + 1], (uint8_t)(input_word >> 8));
        output_word++;
    }
    CHECK_EQ(output_word * 2, PPU_PICTURE_COUNT_V4);
}

int main(void) {
    test_solid_frame_and_linear_stride();
    test_vertical_source_rows();
    test_native_status_gray_palette();
    test_native_text_compaction();
    return ctest_lite_result();
}

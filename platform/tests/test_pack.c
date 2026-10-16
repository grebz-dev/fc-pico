/* SPDX-License-Identifier: BSD-3-Clause */
/* Stream pixel encoding: bit planes, ordering and placement. */
#include "ctest_lite.h"
#include "fcbus_core.h"
#include "fcbus_pack.h"

static uint16_t stream[VRAM_BUF_BYTES / 2];

int main(void) {
    uint8_t line[256] = {0}, back[256];
    line[0] = 1; /* plane 0, leftmost pixel: bit 7 of the low byte */
    line[1] = 2; /* plane 1, second pixel: bit 6 of the high byte */
    line[7] = 3;
    line[255] = 3;
    fcbus_stream_put_line(stream, 5, line);
    CHECK_EQ(stream[VRAM_HEAD_WORDS + 5 * VRAM_TILE_COLS], 0x4000 | 0x0100 | 0x0080 | 0x0001);
    CHECK_EQ(stream[VRAM_HEAD_WORDS + 5 * VRAM_TILE_COLS + 31], 0x0101);
    CHECK_EQ(stream[VRAM_HEAD_WORDS + 4 * VRAM_TILE_COLS], 0);

    for (int x = 0; x < 256; x++) line[x] = (uint8_t)((x * 7 + x / 13) & 3);
    fcbus_stream_put_line(stream, 239, line);
    fcbus_stream_get_line(stream, 239, back);
    CHECK_MEM(back, line, sizeof line);
    return ctest_lite_result();
}

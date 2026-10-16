/**
 * @file fcbus_layout.c
 * @brief Stream and mailbox geometry of the compiled bus core, for tools.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcbus_layout.h"

#include "fcbus_core.h"

static const fcbus_layout_t layout = {
    .version = FCBUS_LAYOUT_VERSION,
    .expected_read_count = PPU_COUNT_VAL,
    .picture_reads = PPU_PICTURE_COUNT,
    .count_report_bias = PPU_COUNTER_REPORT_BIAS,
    .count_window = PPU_COUNT_WINDOW,
    .buffer_bytes = sizeof(((fcbus_core_t *)0)->stream[0]),
    .head_words = VRAM_HEAD_WORDS,
    .words_per_line = VRAM_TILE_COLS,
    .lines = VRAM_LINES,
    .mailbox_offset = VRAM_MAILBOX_OFF,
    .mailbox_bytes = sizeof(((fcbus_core_t *)0)->mailbox),
    .mailbox_magic_index = MBX_MAGIC,
    .mailbox_magic = PF_MAGIC_NO,
    .command_offset = MBX_CMD,
    .command_end = FC_COM_BUF_SIZE16,
    .apu_offset = MBX_APU,
    .apu_pairs_max = APU_PAIRS_MAX,
};

const fcbus_layout_t *fcbus_layout(void) {
    return &layout;
}

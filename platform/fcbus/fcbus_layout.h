/**
 * @file fcbus_layout.h
 * @brief Stream and mailbox geometry of the compiled bus core, for tools.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Tools that inspect streams or mailboxes ask the library for its geometry instead
 * of hard-coding offsets. Field names match `platform/fcbus/gen/fcbus_layout.json`;
 * `version` is #FCBUS_LAYOUT_VERSION, and readers must reject versions they do not
 * know.
 */
#ifndef FCBUS_LAYOUT_H
#define FCBUS_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t version;
    uint32_t expected_read_count;
    uint32_t picture_reads;
    uint32_t count_report_bias;
    uint32_t count_window;
    uint32_t buffer_bytes;
    uint32_t head_words;
    uint32_t words_per_line;
    uint32_t lines;
    uint32_t mailbox_offset;
    uint32_t mailbox_bytes;
    uint32_t mailbox_magic_index;
    uint32_t mailbox_magic;
    uint32_t command_offset;
    uint32_t command_end;
    uint32_t apu_offset;
    uint32_t apu_pairs_max;
} fcbus_layout_t;

/** The layout this build of the core uses. */
const fcbus_layout_t *fcbus_layout(void);

#ifdef __cplusplus
}
#endif

#endif /* FCBUS_LAYOUT_H */

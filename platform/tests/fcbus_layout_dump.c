/* SPDX-License-Identifier: BSD-3-Clause */
/* Prints fcbus_layout() as JSON, for comparison with the generated descriptor. */
#include <stdio.h>

#include "fcbus_layout.h"

int main(void) {
    const fcbus_layout_t *l = fcbus_layout();
    printf("{\"layout_version\": %u, \"layout\": {", (unsigned)l->version);
    printf("\"expected_read_count\": %u, ", (unsigned)l->expected_read_count);
    printf("\"picture_reads\": %u, ", (unsigned)l->picture_reads);
    printf("\"count_report_bias\": %u, ", (unsigned)l->count_report_bias);
    printf("\"count_window\": %u, ", (unsigned)l->count_window);
    printf("\"buffer_bytes\": %u, ", (unsigned)l->buffer_bytes);
    printf("\"head_words\": %u, ", (unsigned)l->head_words);
    printf("\"words_per_line\": %u, ", (unsigned)l->words_per_line);
    printf("\"lines\": %u, ", (unsigned)l->lines);
    printf("\"mailbox_offset\": %u, ", (unsigned)l->mailbox_offset);
    printf("\"mailbox_bytes\": %u, ", (unsigned)l->mailbox_bytes);
    printf("\"mailbox_magic_index\": %u, ", (unsigned)l->mailbox_magic_index);
    printf("\"mailbox_magic\": %u, ", (unsigned)l->mailbox_magic);
    printf("\"command_offset\": %u, ", (unsigned)l->command_offset);
    printf("\"command_end\": %u, ", (unsigned)l->command_end);
    printf("\"apu_offset\": %u, ", (unsigned)l->apu_offset);
    printf("\"apu_pairs_max\": %u}}\n", (unsigned)l->apu_pairs_max);
    return 0;
}

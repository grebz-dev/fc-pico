/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef FCPICO_TRACE_H
#define FCPICO_TRACE_H

#include <stdbool.h>
#include <stdint.h>

#define FCPICO_TRACE_WORDS 65536u
#define FCPICO_TRACE_SAMPLES_PER_WORD 6u
#define FCPICO_TRACE_PERIOD_NS 40u

bool trace_init(void);
bool trace_capture(void);
void trace_dump(void);

#endif

/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef FCVIDEO_PRESETS_H
#define FCVIDEO_PRESETS_H

#include <stdint.h>

typedef struct {
    uint8_t backdrop;
    uint8_t subpalettes[4][3];
} fcvideo_preset_t;

enum {
    FCVIDEO_PRESET_HUE = 0,
    FCVIDEO_PRESET_SHARED_WHITE = 1,
    FCVIDEO_PRESET_PIPU = 2,
    FCVIDEO_PRESET_COUNT = 3,
};

/* Values are 2C02 palette indices. The PiPU set was measured on a TV;
 * see plan/04-video.md for its origin and the A/B trade-off. */
extern const fcvideo_preset_t fcvideo_presets[FCVIDEO_PRESET_COUNT];

#ifndef FCVIDEO_DEFAULT_PRESET
#define FCVIDEO_DEFAULT_PRESET FCVIDEO_PRESET_SHARED_WHITE
#endif

#endif

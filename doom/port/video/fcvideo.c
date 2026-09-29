/* SPDX-License-Identifier: BSD-3-Clause */
#include "fcvideo.h"
#include "native_status_font.h"

#include <math.h>
#include <string.h>

const fcvideo_preset_t fcvideo_presets[FCVIDEO_PRESET_COUNT] = {
    {0x0f, {{0x00, 0x10, 0x30}, {0x07, 0x17, 0x27},
            {0x06, 0x16, 0x26}, {0x09, 0x19, 0x29}}, 0},
    {0x0f, {{0x00, 0x10, 0x30}, {0x07, 0x17, 0x30},
            {0x06, 0x16, 0x30}, {0x09, 0x19, 0x30}}, 0},
    {0x0f, {{0x10, 0x09, 0x2d}, {0x07, 0x28, 0x18},
            {0x02, 0x01, 0x11}, {0x06, 0x16, 0x3d}}, 0},
    {0x0f, {{0x1d, 0x00, 0x30}, {0x08, 0x18, 0x30},
            {0x06, 0x16, 0x30}, {0x09, 0x19, 0x30}}, 1},
};

/* Same NESDev example 2C02 RGB palette as tools/fcpico/nes_palette.py. */
static const uint8_t nes_rgb[64][3] = {
    {128,128,128}, {0,61,166}, {0,18,176}, {68,0,150},
    {161,0,94}, {199,0,40}, {186,6,0}, {140,23,0},
    {92,47,0}, {16,69,0}, {5,74,0}, {0,71,46},
    {0,65,102}, {0,0,0}, {5,5,5}, {5,5,5},
    {199,199,199}, {0,119,255}, {33,85,255}, {130,55,250},
    {235,47,181}, {255,41,80}, {255,34,0}, {214,50,0},
    {196,98,0}, {53,128,0}, {5,143,0}, {0,138,85},
    {0,153,204}, {33,33,33}, {9,9,9}, {9,9,9},
    {255,255,255}, {15,215,255}, {105,162,255}, {212,128,255},
    {255,69,243}, {255,97,139}, {255,136,51}, {255,156,18},
    {250,188,32}, {159,227,14}, {43,240,53}, {12,240,164},
    {5,251,255}, {94,94,94}, {13,13,13}, {13,13,13},
    {255,255,255}, {166,252,255}, {179,236,255}, {218,171,235},
    {255,168,249}, {255,171,179}, {255,210,176}, {255,239,166},
    {255,247,156}, {215,232,149}, {166,237,175}, {162,242,218},
    {153,255,252}, {221,221,221}, {17,17,17}, {17,17,17},
};

static const uint8_t bayer[16] = {
    0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5,
};

void fcvideo_build_tables(const uint8_t playpal_rgb[256 * 3],
                          const fcvideo_preset_t *preset,
                          uint8_t err[FCVIDEO_ERR_BYTES],
                          uint8_t lut[FCVIDEO_LUT_BYTES],
                          uint8_t palette[MBX_PAL_LEN]) {
    for (int p = 0; p < 4; p++) {
        palette[p * 4] = preset->backdrop;
        for (int i = 0; i < 3; i++)
            palette[p * 4 + i + 1] = preset->subpalettes[p][i];

        for (int index = 0; index < 256; index++) {
            int target[3];
            for (int channel = 0; channel < 3; channel++) {
                int source = playpal_rgb[index * 3 + channel];
                target[channel] = source + (preset->shadow_lift
                    ? (source * (255 - source) + 384) / 768 : 0);
            }
            double best_dist2 = INFINITY;
            int best_a = 0, best_b = 0, best_ratio = 0;
            /* Loop order is the Python oracle's flattened (a,b,r) order;
             * a tie therefore keeps its first candidate. */
            for (int a = 0; a < 4; a++) {
                for (int b = 0; b < 4; b++) {
                    for (int ratio = 0; ratio <= 16; ratio++) {
                        double dist2 = 0;
                        for (int channel = 0; channel < 3; channel++) {
                            double mixed = (nes_rgb[palette[p * 4 + a]][channel] * (16 - ratio)
                                            + nes_rgb[palette[p * 4 + b]][channel] * ratio) / 16.0;
                            double diff = mixed - target[channel];
                            dist2 += diff * diff;
                        }
                        if (dist2 < best_dist2) {
                            best_dist2 = dist2;
                            best_a = a;
                            best_b = b;
                            best_ratio = ratio;
                        }
                    }
                }
            }
            err[p * 256 + index] = (uint8_t)fmin(255.0, nearbyint(sqrt(best_dist2 / 3.0)));
            for (int phase = 0; phase < 16; phase++)
                lut[(p * 256 + index) * 16 + phase] =
                    (uint8_t)(bayer[phase] < best_ratio ? best_b : best_a);
        }
    }
}

void fcvideo_build_palette_sets(const fcvideo_preset_t *preset,
                                uint8_t sets[FCVIDEO_PALETTE_SET_COUNT][MBX_PAL_LEN]) {
    for (int p = 0; p < 4; p++) {
        sets[0][p * 4] = preset->backdrop;
        for (int i = 0; i < 3; i++)
            sets[0][p * 4 + i + 1] = preset->subpalettes[p][i];
    }
    for (int set = 1; set < FCVIDEO_PALETTE_SET_COUNT; set++) {
        int mul, target[3];
        if (set < 9) {
            mul = set * 65536 / 9;
            target[0] = 255; target[1] = 0; target[2] = 0;
        } else if (set < 13) {
            mul = (set - 8) * 65536 / 8;
            target[0] = 215; target[1] = 186; target[2] = 69;
        } else {
            mul = 65536 / 8;
            target[0] = 0; target[1] = 256; target[2] = 0;
        }
        for (int i = 0; i < MBX_PAL_LEN; i++) {
            const uint8_t *anchor = nes_rgb[sets[0][i]];
            int tinted[3];
            for (int c = 0; c < 3; c++)
                tinted[c] = anchor[c] + (((target[c] - anchor[c]) * mul) >> 16);
            uint32_t best_dist2 = UINT32_MAX;
            uint8_t best_index = 0;
            for (uint8_t candidate = 0; candidate < 64; candidate++) {
                uint32_t dist2 = 0;
                for (int c = 0; c < 3; c++) {
                    int diff = (int)nes_rgb[candidate][c] - tinted[c];
                    dist2 += (uint32_t)(diff * diff);
                }
                if (dist2 < best_dist2) {
                    best_dist2 = dist2;
                    best_index = candidate;
                }
            }
            sets[set][i] = best_index;
        }
    }
}

void fcvideo_init(fcvideo_t *video, const fcvideo_tables_t *tables) {
    memset(video, 0, sizeof(*video));
    video->tables = *tables;
}

size_t fcvideo_sizeof(void) { return sizeof(fcvideo_t); }

void fcvideo_set_palette(fcvideo_t *video, const uint8_t palette[MBX_PAL_LEN]) {
    memcpy(video->tables.palette, palette, MBX_PAL_LEN);
}

void fcvideo_set_native_status(fcvideo_t *video, bool enabled) {
    video->native_status = enabled;
}

void fcvideo_set_status_snapshot(fcvideo_t *video, const fcui_status_t *status) {
    video->status = *status;
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
            if (video->native_status && by >= FCVIDEO_STATUS_START / 16) {
                attr_set(attr, bx, by, 3);
                continue;
            }
            uint32_t costs[4] = {0};
            for (int y = by * 16; y < by * 16 + 16; y++) {
                for (int x = bx * 16; x < bx * 16 + 16; x++) {
                    uint8_t index = video->frame[y * FCVIDEO_WIDTH + x];
                    for (int p = 0; p < 4; p++) costs[p] += video->tables.err[p * 256 + index];
                }
            }
            uint8_t best = 0;
            uint8_t choices = video->native_status ? 3 : 4;
            for (uint8_t p = 1; p < choices; p++) {
                if (costs[p] < costs[best]) best = p;
            }
            if (video->have_previous) {
                uint8_t old = attr_get(video->previous_attr, bx, by);
                /* Python reference: retain old unless best is strictly more
                 * than 12% cheaper. Integer comparison avoids floating point. */
                if (old < choices && best != old &&
                    costs[best] * 100u >= costs[old] * 88u) best = old;
            }
            attr_set(attr, bx, by, best);
        }
    }
}

void fcvideo_frame_begin(fcvideo_t *video) {
    memset(video->frame, 0, sizeof(video->frame));
}

void fcvideo_push_line(fcvideo_t *video, int y,
                       const uint8_t line[FCVIDEO_SRC_WIDTH]) {
    if (y < 0 || y >= FCVIDEO_SRC_HEIGHT) return;
    /* Output row d samples source floor(d * 200 / 224). Each source row
     * therefore fills one or two destination rows. */
    int first = (y * FCVIDEO_SCALED_HEIGHT + FCVIDEO_SRC_HEIGHT - 1)
              / FCVIDEO_SRC_HEIGHT;
    int end = ((y + 1) * FCVIDEO_SCALED_HEIGHT + FCVIDEO_SRC_HEIGHT - 1)
            / FCVIDEO_SRC_HEIGHT;
    uint8_t *dst = video->frame + (FCVIDEO_TOP_MARGIN + first) * FCVIDEO_WIDTH;
    for (int x = 0; x < FCVIDEO_WIDTH; x++) dst[x] = line[(x / 4) * 5 + (x & 3)];
    for (int row = first + 1; row < end; row++) {
        memcpy(video->frame + (FCVIDEO_TOP_MARGIN + row) * FCVIDEO_WIDTH,
               dst, FCVIDEO_WIDTH);
    }
}

static void panel_glyph(fcvideo_t *video, int x, int y, char c,
                        int advance, int height_scale, uint8_t color) {
    const uint8_t *rows = native_font_rows(c);
    if (!rows) return;
    for (int row = 0; row < 7; ++row) {
        for (int repeat = 0; repeat < height_scale; ++repeat) {
            int py = y + row * height_scale + repeat;
            if (py < FCVIDEO_STATUS_START || py >= VRAM_LINES ||
                (py >= 224 && py < 232 && x >= 16 && x < 240)) continue;
            for (int col = 0; col < 5; ++col) {
                int px = x + col;
                if (px >= 0 && px < FCVIDEO_WIDTH &&
                    (rows[row] & (1u << (4 - col)))) {
                    video->frame[py * FCVIDEO_WIDTH + px] = color;
                }
            }
        }
    }
    (void)advance;
}

static void panel_text(fcvideo_t *video, int x, int y, const char *text,
                       int advance, int height_scale, uint8_t color) {
    while (*text) {
        panel_glyph(video, x, y, *text++, advance, height_scale, color);
        x += advance;
    }
}

static int panel_decimal(char out[4], uint16_t value) {
    if (value > 999) value = 999;
    int count = 0;
    if (value >= 100) out[count++] = (char)('0' + value / 100);
    if (value >= 10) out[count++] = (char)('0' + value / 10 % 10);
    out[count++] = (char)('0' + value % 10);
    out[count] = 0;
    return count;
}

static void panel_inventory(fcvideo_t *video, int y, const char *name,
                            uint16_t current, uint16_t maximum) {
    char left[4], right[4], count[9];
    int used = panel_decimal(left, current);
    int max_used = panel_decimal(right, maximum);
    int pos = 0;
    for (int i = 0; i < used; ++i) count[pos++] = left[i];
    count[pos++] = '/';
    for (int i = 0; i < max_used; ++i) count[pos++] = right[i];
    count[pos] = 0;
    panel_text(video, 194, y, name, 5, 1, 2);
    panel_text(video, 254 - pos * 5, y, count, 5, 1, 2);
}

void fcvideo_blank_status(fcvideo_t *video) {
    if (!video->native_status) {
        memset(video->frame + FCVIDEO_STATUS_START * FCVIDEO_WIDTH, 97,
               (VRAM_LINES - FCVIDEO_STATUS_START) * FCVIDEO_WIDTH);
        return;
    }
    for (int y = FCVIDEO_STATUS_START; y < VRAM_LINES; ++y) {
        for (int x = 0; x < FCVIDEO_WIDTH; ++x) {
            unsigned grain = ((unsigned)(x >> 2) * 13u ^
                              (unsigned)(y >> 1) * 7u ^
                              (unsigned)(x * y) >> 5) & 63u;
            uint8_t color = grain == 0 ? 0 : grain == 1 ? 2 : 1;
            if (x == 0 || x == 255 || y == 192 || y == 239) color = 0;
            if (y == 193 || y == 238) color = 2;
            if (x == 31 || x == 79 || x == 109 || x == 146 ||
                x == 182 || x == 191) color = 0;
            if (x >= 110 && x <= 145 && y >= 196 && y <= 237) color = 0;
            video->frame[y * FCVIDEO_WIDTH + x] = color;
        }
    }
    char number[4];
    unsigned weapon = video->status.ready_weapon & 15u;
    int ammo_type = -1;
    if (weapon == 1 || weapon == 3) ammo_type = 0;
    if (weapon == 2 || weapon == 8) ammo_type = 1;
    if (weapon == 4) ammo_type = 3;
    if (weapon == 5 || weapon == 6) ammo_type = 2;
    if (ammo_type >= 0) {
        panel_decimal(number, video->status.ammo[ammo_type]);
        panel_text(video, 4, 198, number, 8, 2, 3);
    }
    panel_decimal(number, video->status.health);
    panel_text(video, 36, 198, number, 8, 2, 3);
    panel_text(video, 36 + (int)strlen(number) * 8, 198, "%", 8, 2, 3);
    panel_decimal(number, video->status.armor);
    panel_text(video, 149, 198, number, 8, 2, 3);
    panel_text(video, 149 + (int)strlen(number) * 8, 198, "%", 8, 2, 3);
    panel_text(video, 4, 232, "AMMO", 6, 1, 2);
    panel_text(video, 36, 232, "HEALTH", 6, 1, 2);
    panel_text(video, 81, 232, "ARMS", 6, 1, 2);
    panel_text(video, 149, 232, "ARMOR", 6, 1, 2);
    panel_inventory(video, 196, "BULL", video->status.ammo[0], video->status.maxammo[0]);
    panel_inventory(video, 203, "SHEL", video->status.ammo[1], video->status.maxammo[1]);
    panel_inventory(video, 210, "RCKT", video->status.ammo[3], video->status.maxammo[3]);
    panel_inventory(video, 217, "CELL", video->status.ammo[2], video->status.maxammo[2]);
}

void fcvideo_convert_staged(fcvideo_t *video,
                            uint8_t stream[VRAM_BUF_BYTES_V2],
                            uint8_t attr[MBX_ATTR_LEN], bool reset_hysteresis) {
    if (reset_hysteresis) video->have_previous = false;
    choose_attributes(video, attr);
    memcpy(video->previous_attr, attr, MBX_ATTR_LEN);
    video->have_previous = true;

    memset(stream, 0, VRAM_BUF_BYTES_V2);
    for (int y = 0; y < VRAM_LINES; y++) {
        for (int tile = 0; tile < VRAM_TILE_COLS; tile++) {
            uint8_t lo = 0, hi = 0;
            for (int bit = 0; bit < 8; bit++) {
                int flat = y * FCVIDEO_WIDTH + tile * 8 + bit;
                if (flat >= FCVIDEO_FRAME_BYTES) continue;
                int px = flat % FCVIDEO_WIDTH;
                int py = flat / FCVIDEO_WIDTH;
                if (py < FCVIDEO_TOP_MARGIN ||
                    py >= FCVIDEO_TOP_MARGIN + FCVIDEO_SCALED_HEIGHT) continue;
                uint8_t palette = attr_get(attr, px / 16, py / 16);
                uint8_t index = video->frame[flat];
                uint8_t position = (uint8_t)((px & 3) | ((py & 3) << 2));
                uint8_t pixel = video->native_status && py >= FCVIDEO_STATUS_START
                    ? video->frame[flat]
                    : video->tables.lut[(palette * 256 + index) * 16 + position] & 3u;
                lo |= (pixel & 1u) << (7 - bit);
                hi |= (pixel >> 1) << (7 - bit);
            }
            size_t offset = (size_t)(VRAM_HEAD_WORDS + y * VRAM_TILE_COLS + tile) * 2;
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

void fcvideo_compact_native_text(uint8_t stream[VRAM_BUF_BYTES_V4]) {
    size_t write_word = 0;
    const size_t picture_words = PPU_PICTURE_COUNT / 2;
    for (size_t read_word = 0; read_word < picture_words; read_word++) {
        if (read_word >= VRAM_HEAD_WORDS) {
            size_t visible = read_word - VRAM_HEAD_WORDS;
            size_t row = visible / VRAM_TILE_COLS;
            size_t col = visible % VRAM_TILE_COLS;
            if (row >= NATIVE_TEXT_ROW * 8 && row < (NATIVE_TEXT_ROW + 1) * 8 &&
                col >= NATIVE_TEXT_COL && col < NATIVE_TEXT_COL + NATIVE_TEXT_TILES) {
                continue;
            }
        }
        stream[write_word * 2] = stream[read_word * 2];
        stream[write_word * 2 + 1] = stream[read_word * 2 + 1];
        write_word++;
    }
    /* The mailbox starts here and is written by fcbus_core_heartbeat(). */
}

void fcvideo_convert(fcvideo_t *video,
                     const uint8_t source[FCVIDEO_SRC_HEIGHT * FCVIDEO_SRC_WIDTH],
                     uint8_t stream[VRAM_BUF_BYTES_V2],
                     uint8_t attr[MBX_ATTR_LEN], bool reset_hysteresis) {
    fcvideo_frame_begin(video);
    for (int y = 0; y < FCVIDEO_SRC_HEIGHT; y++) {
        fcvideo_push_line(video, y, source + y * FCVIDEO_SRC_WIDTH);
    }
    fcvideo_convert_staged(video, stream, attr, reset_hysteresis);
}

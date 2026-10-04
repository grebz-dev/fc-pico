/*
 * test_fcworld.c -- world sprite raster, tile/OAM frames and the v5 scheduler.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The console model below mirrors build_world_sprite_rom.py: a pending OAM DMA
 * leaves world packets unapplied, duplicates are ignored, OAM commits are
 * published by the main loop and become visible at the next DMA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ctest_lite.h"
#include "fcbus_core.h"
#include "fcworld.h"

static const uint8_t world_pal[6] = {0x08, 0x00, 0x26, 0x09, 0x0F, 0x2A};
static const uint8_t menu_pal[6] = FCWORLD_MENU_PALETTES_INIT;

/* ---------------------------------------------------------------- console */
typedef struct {
    uint8_t chr[256][16];
    uint8_t shadow[256];   /* $0200 */
    uint8_t oam[256];      /* PPU OAM after DMA */
    uint8_t stage[128];    /* $0400 */
    uint8_t ack, shown, slots, commit, commit_gen, pending;
    uint8_t pal[6];
    unsigned writes_to_shown;   /* CHR writes into tiles the displayed OAM uses */
} console_t;

static void console_init(console_t *c) {
    memset(c, 0, sizeof *c);
    for (int n = 0; n < 64; ++n) c->shadow[n * 4] = c->oam[n * 4] = 0xEF;
    memcpy(c->pal, menu_pal, 6);
}

static bool displayed_tile(const console_t *c, uint8_t tile) {
    for (int n = WORLD_OAM_FIRST; n < 64; ++n) {
        if (c->oam[n * 4] < 0xEF && c->oam[n * 4 + 1] == tile) return true;
    }
    return false;
}

static uint8_t console_nmi(console_t *c, const uint8_t *m) {
    if (c->pending) {
        memcpy(c->oam, c->shadow, 256);
        c->shown = c->slots;
        c->pending = 0;
    } else if ((m[MBX_FLAGS] & MBX_FLAG_WORLD) && m[MBX_WORLD_SEQ] != c->ack) {
        if (m[MBX_WORLD_OP] == WORLD_OP_CHR) {
            for (int t = 0; t < WORLD_CHR_TILES; ++t) {
                uint16_t address = (uint16_t)(m[MBX_WORLD_ARG + t * 2] << 8 |
                                              m[MBX_WORLD_ARG + t * 2 + 1]);
                CHECK(address >= 0x1000 && address < 0x2000 && (address & 15) == 0);
                uint8_t tile = (uint8_t)((address - 0x1000) >> 4);
                if (displayed_tile(c, tile) &&
                    memcmp(c->chr[tile], m + MBX_WORLD_PAYLOAD + t * 16, 16) != 0) {
                    c->writes_to_shown++;
                }
                memcpy(c->chr[tile], m + MBX_WORLD_PAYLOAD + t * 16, 16);
            }
        } else if (m[MBX_WORLD_OP] == WORLD_OP_OAM) {
            uint8_t arg = m[MBX_WORLD_ARG];
            if ((arg & 0x7F) != WORLD_OAM_NONE) {
                memcpy(c->stage + (arg & 0x7F) * 4, m + MBX_WORLD_PAYLOAD, 64);
            }
            if (arg & WORLD_OAM_COMMIT) {
                memcpy(c->pal, m + MBX_WORLD_PAL, 6);
                c->commit_gen = m[MBX_WORLD_GEN];
                c->commit = 1;
            }
        }
        c->ack = m[MBX_WORLD_SEQ];
    }
    return (uint8_t)(c->shown << WORLD_HB_GEN_SHIFT | c->ack);
}

static void console_main(console_t *c, bool ui_dma) {
    if (c->commit) {
        c->pending = 0;
        c->commit = 0;
        for (int i = 0; i < WORLD_OAM_ENTRIES * 4; ++i) {
            c->shadow[WORLD_OAM_FIRST * 4 + i] = c->commit_gen ? c->stage[i] :
                (i % 4 == 0 ? 0xEF : c->shadow[WORLD_OAM_FIRST * 4 + i]);
        }
        c->slots = c->commit_gen;
        c->pending = 1;
    }
    if (ui_dma) c->pending = 1;
}

/* ------------------------------------------------------------------ frames */
static void make_tile(uint8_t tile[16], unsigned seed) {
    for (int i = 0; i < 16; ++i) tile[i] = (uint8_t)(seed * 37u + (unsigned)i * 11u + 1u);
}

static void make_frame(fcworld_frame_t *f, unsigned tiles, unsigned seed) {
    fcworld_frame_clear(f, world_pal);
    for (unsigned t = 0; t < tiles; ++t) {
        make_tile(f->tiles[t], seed + t);
        uint8_t *e = f->oam[f->oam_count++];
        e[0] = (uint8_t)(40 + (t / 4) * 8 - 1);
        e[1] = (uint8_t)t;
        e[2] = 1;
        e[3] = (uint8_t)(64 + (t % 4) * 8);
    }
    f->tile_count = (uint8_t)tiles;
}

/* Expected displayed pixels of entry n: tile data presented with flips. */
static void check_displayed(const console_t *c, const fcworld_frame_t *f) {
    int visible = 0;
    for (int n = 0; n < WORLD_OAM_ENTRIES; ++n) {
        const uint8_t *o = c->oam + (WORLD_OAM_FIRST + n) * 4;
        if (n >= f->oam_count) { CHECK_EQ(o[0], 0xEF); continue; }
        const uint8_t *e = f->oam[n];
        visible++;
        CHECK_EQ(o[0], e[0]);
        CHECK_EQ(o[3], e[3]);
        CHECK_EQ(o[2] & 3, e[2] & 3);
        const uint8_t *stored = c->chr[o[1]];
        const uint8_t *wanted = f->tiles[e[1]];
        bool h = (o[2] ^ e[2]) & 0x40, v = (o[2] ^ e[2]) & 0x80;
        for (int plane = 0; plane < 2; ++plane) {
            for (int row = 0; row < 8; ++row) {
                uint8_t s = stored[plane * 8 + (v ? 7 - row : row)];
                if (h) {
                    uint8_t r = 0;
                    for (int b = 0; b < 8; ++b) r = (uint8_t)(r | ((s >> b) & 1) << (7 - b));
                    s = r;
                }
                CHECK_EQ(s, wanted[plane * 8 + row]);
            }
        }
    }
    CHECK_EQ(visible, f->oam_count);
}

/* Run heartbeats until the scheduler is idle; DMA frames interleave. */
static unsigned drive(fcworld_t *w, console_t *c, unsigned seed) {
    unsigned beats = 0;
    uint8_t mailbox[FC_COM_BUF_SIZE_V5];
    uint8_t pad2 = (uint8_t)(c->shown << WORLD_HB_GEN_SHIFT | c->ack);
    srand(seed);
    do {
        memset(mailbox, 0, sizeof mailbox);
        mailbox[MBX_MAGIC] = PF_MAGIC_NO;
        fcworld_heartbeat(w, mailbox, pad2);
        pad2 = console_nmi(c, mailbox);
        console_main(c, rand() % 5 == 0);
        CHECK(++beats < 400);
    } while (!fcworld_idle(w) && beats < 400);
    /* One more heartbeat lets the shown generation settle. */
    memset(mailbox, 0, sizeof mailbox);
    fcworld_heartbeat(w, mailbox, pad2);
    return beats;
}

static void test_scheduler_uploads_commits_and_protects(void) {
    fcworld_t w;
    console_t c;
    fcworld_reset(&w);
    console_init(&c);
    fcworld_frame_t a, b;
    make_frame(&a, 20, 1);
    uint8_t gen = fcworld_submit(&w, &a);
    CHECK(gen >= 1 && gen <= 15);
    CHECK_EQ(fcworld_submit(&w, &a), 0xFF); /* busy */
    unsigned beats = drive(&w, &c, 7);
    CHECK(beats >= 5 + 2);                   /* 5 CHR packets, 2 OAM halves */
    CHECK_EQ(c.shown, gen);
    check_displayed(&c, &a);
    CHECK_EQ(w.stats.chr_tiles, 20u);

    /* Unchanged frame: no generation, no packets. */
    uint32_t packets = w.stats.packets;
    CHECK_EQ(fcworld_submit(&w, &a), gen);
    CHECK(fcworld_idle(&w));
    CHECK_EQ(w.stats.packets, packets);

    /* Half the tiles reused, half new: displayed tiles are never rewritten. */
    make_frame(&b, 20, 11);
    uint8_t gen2 = fcworld_submit(&w, &b);
    CHECK(gen2 != gen && gen2 != 0);
    drive(&w, &c, 9);
    CHECK_EQ(c.shown, gen2);
    check_displayed(&c, &b);
    CHECK_EQ(c.writes_to_shown, 0u);
    CHECK(w.stats.cache_hits >= 10);
    CHECK(w.stats.resends > 0 || w.stats.packets > 0);

    /* Clearing for a menu: first world palettes, then menu palettes. */
    fcworld_frame_t empty;
    fcworld_frame_clear(&empty, world_pal);
    CHECK_EQ(fcworld_submit(&w, &empty), 0);
    drive(&w, &c, 3);
    CHECK_EQ(c.shown, 0);
    check_displayed(&c, &empty);
    CHECK(fcworld_quiescent(&w, world_pal));
    CHECK(!fcworld_quiescent(&w, menu_pal));
    fcworld_frame_clear(&empty, menu_pal);
    CHECK_EQ(fcworld_submit(&w, &empty), 0);
    drive(&w, &c, 4);
    CHECK_MEM(c.pal, menu_pal, 6);
    CHECK(fcworld_quiescent(&w, menu_pal));
}

static void test_flipped_cache_reuse(void) {
    fcworld_t w;
    console_t c;
    fcworld_reset(&w);
    console_init(&c);
    fcworld_frame_t a;
    make_frame(&a, 1, 5);
    fcworld_submit(&w, &a);
    drive(&w, &c, 1);
    /* The same tile mirrored horizontally needs no upload. */
    fcworld_frame_t b = a;
    for (int i = 0; i < 16; ++i) {
        uint8_t s = a.tiles[0][i], r = 0;
        for (int bit = 0; bit < 8; ++bit) r = (uint8_t)(r | ((s >> bit) & 1) << (7 - bit));
        b.tiles[0][i] = r;
    }
    b.oam[0][3] = 100;
    uint32_t tiles = w.stats.chr_tiles;
    fcworld_submit(&w, &b);
    drive(&w, &c, 2);
    CHECK_EQ(w.stats.chr_tiles, tiles);
    check_displayed(&c, &b);
    CHECK_EQ(c.oam[WORLD_OAM_FIRST * 4 + 2] & 0x40, 0x40);
}

/* ---------------------------------------------------------------- atlas */
/* A v2 atlas with one sprite, one family and one 16x16 patch. Local colours
 * 1..3 match NES $0F/$16/$30 exactly; the family candidates are those colours
 * and $0F/$1A/$27. */
enum { FAMILY_BYTES = 16 + 2 * 15 * 64 };
static uint8_t atlas_bytes[48 + 4 + 16 + FAMILY_BYTES + 12 + 8 * 16];
static const uint8_t targets[4] = {0, 0x0F, 0x16, 0x30};

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

static void make_atlas(fcworld_atlas_t *atlas) {
    uint8_t *d = atlas_bytes;
    memset(d, 0, sizeof atlas_bytes);
    uint32_t sprites = 48, entries = 52, families = 68, patches = families + FAMILY_BYTES,
             pixels = patches + 12;
    put32(d, FCWORLD_ATLAS_MAGIC); put16(d + 4, FCWORLD_ATLAS_VERSION); put16(d + 6, 1);
    put32(d + 8, sizeof atlas_bytes); put32(d + 12, 0x1234); put16(d + 16, 1); put16(d + 18, 1);
    put32(d + 20, sprites); put32(d + 24, entries); put32(d + 28, 8); put32(d + 32, patches);
    put32(d + 36, families);
    put16(d + sprites, 0); d[sprites + 2] = 1; d[sprites + 3] = 0;
    for (int r = 0; r < 8; ++r) put16(d + entries + 2 * r, r == 7 ? 0x8001 : 1);
    uint8_t *f = d + families;
    const uint8_t candidates[6] = {0x0F, 0x16, 0x30, 0x0F, 0x1A, 0x27};
    memcpy(f, candidates, 6);
    f[6] = 0; f[7] = 3;
    for (int v = 1; v <= 3; ++v) f[8 + v / 2] |= (uint8_t)(1 << ((v & 1) * 4));
    for (int v = 1; v <= 15; ++v)
        for (int c = 0; c < 64; ++c) {
            uint8_t e = (v <= 3 && c == targets[v]) ? 0 : 100;
            f[16 + (v - 1) * 64 + c] = e;
            f[16 + 15 * 64 + (v - 1) * 64 + c] = e;
        }
    uint8_t *rec = d + patches;
    rec[0] = 16; rec[1] = 8; put16(rec + 2, 8); put16(rec + 4, 8); rec[6] = 0;
    put32(rec + 8, pixels);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 16; ++x)
            d[pixels + y * 8 + x / 2] |= (uint8_t)(((x + y) % 3 + 1) << ((x & 1) * 4));
    CHECK(fcworld_atlas_open(atlas, atlas_bytes, sizeof atlas_bytes));
}

static void test_atlas_v2_lookup_and_family(void) {
    fcworld_atlas_t atlas;
    make_atlas(&atlas);
    fcworld_patch_t patch;
    bool flip;
    CHECK(fcworld_atlas_lookup(&atlas, 0, 0, 3, &flip, &patch));
    CHECK(!flip && patch.width == 16 && patch.height == 8 && patch.family == 0);
    CHECK_EQ(fcworld_patch_texel(&patch, 1, 0), 2);
    CHECK(fcworld_atlas_lookup(&atlas, 0, 0, 7, &flip, &patch) && flip);
    CHECK(!fcworld_atlas_lookup(&atlas, 0, 1, 0, &flip, &patch));
    fcworld_family_t family;
    CHECK(fcworld_atlas_family(&atlas, 0, &family));
    CHECK_EQ(family.count, 3);
    CHECK_EQ(family.weight[2], 1);
    CHECK_EQ(family.select[1 * 64 + 0x16], 0);
    atlas_bytes[4] = 1;  /* version 1 is refused */
    CHECK(!fcworld_atlas_open(&atlas, atlas_bytes, sizeof atlas_bytes));
    atlas_bytes[4] = FCWORLD_ATLAS_VERSION;
}

/* ---------------------------------------------------------------- raster */
static bool hide_right(void *context, int vx, int vy) {
    (void)context;
    (void)vy;
    return vx >= 8;
}

static void test_raster_uses_fcvideo_sampling(void) {
    fcworld_atlas_t atlas;
    make_atlas(&atlas);
    fcworld_projection_t p = {0};
    bool flip;
    CHECK(fcworld_atlas_lookup(&atlas, 0, 0, 0, &flip, &p.patch));
    p.x1 = 0; p.x2 = 15;
    p.startfrac = 0; p.xiscale = 1 << 16;
    p.texturemid = 0; p.iscale = 1 << 16;
    p.centery = 20; p.view_width = 320; p.view_height = 168;
    fcworld_box_t box;
    CHECK(fcworld_bounds(&p, &box));
    CHECK_EQ(box.x, 0);
    CHECK_EQ(box.w, 13);                    /* source columns 4, 9, 14 drop */
    CHECK_EQ(fcworld_native_to_source_y(box.y), 20);
    CHECK_EQ(fcworld_native_to_source_y(box.y - 1), 19);
    CHECK_EQ(fcworld_native_to_source_y(box.y + box.h), 28);
    fcworld_raster_t raster;
    fcworld_raster_reset(&raster);
    CHECK(fcworld_raster_actor(&raster, &p, &box, hide_right, NULL));
    for (int row = 0; row < box.h; ++row) {
        int ty = fcworld_native_to_source_y(box.y + row) - 20;
        for (int col = 0; col < box.w; ++col) {
            int tx = fcworld_native_to_source_x(col);
            uint8_t want = tx >= 8 ? 0 : (uint8_t)((tx + ty) % 3 + 1);
            CHECK_EQ(raster.pixels[row * box.w + col], want);
        }
    }
    /* With the family's candidates loaded, tiles use palette 0 and keep indices. */
    fcworld_palette_state_t shown = {{0x0F, 0x16, 0x30, 0x0F, 0x1A, 0x27}, 0};
    fcworld_frame_t frame;
    CHECK(fcworld_frame_build(&frame, &raster, &atlas, &shown));
    CHECK(frame.oam_count >= 1 && frame.oam_count <= 4);
    CHECK_EQ(frame.oam[0][0], box.y - 1);
    CHECK_EQ(frame.oam[0][2] & 3, 0);
    uint8_t expected[16];
    fcworld_encode_tile(expected, raster.pixels, box.w, 8, 8);
    CHECK_MEM(frame.tiles[frame.oam[0][1]], expected, 16);
}

static void test_plan_respects_shown_palettes(void) {
    fcworld_atlas_t atlas;
    make_atlas(&atlas);
    fcworld_raster_t raster;
    fcworld_raster_reset(&raster);
    raster.count = 1;
    raster.actor[0].family = 0;
    uint8_t palettes[6];
    /* Both slots free: the first candidate takes palette 0. */
    fcworld_palette_state_t shown = {{0x01, 0x11, 0x21, 0x02, 0x12, 0x22}, 0};
    fcworld_plan_palettes(&atlas, &raster, &shown, palettes);
    CHECK_MEM(palettes, atlas_bytes + 68, 3);
    /* Palette 0 is still on screen: the candidate goes to palette 2 instead. */
    shown.busy = 1;
    fcworld_plan_palettes(&atlas, &raster, &shown, palettes);
    CHECK_MEM(palettes, shown.palettes, 3);
    CHECK_MEM(palettes + 3, atlas_bytes + 68, 3);
    /* Both in use: nothing changes and tiles fall back to loaded palettes. */
    shown.busy = 3;
    fcworld_plan_palettes(&atlas, &raster, &shown, palettes);
    CHECK_MEM(palettes, shown.palettes, 6);
    /* An already loaded match is reused rather than duplicated. */
    fcworld_palette_state_t loaded = {{0x02, 0x12, 0x22, 0x0F, 0x16, 0x30}, 0};
    fcworld_plan_palettes(&atlas, &raster, &loaded, palettes);
    CHECK_MEM(palettes + 3, loaded.palettes + 3, 3);
    fcworld_frame_t frame;
    fcworld_frame_clear(&frame, palettes);
    frame.oam_count = 2;
    frame.oam[0][2] = 2;
    frame.oam[1][2] = 3 | 0x40;
    fcworld_frame_state(&frame, &shown);
    CHECK_EQ(shown.busy, 2);
}

static void test_frame_dedupes_mirrors_and_limits_lines(void) {
    fcworld_atlas_t atlas;
    make_atlas(&atlas);
    fcworld_palette_state_t shown = {{0x0F, 0x16, 0x30, 0x0F, 0x1A, 0x27}, 0};
    fcworld_raster_t raster;
    fcworld_raster_reset(&raster);
    raster.count = 1;
    raster.actor[0].box = (fcworld_box_t){10, 50, 16, 8};
    raster.actor[0].family = 0;
    raster.actor[0].offset = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            uint8_t v = (uint8_t)((x * y) % 4);
            raster.pixels[y * 16 + x] = v;
            raster.pixels[y * 16 + 15 - x] = v;
        }
    raster.used = 128;
    fcworld_frame_t frame;
    CHECK(fcworld_frame_build(&frame, &raster, &atlas, &shown));
    CHECK_EQ(frame.tile_count, 1);
    CHECK_EQ(frame.oam_count, 2);
    CHECK_EQ(frame.oam[1][2], 0 | 0x40);

    /* Nine tile columns on one line exceed the sprite evaluation limit. */
    fcworld_raster_reset(&raster);
    raster.count = 1;
    raster.actor[0].box = (fcworld_box_t){0, 60, 72, 8};
    raster.actor[0].family = 0;
    memset(raster.pixels, 1, 72 * 8);
    raster.used = 72 * 8;
    CHECK(!fcworld_frame_build(&frame, &raster, &atlas, &shown));
    CHECK_EQ(frame.oam_count, 0);
}

/* ---------------------------------------------------------------- gating */
static fcworld_t g_world;
static void hook(void *user, uint8_t *mailbox, uint8_t pad2, bool swapped) {
    (void)user;
    (void)swapped;
    fcworld_heartbeat(&g_world, mailbox, pad2);
}

static void test_core_gate_matches_console_generation(void) {
    static fcbus_core_t core;
    static const uint8_t rom[FCBUS_ROM_PRG_BYTES] = {0};
    fcbus_config_t cfg = {.rom_image = rom, .proto_default = FCBUS_PROTO_V5};
    fcbus_core_init(&core, &cfg);
    fcworld_reset(&g_world);
    fcbus_core_set_mailbox_hook(&core, hook, NULL);
    console_t c;
    console_init(&c);
    fcworld_frame_t a;
    make_frame(&a, 6, 3);
    uint8_t gen = fcworld_submit(&g_world, &a);
    fcbus_core_publish_world(&core, gen);
    int old_front = core.front;
    bool swapped_with_sprites = false;
    for (int beat = 0; beat < 60 && !fcbus_core_back_is_free(&core); ++beat) {
        core.pad2 = c.shown << WORLD_HB_GEN_SHIFT | c.ack;
        uint8_t shown_before = c.shown;
        fcbus_core_heartbeat(&core, PPU_COUNT_VAL_V5);
        if (core.front != old_front) {
            /* The swap heartbeat is the first one reporting gen in OAM. */
            CHECK_EQ(shown_before, gen);
            swapped_with_sprites = true;
        }
        const uint8_t *tail = (const uint8_t *)fcbus_core_stream_front(&core) + VRAM_MAILBOX_OFF_V5;
        console_nmi(&c, tail);
        console_main(&c, beat % 7 == 3);
    }
    CHECK(swapped_with_sprites);
    check_displayed(&c, &a);
}

int main(void) {
    test_atlas_v2_lookup_and_family();
    test_raster_uses_fcvideo_sampling();
    test_plan_respects_shown_palettes();
    test_frame_dedupes_mirrors_and_limits_lines();
    test_scheduler_uploads_commits_and_protects();
    test_flipped_cache_reuse();
    test_core_gate_matches_console_generation();
    return ctest_lite_result();
}

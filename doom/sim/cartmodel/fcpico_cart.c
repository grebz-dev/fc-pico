/*
 * fcpico_cart.c -- bounded host cartridge adapter for Mesen co-simulation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcpico_cart.h"

#include <string.h>

#include "fcbus_host.h"

static bool g_initialized;
static const uint8_t *g_prg;
static fcpico_cart_metrics_t g_metrics;

static uint8_t pattern_pixel(uint32_t frame, int x, int y) {
    (void)frame;
    (void)y;
    /* Pattern 0 from port/main_testpattern.c.  S0 intentionally keeps it
     * stationary so its screenshot remains a deterministic integration check. */
    return (uint8_t)((x / 34) & 3);
}

static void build_pattern(uint16_t *stream, uint32_t frame) {
    for (int y = 0; y < VRAM_LINES; ++y) {
        for (int tile = 0; tile < VRAM_LINE_WORDS; ++tile) {
            uint16_t word = 0;
            for (int pixel = 0; pixel < 8; ++pixel) {
                uint8_t colour = pattern_pixel(frame, tile * 8 + pixel, y);
                word = (uint16_t)((word << 1) | (colour & 1u) |
                                  ((colour & 2u) << 7));
            }
            *fcbus_stream_word(stream, y, tile) = word;
        }
    }
}

static void fill_tables(fcbus_core_t *core, uint32_t frame) {
    static const uint8_t palette[MBX_PAL_LEN] = {
        0x0f, 0x00, 0x10, 0x20, 0x0f, 0x06, 0x16, 0x26,
        0x0f, 0x09, 0x19, 0x29, 0x0f, 0x01, 0x21, 0x31,
    };
    uint8_t attributes[MBX_ATTR_LEN];

    for (size_t index = 0; index < sizeof attributes; ++index) {
        attributes[index] = (uint8_t)(((index + frame / 30u) & 1u) ? 0x1b : 0xe4);
    }
    fcbus_core_palette(core, palette);
    fcbus_core_attr_table(core, attributes);
}

static void publish_pattern(uint32_t frame) {
    fcbus_core_t *core = fcbus_host_core();
    if (!fcbus_core_back_is_free(core)) {
        return;
    }

    build_pattern(fcbus_core_stream_back(core), frame);
    fill_tables(core, frame);
    fcbus_core_publish(core);
    g_metrics.pattern_frames++;
}

static void handle_init(uint8_t stage) {
    fcbus_core_t *core = fcbus_host_core();

    /* AplGame sends FP_COM_INI + PICO_STAGE, then NMI reads the following
     * mailbox.  The tutorial's per-frame PF_COM_VRAM path is compiled out, so
     * the first palette and attribute tables must travel through its actual
     * PF_COM_DMOD -> DRQ -> DLD transfer before the display is meaningful. */
    if (fcbus_core_back_is_free(core)) {
        build_pattern(fcbus_core_stream_back(core), 0);
        /* Queue DMOD before the tables.  On v1 table diffs can consume the
         * 14-byte mailbox command area, while DMOD must be present in the
         * first post-init mailbox to reach the tutorial's bulk-transfer code. */
        fcbus_core_request_data_mode(core);
        fill_tables(core, 0);
        fcbus_core_publish(core);
        g_metrics.pattern_frames++;
    }
    g_metrics.init_actions++;
    g_metrics.last_init_stage = stage;
}

static void reset_link(void) {
    fcbus_config_t config = {
        .rom_image = g_prg,
        .proto_default = FCBUS_PROTO_V1,
    };
    fcbus_host_init(&config);
    memset(&g_metrics, 0, sizeof g_metrics);
}

/* The adapter owns the host action log.  Clear it before every input byte, then
 * consume only the actions that byte actually caused.  In particular, this
 * avoids treating an opcode-looking byte in a ROM-page or FP_COM_LOG payload
 * as a second parser would. */
static void consume_actions(void) {
    const fcbus_host_log_entry_t *entries = fcbus_host_action_log();
    size_t count = fcbus_host_action_log_count();
    bool reset = false;

    for (size_t index = 0; index < count; ++index) {
        switch (entries[index].kind) {
        case FCBUS_ACT_INIT:
            handle_init((uint8_t)entries[index].arg);
            break;
        case FCBUS_ACT_RESET:
            reset = true;
            break;
        case FCBUS_ACT_LOG:
        case FCBUS_ACT_PROTO_ERROR:
        case FCBUS_ACT_STREAM_RESPONSE:
        case FCBUS_ACT_HEARTBEAT:
        case FCBUS_ACT_STOP_DMA:
        default:
            break;
        }
    }

    if (reset) {
        /* FP_COM_RST means restart the cartridge firmware.  Retaining the
         * caller-owned PRG is enough to recreate the bounded host model; its
         * public protocol and adapter metrics intentionally restart at zero. */
        reset_link();
        return;
    }
    fcbus_host_action_log_clear();
}

bool fcpico_cart_init(const uint8_t *prg, size_t prg_len) {
    if (prg == NULL || prg_len != FCBUS_ROM_PRG_BYTES) {
        return false;
    }

    g_prg = prg;
    reset_link();
    g_initialized = true;
    return true;
}

void fcpico_cart_shutdown(void) {
    g_initialized = false;
    g_prg = NULL;
    memset(&g_metrics, 0, sizeof g_metrics);
}

uint8_t fcpico_cart_ppu_read(void) {
    if (!g_initialized) {
        return 0xff;
    }

    int value = fcbus_host_ppu_read();
    g_metrics.ppu_reads++;
    if (value < 0) {
        g_metrics.open_bus_reads++;
        return 0xff;
    }
    return (uint8_t)value;
}

void fcpico_cart_ppu_write(uint8_t value) {
    if (!g_initialized) {
        return;
    }

    uint32_t frames_before = fcbus_core_stats(fcbus_host_core())->frames;
    fcbus_host_action_log_clear();
    fcbus_host_ppu_write(value);
    g_metrics.ppu_writes++;
    consume_actions();

    /* The device test-pattern main loop notices a completed heartbeat and
     * fills the now-free back buffer for the next one.  Do the same on this
     * emulator thread; no count, state, or stream layout is adjusted here. */
    if (fcbus_core_stats(fcbus_host_core())->frames != frames_before) {
        publish_pattern(fcbus_core_stats(fcbus_host_core())->frames);
    }
}

void fcpico_cart_tick_ms(uint32_t now_ms) {
    if (g_initialized) {
        fcbus_host_tick_ms(now_ms);
    }
}

const fcbus_stats_t *fcpico_cart_stats(void) {
    return g_initialized ? fcbus_core_stats(fcbus_host_core()) : NULL;
}

const fcpico_cart_metrics_t *fcpico_cart_metrics(void) {
    return g_initialized ? &g_metrics : NULL;
}

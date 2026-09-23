/*
 * fcpico_cart.c -- bounded host cartridge adapter for Mesen co-simulation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcpico_cart.h"

#include <string.h>

#include "fcbus_host.h"
#include "testpattern.h"

static bool g_initialized;
static const uint8_t *g_prg;
static fcpico_cart_metrics_t g_metrics;
static fcpico_testpattern_t g_pattern_app;

static void publish_pattern(uint32_t frame) {
    (void)frame;
    if (fcpico_testpattern_poll(&g_pattern_app, 0)) {
        g_metrics.pattern_frames++;
    }
}

static void handle_init(uint8_t stage) {
    if (fcpico_testpattern_console_init(&g_pattern_app, stage, 0)) {
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
    fcpico_testpattern_init(&g_pattern_app, fcbus_host_core());
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

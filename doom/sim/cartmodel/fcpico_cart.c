/*
 * fcpico_cart.c -- bounded host cartridge adapter for Mesen co-simulation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcpico_cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fcbus_host.h"
#include "testpattern.h"

static bool g_initialized;
static uint8_t g_prg[FCBUS_ROM_PRG_BYTES];
static fcpico_cart_metrics_t g_metrics;
static fcpico_testpattern_t g_pattern_app;
static uint8_t g_stream[VRAM_BUF_BYTES_V3];
static size_t g_stream_length;
static bool g_stream_enabled;
static bool g_rom_v2;
static bool g_rom_v3;
static bool g_rom_v4;

/* The console's PRG is flash and may be rewritten by the fix bank while the
 * firmware keeps serving its own compiled-in image, so the model keeps a copy. */
static bool load_served_rom(const char *path) {
    FILE *input = fopen(path, "rb");
    if (input == NULL) return false;
    uint8_t header[FCBUS_ROM_INES_HDR];
    bool valid = fread(header, 1, sizeof header, input) == sizeof header &&
                 memcmp(header, "NES\x1a", 4) == 0 &&
                 fread(g_prg, 1, sizeof g_prg, input) == sizeof g_prg && fgetc(input) == EOF;
    if (fclose(input) != 0) valid = false;
    return valid;
}

static bool load_stream(const char *path) {
    FILE *input = fopen(path, "rb");
    if (input == NULL) return false;
    g_stream_length = fread(g_stream, 1, sizeof g_stream, input);
    bool valid = (g_stream_length == VRAM_BUF_BYTES_V2 ||
                  g_stream_length == VRAM_BUF_BYTES_V3) && fgetc(input) == EOF;
    if (fclose(input) != 0) valid = false;
    if (!valid) return false;
    const uint8_t *mbx_plain = g_stream + VRAM_MAILBOX_OFF_V2;
    const uint8_t *mbx_text = g_stream + VRAM_MAILBOX_OFF_V4;
    const uint8_t required = MBX_FLAG_V2 | MBX_FLAG_ATTR_VALID | MBX_FLAG_PAL_VALID;
    return (mbx_plain[MBX_MAGIC] == PF_MAGIC_NO &&
            (mbx_plain[MBX_FLAGS] & required) == required) ||
           (g_stream_length == VRAM_BUF_BYTES_V4 && mbx_text[MBX_MAGIC] == PF_MAGIC_NO &&
            (mbx_text[MBX_FLAGS] & (required | MBX_FLAG_V4)) ==
                (required | MBX_FLAG_V4));
}

static bool publish_stream(bool init) {
    fcbus_core_t *core = fcbus_host_core();
    if (!fcbus_core_back_is_free(core)) return false;
    const uint8_t *mailbox = g_stream +
        (g_rom_v4 ? VRAM_MAILBOX_OFF_V4 : VRAM_MAILBOX_OFF_V2);
    memcpy(fcbus_core_stream_back(core), g_stream, g_stream_length);
    if (init && !g_rom_v2 && !g_rom_v3 && !g_rom_v4) fcbus_core_request_data_mode(core);
    fcbus_core_palette(core, mailbox + MBX_PAL);
    fcbus_core_attr_table(core, mailbox + MBX_ATTR);
    if ((g_rom_v3 || g_rom_v4) && g_stream_length == VRAM_BUF_BYTES_V3) {
        (void)fcbus_core_ui_snapshot(core, mailbox + MBX_UI);
    }
    fcbus_core_publish(core);
    return true;
}

static void publish_pattern(uint32_t frame) {
    (void)frame;
    if (fcpico_testpattern_poll(&g_pattern_app, 0)) {
        g_metrics.pattern_frames++;
    }
}

static void handle_init(uint8_t stage) {
    if (g_stream_enabled) {
        (void)publish_stream(true);
    } else if (fcpico_testpattern_console_init(&g_pattern_app, stage, 0)) {
        g_metrics.pattern_frames++;
    }
    g_metrics.init_actions++;
    g_metrics.last_init_stage = stage;
}

static void reset_link(void) {
    fcbus_config_t config = {
        .rom_image = g_prg,
        /* Match the device: the Doom firmware assumes v2 until told otherwise. */
        .proto_default = g_rom_v4 ? FCBUS_PROTO_V4 :
                         g_rom_v3 ? FCBUS_PROTO_V3 :
                         g_rom_v2 ? FCBUS_PROTO_V2 : FCBUS_PROTO_V1,
    };
    fcbus_host_init(&config);
    fcbus_host_set_bulk_lead_byte(g_stream_enabled && !g_rom_v2 && !g_rom_v3 && !g_rom_v4);
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

    const char *stream_path = getenv("FCPICO_STREAM_FILE");
    g_stream_enabled = stream_path != NULL && stream_path[0] != '\0';
    if (g_stream_enabled && !load_stream(stream_path)) return false;
    const char *serve_path = getenv("FCPICO_SERVE_ROM");
    if (serve_path != NULL && serve_path[0] != '\0') {
        if (!load_served_rom(serve_path)) return false;
    } else {
        memcpy(g_prg, prg, sizeof g_prg);
    }
    g_rom_v2 = memcmp(g_prg + FCBUS_ROM_STAMP_OFF, "20DOOM-02-", 10) == 0;
    g_rom_v3 = memcmp(g_prg + FCBUS_ROM_STAMP_OFF, "20DOOM-03-", 10) == 0;
    g_rom_v4 = memcmp(g_prg + FCBUS_ROM_STAMP_OFF, "20DOOM-04-", 10) == 0;
    if ((g_rom_v3 || g_rom_v4) && g_stream_enabled &&
        g_stream_length != VRAM_BUF_BYTES_V3) return false;
    reset_link();
    g_initialized = true;
    return true;
}

void fcpico_cart_shutdown(void) {
    g_initialized = false;
    g_stream_enabled = false;
    g_rom_v2 = false;
    g_rom_v3 = false;
    g_rom_v4 = false;
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
        if (g_stream_enabled) {
            (void)publish_stream(false);
        } else {
            publish_pattern(fcbus_core_stats(fcbus_host_core())->frames);
        }
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

/**
 * @file fcbus_host.c
 * @brief Host backend for fcbus_core: a byte-level model of the PPU bus.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcbus_host.h"

#include <string.h>

static void log_action(fcbus_host_t *h, const fcbus_action_t *act) {
    if (h->log_len >= FCBUS_HOST_LOG_CAP) {
        memmove(&h->log[0], &h->log[1], sizeof h->log[0] * (FCBUS_HOST_LOG_CAP - 1));
        h->log_len--;
    }
    fcbus_host_log_entry_t *e = &h->log[h->log_len++];
    e->kind = act->kind;
    e->arg = act->arg;
    e->len = 0;
    if (act->data != NULL && act->len > 0) {
        e->len = act->len > FCBUS_LOG_MAX ? FCBUS_LOG_MAX : act->len;
        memcpy(e->data, act->data, e->len);
    }
}

static void drain(fcbus_host_t *h) {
    fcbus_action_t act;
    while (fcbus_core_pop_action(&h->core, &act)) {
        switch (act.kind) {
        case FCBUS_ACT_STREAM_RESPONSE:
            h->resp = act.data;
            h->resp_len = act.len;
            h->resp_pos = 0;
            h->mode = act.len > 0 ? FCBUS_HOST_RESPONSE : FCBUS_HOST_FILLER;
            /* On hardware the console receives one extra byte before an attribute
             * table page ($23C0); the tutorial boot ROM discards it with a dummy
             * read ("cause never identified" in BOOTROM/SysPico.asm). */
            h->lead_byte = act.data == h->core.dld_buf && h->core.drq_source == h->core.attr_want;
            break;
        case FCBUS_ACT_HEARTBEAT:
            /* A manual `out pins, 8` nudge consumes one buffer byte. */
            h->stream_pos = act.arg == FCBUS_ARM_NUDGE2 ? 2 : act.arg == FCBUS_ARM_NUDGE1 ? 1 : 0;
            h->mode = FCBUS_HOST_STREAM;
            h->read_count = 0;
            break;
        case FCBUS_ACT_STOP_DMA:
            h->mode = FCBUS_HOST_STOPPED;
            h->read_count = 0;
            break;
        default:
            log_action(h, &act);
            break;
        }
    }
}

void fcbus_host_init(fcbus_host_t *h, const fcbus_config_t *cfg) {
    memset(h, 0, sizeof *h);
    fcbus_core_init(&h->core, cfg);
    h->mode = FCBUS_HOST_STOPPED;
}

int fcbus_host_ppu_read(fcbus_host_t *h) {
    h->read_count++;
    switch (h->mode) {
    case FCBUS_HOST_RESPONSE: {
        if (h->lead_byte) {
            h->lead_byte = false;
            return 0x00;
        }
        uint8_t b = h->resp[h->resp_pos++];
        if (h->resp_pos >= h->resp_len) {
            h->mode = FCBUS_HOST_FILLER;
        }
        return b;
    }
    case FCBUS_HOST_FILLER:
        return 0xFF;
    case FCBUS_HOST_STREAM: {
        const uint8_t *front = (const uint8_t *)fcbus_core_stream_front(&h->core);
        uint32_t pos = h->stream_pos++;
        return pos < VRAM_BUF_BYTES ? front[pos] : 0xFF;
    }
    case FCBUS_HOST_STOPPED:
    default:
        return -1;
    }
}

void fcbus_host_ppu_write(fcbus_host_t *h, uint8_t b) {
    uint32_t reported = h->read_count > 0 ? h->read_count - PPU_COUNTER_REPORT_BIAS : 0;
    fcbus_core_set_read_count(&h->core, reported);
    fcbus_core_rx_byte(&h->core, b);
    drain(h);
}

void fcbus_host_write_burst(fcbus_host_t *h, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        fcbus_host_ppu_write(h, data[i]);
    }
    fcbus_core_rx_idle(&h->core);
    drain(h);
}

fcbus_core_t *fcbus_host_core(fcbus_host_t *h) {
    return &h->core;
}

const fcbus_host_log_entry_t *fcbus_host_log(const fcbus_host_t *h, size_t *count) {
    *count = h->log_len;
    return h->log;
}

void fcbus_host_log_clear(fcbus_host_t *h) {
    h->log_len = 0;
}

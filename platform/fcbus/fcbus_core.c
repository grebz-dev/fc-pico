/**
 * @file fcbus_core.c
 * @brief Backend-independent FC PICO cartridge bus logic for the tutorial protocol.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See fcbus_core.h for the overview and the rp_system function each part mirrors.
 */
#include "fcbus_core.h"

#include <string.h>

/* ---------------------------------------------------------------------------- */
/* Action ring                                                                    */
/* ---------------------------------------------------------------------------- */

static void push_action(fcbus_core_t *c, fcbus_action_kind_t kind, uint32_t arg,
                        const uint8_t *data, uint16_t len) {
    if (c->action_count >= FCBUS_ACTION_RING_SIZE) {
        /* A byte produces at most two actions and backends drain after every byte,
         * so this does not happen in practice; keep the newest if it does. */
        c->action_tail = (uint8_t)((c->action_tail + 1) % FCBUS_ACTION_RING_SIZE);
        c->action_count--;
    }
    fcbus_action_t *slot = &c->action_ring[c->action_head];
    slot->kind = kind;
    slot->arg = arg;
    slot->data = data;
    slot->len = len;
    c->action_head = (uint8_t)((c->action_head + 1) % FCBUS_ACTION_RING_SIZE);
    c->action_count++;
}

bool fcbus_core_pop_action(fcbus_core_t *c, fcbus_action_t *out) {
    if (c->action_count == 0) {
        return false;
    }
    *out = c->action_ring[c->action_tail];
    c->action_tail = (uint8_t)((c->action_tail + 1) % FCBUS_ACTION_RING_SIZE);
    c->action_count--;
    return true;
}

/* ---------------------------------------------------------------------------- */
/* Mailbox: rp_system::initFC_COM_BUF / setPF_COM / setPF_VRAM / setAPU / setPF_APU */
/* ---------------------------------------------------------------------------- */

static void reset_mailbox(fcbus_core_t *c) {
    memset(c->mailbox, 0, sizeof c->mailbox);
    c->mailbox[MBX_MAGIC] = PF_MAGIC_NO;
    c->cmd_cursor = MBX_CMD;
    /* startDataMode() keeps setting PF_COM_DMOD until FP_COM_DRQ arrives. */
    if (c->data_mode_requested) {
        (void)fcbus_core_cmd(c, PF_COM_DMOD);
    }
}

const uint8_t *fcbus_core_mailbox(const fcbus_core_t *c) {
    return c->mailbox;
}

bool fcbus_core_cmd(fcbus_core_t *c, uint8_t com) {
    if (c->cmd_cursor >= FC_COM_BUF_SIZE16) {
        return false;
    }
    c->mailbox[c->cmd_cursor++] = com;
    return true;
}

bool fcbus_core_cmd_vram(fcbus_core_t *c, uint16_t addr, uint8_t val) {
    if (c->cmd_cursor >= FC_COM_BUF_SIZE16 - 2) {
        return false;
    }
    addr &= 0x3FFF;
    c->mailbox[c->cmd_cursor++] = (uint8_t)(PF_COM_VRAM | (addr >> 8));
    c->mailbox[c->cmd_cursor++] = (uint8_t)(addr & 0xFF);
    c->mailbox[c->cmd_cursor++] = val;
    return true;
}

void fcbus_core_apu_write(fcbus_core_t *c, uint8_t reg, uint8_t val) {
    c->apu_reg[c->apu_w] = reg;
    c->apu_dat[c->apu_w] = val;
    c->apu_w = (uint8_t)((c->apu_w + 1) & (PICO_APU_BUF_SIZE - 1));
}

void fcbus_core_apu_flush(fcbus_core_t *c) {
    int i = MBX_APU;
    while (i < FC_COM_BUF_SIZE - 2 && c->apu_r != c->apu_w) {
        c->mailbox[i++] = c->apu_reg[c->apu_r];
        c->mailbox[i++] = c->apu_dat[c->apu_r];
        c->apu_r = (uint8_t)((c->apu_r + 1) & (PICO_APU_BUF_SIZE - 1));
    }
    c->mailbox[i] = 0xFF;
}

/* ---------------------------------------------------------------------------- */
/* Palette and attribute table: rp_system::setPalData ... update()               */
/* ---------------------------------------------------------------------------- */

void fcbus_core_set_palette(fcbus_core_t *c, const uint8_t pal[FCBUS_PAL_LEN]) {
    memcpy(c->pal_want, pal, FCBUS_PAL_LEN);
    memcpy(c->pal_sent, pal, FCBUS_PAL_LEN);
    c->pal_chg = true;
}

void fcbus_core_set_pal(fcbus_core_t *c, uint8_t idx, uint8_t val) {
    idx &= FCBUS_PAL_LEN - 1;
    if (c->pal_want[idx] != val) {
        c->pal_want[idx] = val;
        c->pal_chg = true;
    }
}

void fcbus_core_set_attr_table(fcbus_core_t *c, const uint8_t attr[FCBUS_ATTR_LEN]) {
    memcpy(c->attr_want, attr, FCBUS_ATTR_LEN);
    memcpy(c->attr_sent, attr, FCBUS_ATTR_LEN);
    c->attr_chg = true;
}

void fcbus_core_clear_attr(fcbus_core_t *c) {
    memset(c->attr_want, 0, FCBUS_ATTR_LEN);
    memset(c->attr_sent, 0, FCBUS_ATTR_LEN);
    c->attr_chg = true;
}

void fcbus_core_set_attr(fcbus_core_t *c, uint8_t lx, uint8_t ly, uint8_t val) {
    static const uint8_t mask[4] = {0xFC, 0xF3, 0xCF, 0x3F};
    static const uint8_t unit[4] = {0x01, 0x04, 0x10, 0x40};
    uint8_t idx = (uint8_t)(((lx >> 1) + ((ly >> 1) << 3)) & 0x3F);
    uint8_t sel = (uint8_t)((lx & 1) + ((ly & 1) << 1));
    c->attr_want[idx] &= mask[sel];
    c->attr_want[idx] |= (uint8_t)(unit[sel] * val);
    c->attr_chg = true;
}

void fcbus_core_update_tables(fcbus_core_t *c) {
    for (int i = 0; i < FCBUS_PAL_LEN; i++) {
        if (c->pal_want[i] != c->pal_sent[i]) {
            if (!fcbus_core_cmd_vram(c, (uint16_t)(0x3F00 + i), c->pal_want[i])) {
                break;
            }
            c->pal_sent[i] = c->pal_want[i];
        }
    }
    for (int i = 0; i < FCBUS_ATTR_LEN; i++) {
        if (c->attr_want[i] != c->attr_sent[i]) {
            if (!fcbus_core_cmd_vram(c, (uint16_t)(0x23C0 + i), c->attr_want[i])) {
                break;
            }
            c->attr_sent[i] = c->attr_want[i];
        }
    }
}

/* ---------------------------------------------------------------------------- */
/* Picture buffers                                                                */
/* ---------------------------------------------------------------------------- */

uint16_t *fcbus_core_stream_back(fcbus_core_t *c) {
    return (uint16_t *)c->stream[1 - c->front];
}

const uint16_t *fcbus_core_stream_front(const fcbus_core_t *c) {
    return (const uint16_t *)c->stream[c->front];
}

bool fcbus_core_back_is_free(const fcbus_core_t *c) {
    return !c->publish_pending;
}

void fcbus_core_publish(fcbus_core_t *c) {
    c->publish_pending = true;
}

/* ---------------------------------------------------------------------------- */
/* Frame sync: rp_system::ppu_dma()                                               */
/* ---------------------------------------------------------------------------- */

fcbus_sync_t fcbus_sync_decide(uint32_t count, uint32_t expected) {
    int64_t d = (int64_t)count - (int64_t)expected;
    if (d > PPU_COUNT_WINDOW || d < -PPU_COUNT_WINDOW) {
        return FCBUS_STOP;
    }
    if (d == -2) {
        return FCBUS_ARM_NUDGE2;
    }
    if (d == -1) {
        return FCBUS_ARM_NUDGE1;
    }
    return FCBUS_ARM;
}

fcbus_sync_t fcbus_core_heartbeat(fcbus_core_t *c, uint32_t count) {
    fcbus_sync_t decision = fcbus_sync_decide(count, PPU_COUNT_VAL);
    if (c->state == FCBUS_ST_IDLE) {
        c->state = FCBUS_ST_RUN;
    }

    c->stats.frames++;
    c->stats.last_count = count;
    int64_t d = (int64_t)count - (int64_t)PPU_COUNT_VAL;
    d = d < -4 ? -4 : d > 4 ? 4 : d;
    c->stats.count_hist[d + 4]++;

    if (decision == FCBUS_STOP) {
        c->stats.dma_stops++;
        c->stopped_last_heartbeat = true;
        push_action(c, FCBUS_ACT_STOP_DMA, (uint32_t)decision, NULL, 0);
        return decision;
    }
    if (c->stopped_last_heartbeat) {
        c->stats.resyncs++;
    }
    c->stopped_last_heartbeat = false;

    if (c->publish_pending) {
        c->front = 1 - c->front;
        c->publish_pending = false;
    }
    memcpy((uint8_t *)c->stream[c->front] + VRAM_MAILBOX_OFF, c->mailbox, FC_COM_BUF_SIZE);
    reset_mailbox(c);
    push_action(c, FCBUS_ACT_HEARTBEAT, (uint32_t)decision, NULL, 0);
    return decision;
}

uint8_t fcbus_core_pad(const fcbus_core_t *c) {
    return c->pad;
}

/* ---------------------------------------------------------------------------- */
/* Bulk data mode: rp_system::startDataMode / jobFP_COM_DRQ / drq_ret / DLD      */
/* ---------------------------------------------------------------------------- */

void fcbus_core_request_data_mode(fcbus_core_t *c) {
    if (!c->data_mode_requested) {
        c->data_mode_requested = true;
        (void)fcbus_core_cmd(c, PF_COM_DMOD);
    }
}

void fcbus_core_set_step(fcbus_core_t *c, uint8_t step) {
    c->step = step;
}

static void drq_reply(fcbus_core_t *c, uint8_t com, uint16_t adr, uint16_t size) {
    uint32_t w0 = FCBUS_DRQ_MAGIC_BASE + com + ((uint32_t)adr << 16);
    uint32_t w1 = size;
    for (int i = 0; i < 4; i++) {
        c->drq_buf[i] = (uint8_t)(w0 >> (8 * i));
        c->drq_buf[4 + i] = (uint8_t)(w1 >> (8 * i));
    }
    push_action(c, FCBUS_ACT_STREAM_RESPONSE, 0, c->drq_buf, sizeof c->drq_buf);
}

static void job_drq(fcbus_core_t *c) {
    c->data_mode_requested = false;
    c->state = FCBUS_ST_DATA;
    if (c->pal_chg) {
        c->pal_chg = false;
        c->drq_source = c->pal_want;
        c->drq_source_len = FCBUS_PAL_LEN;
        drq_reply(c, PF_DAT_VRAM, 0x3F00, FCBUS_PAL_LEN);
        return;
    }
    if (c->attr_chg) {
        c->attr_chg = false;
        c->drq_source = c->attr_want;
        c->drq_source_len = FCBUS_ATTR_LEN;
        drq_reply(c, PF_DAT_VRAM, 0x23C0, FCBUS_ATTR_LEN);
        return;
    }
    c->state = FCBUS_ST_RUN;
    if (c->step != 0) {
        drq_reply(c, PF_DAT_STEP, c->step, 0);
        c->step = 0;
        return;
    }
    drq_reply(c, PF_COM_NONE, 0, 0);
}

static void job_dld(fcbus_core_t *c, uint8_t page) {
    /* rp_system streams 256 bytes from m_pDRQ + (page << 8). Only page 0 of a 32- or
     * 64-byte table carries data; the rest of the page is served as zero rather than
     * whatever memory follows the table. */
    memset(c->dld_buf, 0, sizeof c->dld_buf);
    if (page == 0 && c->drq_source != NULL) {
        memcpy(c->dld_buf, c->drq_source, c->drq_source_len);
    }
    push_action(c, FCBUS_ACT_STREAM_RESPONSE, 0, c->dld_buf, sizeof c->dld_buf);
}

/* ---------------------------------------------------------------------------- */
/* FP_COM_VER / FP_COM_ROM: rp_system::ver_dma / rom_dma                          */
/* ---------------------------------------------------------------------------- */

static void job_ver(fcbus_core_t *c) {
    for (int i = 0; i < 4; i++) {
        c->ver_buf[i] = (uint8_t)(FCBUS_VER_SYNC_W0 >> (8 * i));
        c->ver_buf[4 + i] = (uint8_t)(FCBUS_VER_SYNC_W1 >> (8 * i));
    }
    memcpy(c->ver_buf + 8, c->rom_image + FCBUS_ROM_STAMP_OFF, 16);
    push_action(c, FCBUS_ACT_STREAM_RESPONSE, 0, c->ver_buf, sizeof c->ver_buf);
}

static void job_rom(fcbus_core_t *c, uint8_t page) {
    /* rom_dma() keeps streaming past the requested page; ROM_UPDATE's final dummy
     * $2007 read takes the first byte of the next page. */
    uint32_t off = (uint32_t)(page & 0x7F) << 8;
    push_action(c, FCBUS_ACT_STREAM_RESPONSE, 0, c->rom_image + off,
                (uint16_t)(FCBUS_ROM_PRG_BYTES - off));
}

/* ---------------------------------------------------------------------------- */
/* Reset: rp_system::init2()                                                      */
/* ---------------------------------------------------------------------------- */

/* Like init2(), this keeps the palette, the APU ring and the data-mode source. */
static void reset_link(fcbus_core_t *c) {
    memset(c->stream, 0, sizeof c->stream);
    c->front = 0;
    c->publish_pending = false;
    c->data_mode_requested = false;
    reset_mailbox(c);
    c->pal_chg = false;
    fcbus_core_clear_attr(c);
    c->pad = 0;
    c->step = 0;
    c->state = FCBUS_ST_IDLE;
}

/* ---------------------------------------------------------------------------- */
/* Receive: rp_system::jobRcvCom()                                                */
/* ---------------------------------------------------------------------------- */

static bool is_opcode(uint8_t b) {
    switch (b) {
    case FP_COM_VER:
    case FP_COM_ROM:
    case FP_COM_LOG:
    case FP_COM_DRQ:
    case FP_COM_DLD:
    case FP_COM_RST:
    case FP_COM_INI:
        return true;
    default:
        return false;
    }
}

void fcbus_core_set_read_count(fcbus_core_t *c, uint32_t count) {
    c->pending_read_count = count;
}

bool fcbus_core_rx_will_heartbeat(const fcbus_core_t *c, uint8_t b) {
    return c->rxwait == FCBUS_RXW_NONE && !is_opcode(b);
}

static void end_log(fcbus_core_t *c) {
    push_action(c, FCBUS_ACT_LOG, 0, c->log_buf, c->log_len);
    c->rxwait = FCBUS_RXW_NONE;
    c->log_len = 0;
}

void fcbus_core_rx_idle(fcbus_core_t *c) {
    if (c->rxwait == FCBUS_RXW_LOG) {
        end_log(c);
    }
}

void fcbus_core_rx_byte(fcbus_core_t *c, uint8_t b) {
    switch (c->rxwait) {
    case FCBUS_RXW_ROM_PAGE:
        c->rxwait = FCBUS_RXW_NONE;
        job_rom(c, b);
        return;
    case FCBUS_RXW_DLD_PAGE:
        c->rxwait = FCBUS_RXW_NONE;
        job_dld(c, b);
        return;
    case FCBUS_RXW_INI_STAGE:
        c->rxwait = FCBUS_RXW_NONE;
        reset_link(c);
        c->stats.init_events++;
        c->stats.last_init_stage = b;
        push_action(c, FCBUS_ACT_INIT, b, NULL, 0);
        return;
    case FCBUS_RXW_LOG:
        c->log_buf[c->log_len++] = b;
        if (c->log_len >= FCBUS_LOG_MAX) {
            end_log(c);
        }
        return;
    case FCBUS_RXW_NONE:
    default:
        break;
    }

    switch (b) {
    case FP_COM_VER:
        job_ver(c);
        return;
    case FP_COM_ROM:
        c->rxwait = FCBUS_RXW_ROM_PAGE;
        return;
    case FP_COM_LOG:
        c->rxwait = FCBUS_RXW_LOG;
        c->log_len = 0;
        return;
    case FP_COM_DRQ:
        job_drq(c);
        return;
    case FP_COM_DLD:
        c->rxwait = FCBUS_RXW_DLD_PAGE;
        return;
    case FP_COM_RST:
        c->state = FCBUS_ST_IDLE;
        push_action(c, FCBUS_ACT_RESET, 0, NULL, 0);
        return;
    case FP_COM_INI:
        c->rxwait = FCBUS_RXW_INI_STAGE;
        return;
    default:
        /* Any other byte is the controller state and marks the end of a frame. */
        c->pad = b;
        (void)fcbus_core_heartbeat(c, c->pending_read_count);
        return;
    }
}

/* ---------------------------------------------------------------------------- */
/* Lifecycle                                                                      */
/* ---------------------------------------------------------------------------- */

void fcbus_core_init(fcbus_core_t *c, const fcbus_config_t *cfg) {
    memset(c, 0, sizeof *c);
    c->rom_image = cfg->rom_image;
    reset_link(c);
}

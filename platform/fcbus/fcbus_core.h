/**
 * @file fcbus_core.h
 * @brief Backend-independent FC PICO cartridge bus logic for the tutorial protocol.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This is the protocol half of `tutorial_project/tuto1_hw/sys/rp_system.cpp`, without
 * PIO, DMA, GPIO, serial or the watchdog: the mailbox builder, the receive dispatcher,
 * the frame-sync decision, bulk data mode and the version/ROM responses. It has no I/O
 * and no dynamic allocation; all state lives in a caller-owned ::fcbus_core_t.
 *
 * A backend connects it to a bus. `fcbus_host.h` simulates the PPU and the 6502 in
 * the same process for tests and emulators; a device backend would sample the PIO read
 * counter, call fcbus_core_rx_byte() for each received byte, and drive the DMA from the
 * actions the core queues. The core is not thread-safe: a backend that receives bytes
 * in an interrupt must serialise access with the application.
 *
 * Mapping to the tutorial (rp_system::):
 *
 * | rp_system                  | fcbus_core                                   |
 * |----------------------------|----------------------------------------------|
 * | init2(), initFC_COM_BUF()  | fcbus_core_init(), FP_COM_INI handling       |
 * | setPF_COM(), setPF_VRAM()  | fcbus_core_cmd(), fcbus_core_cmd_vram()      |
 * | setAPU(), setPF_APU()      | fcbus_core_apu_write(), fcbus_core_apu_flush() |
 * | setPalData(), setPal()     | fcbus_core_set_palette(), fcbus_core_set_pal() |
 * | setAtrData(), clearAtrData(), setAtr() | fcbus_core_set_attr_table(), fcbus_core_clear_attr(), fcbus_core_set_attr() |
 * | update() (table diff)      | fcbus_core_update_tables()                   |
 * | convVram() buffer swap     | fcbus_core_stream_back(), fcbus_core_publish() |
 * | jobRcvCom()                | fcbus_core_rx_byte(), fcbus_core_rx_idle()   |
 * | ppu_dma()                  | fcbus_core_heartbeat()                       |
 * | ver_dma(), rom_dma(), drq_ret(), jobFP_COM_DLD() | FCBUS_ACT_STREAM_RESPONSE |
 * | startDataMode(), setFcStep(), jobFP_COM_DRQ() | fcbus_core_request_data_mode(), fcbus_core_set_step() |
 */
#ifndef FCBUS_CORE_H
#define FCBUS_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fcbus_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Depth of the pending-action ring fcbus_core_pop_action() drains. */
#define FCBUS_ACTION_RING_SIZE 8

/** Bytes an FP_COM_LOG burst may carry (`SysPico.asm` sends at most seven). */
#define FCBUS_LOG_MAX 7

/** Link state. */
typedef enum {
    FCBUS_ST_IDLE, /**< After init, FP_COM_INI or FP_COM_RST, until the first heartbeat. */
    FCBUS_ST_RUN,  /**< Heartbeats are arriving. */
    FCBUS_ST_DATA, /**< Bulk data mode: between the first FP_COM_DRQ and its final reply. */
} fcbus_state_t;

/** Frame-sync decision: rp_system::ppu_dma()'s count test and its manual
 *  `out pins, 8` nudges when the count is one or two short. */
typedef enum {
    FCBUS_ARM,        /**< Stream the front buffer from byte 0. */
    FCBUS_ARM_NUDGE1, /**< Count one short: shift one byte out before streaming. */
    FCBUS_ARM_NUDGE2, /**< Count two short: shift two bytes out before streaming. */
    FCBUS_STOP,       /**< Count outside the window: stop the DMA. */
} fcbus_sync_t;

/** Configuration supplied to fcbus_core_init(). */
typedef struct {
    const uint8_t *rom_image; /**< 32 KB boot ROM PRG (no iNES header) served by FP_COM_VER/ROM. */
} fcbus_config_t;

/** Counters for diagnostics. They do not affect behaviour. */
typedef struct {
    uint32_t frames;        /**< Heartbeats seen. */
    uint32_t dma_stops;     /**< Heartbeats that resolved to #FCBUS_STOP. */
    uint32_t resyncs;       /**< #FCBUS_STOP immediately followed by an ARM decision. */
    uint32_t last_count;    /**< Read count reported at the most recent heartbeat. */
    uint16_t count_hist[9]; /**< Index 4 = expected; 0 = expected-4 or less, 8 = expected+4 or more. */
    uint32_t init_events;   /**< FP_COM_INI packets received. */
    uint8_t last_init_stage; /**< Stage byte of the most recent FP_COM_INI. */
} fcbus_stats_t;

/** What the backend must do after a byte or a heartbeat; see fcbus_core_pop_action(). */
typedef enum {
    /** Serve `data[0..len)` as the next PPU reads, then filler (0xFF) until the next
     *  heartbeat. Covers the FP_COM_VER, FP_COM_ROM, FP_COM_DRQ and FP_COM_DLD replies,
     *  which rp_system answers by restarting the DMA on another buffer. */
    FCBUS_ACT_STREAM_RESPONSE,
    /** A heartbeat resolved to an ARM variant (`arg` is the ::fcbus_sync_t): restart the
     *  stream from byte 0 of fcbus_core_stream_front(), after any nudges. */
    FCBUS_ACT_HEARTBEAT,
    /** A heartbeat resolved to #FCBUS_STOP: stop the DMA. */
    FCBUS_ACT_STOP_DMA,
    /** FP_COM_INI received; `arg` is the stage byte. The application restarts its step
     *  (rp_system calls `ap.setStep(ST_INIT)`). */
    FCBUS_ACT_INIT,
    /** FP_COM_RST received: restart the cartridge firmware (rp_system::soft_reset()). */
    FCBUS_ACT_RESET,
    /** An FP_COM_LOG burst ended; `data[0..len)` holds its bytes. */
    FCBUS_ACT_LOG,
} fcbus_action_kind_t;

/** One queued action. */
typedef struct {
    fcbus_action_kind_t kind;
    uint32_t arg;
    /** NULL unless STREAM_RESPONSE or LOG. Points into the core or the ROM image and
     *  stays valid until the next byte is fed to the core. */
    const uint8_t *data;
    uint16_t len;
} fcbus_action_t;

/** @internal What fcbus_core_rx_byte() is waiting for. */
typedef enum {
    FCBUS_RXW_NONE = 0,
    FCBUS_RXW_ROM_PAGE,
    FCBUS_RXW_DLD_PAGE,
    FCBUS_RXW_INI_STAGE,
    FCBUS_RXW_LOG,
} fcbus_rxwait_t;

/**
 * @brief All state for one cartridge bus link. Embed by value; no allocation.
 *
 * Reading fields directly is fine (tests do). Change them only through the functions
 * below, which keep the cursors and the action ring consistent.
 */
typedef struct fcbus_core {
    const uint8_t *rom_image;

    fcbus_state_t state;
    fcbus_rxwait_t rxwait;

    /* rp_system::vram_buf0 / vram_buf1 */
    uint32_t stream[2][VRAM_BUF_BYTES / sizeof(uint32_t)];
    int front;            /**< Index of the buffer being streamed. */
    bool publish_pending; /**< Back buffer published; swapped in at the next ARM heartbeat. */

    /* rp_system::FC_COM_BUF / m_FC_COM_IDX */
    uint8_t mailbox[FC_COM_BUF_SIZE];
    uint8_t cmd_cursor;

    /* rp_system::m_APU_REG / m_APU_DAT ring */
    uint8_t apu_reg[PICO_APU_BUF_SIZE];
    uint8_t apu_dat[PICO_APU_BUF_SIZE];
    uint8_t apu_w;
    uint8_t apu_r;

    /* rp_system::m_PAL_W / m_PAL_W_old / m_ATR_W / m_ATR_W_old and change flags */
    uint8_t pal_want[FCBUS_PAL_LEN];
    uint8_t pal_sent[FCBUS_PAL_LEN];
    uint8_t attr_want[FCBUS_ATTR_LEN];
    uint8_t attr_sent[FCBUS_ATTR_LEN];
    bool pal_chg;
    bool attr_chg;

    /* rp_system::m_key_imp */
    uint8_t pad;

    /* rp_system::m_waitFP_COM_DRQ, m_FC_STEP, m_pDRQ */
    bool data_mode_requested;
    uint8_t step;
    const uint8_t *drq_source;
    uint16_t drq_source_len;

    /* FP_COM_LOG burst */
    uint8_t log_buf[FCBUS_LOG_MAX];
    uint8_t log_len;

    /* heartbeat */
    uint32_t pending_read_count;
    bool stopped_last_heartbeat;

    fcbus_stats_t stats;

    /* response buffers referenced by queued actions */
    uint8_t ver_buf[8 + 16];
    uint8_t drq_buf[8];
    uint8_t dld_buf[256];

    fcbus_action_t action_ring[FCBUS_ACTION_RING_SIZE];
    uint8_t action_head, action_tail, action_count;
} fcbus_core_t;

/* --------------------------------------------------------------------------------- */
/* Lifecycle                                                                         */
/* --------------------------------------------------------------------------------- */

/** Zeroes `*c`, applies `cfg` and resets the link like rp_system::init2(). */
void fcbus_core_init(fcbus_core_t *c, const fcbus_config_t *cfg);

/* --------------------------------------------------------------------------------- */
/* Picture buffers                                                                   */
/* --------------------------------------------------------------------------------- */

/** The buffer the application converts the next picture into (#VRAM_BUF_BYTES bytes). */
uint16_t *fcbus_core_stream_back(fcbus_core_t *c);

/** The buffer currently streamed to the PPU. */
const uint16_t *fcbus_core_stream_front(const fcbus_core_t *c);

/** Word the PPU reads for 8 pixels of `line` (0..239) at tile column `tile` (0..31): the
 *  stream is flat, 32 selected words per visible line from word #VRAM_HEAD_WORDS.
 *  rp_system::convVram() fills it from a flat 256-pixel-wide bitmap in batches of
 *  #VRAM_LINE_WORDS words, which therefore is not the line stride. */
static inline uint16_t *fcbus_stream_word(uint16_t *buf, int line, int tile) {
    return buf + VRAM_HEAD_WORDS + line * VRAM_TILE_COLS + tile;
}

/** False while a published picture is waiting for the next heartbeat; the back buffer
 *  must not be written until then. */
bool fcbus_core_back_is_free(const fcbus_core_t *c);

/** Hands the back buffer over: it becomes the front buffer at the next ARM heartbeat. */
void fcbus_core_publish(fcbus_core_t *c);

/* --------------------------------------------------------------------------------- */
/* Mailbox builder                                                                   */
/* --------------------------------------------------------------------------------- */

/** The mailbox that will be appended to the next frame (#FC_COM_BUF_SIZE bytes). */
const uint8_t *fcbus_core_mailbox(const fcbus_core_t *c);

/** Appends a PF_COM_* byte. Returns true if it fit. (rp_system::setPF_COM() returns
 *  the opposite: true on overflow.) */
bool fcbus_core_cmd(fcbus_core_t *c, uint8_t com);

/** Appends a PF_COM_VRAM poke (`0xC0|adrH`, `adrL`, `data`); `addr` is masked to 14 bits.
 *  Returns true if it fit. */
bool fcbus_core_cmd_vram(fcbus_core_t *c, uint16_t addr, uint8_t val);

/** Queues one APU register write in the #PICO_APU_BUF_SIZE-entry ring (rp_system::setAPU()).
 *  Like the tutorial, the ring does not check for overflow. */
void fcbus_core_apu_write(fcbus_core_t *c, uint8_t reg, uint8_t val);

/** Moves queued APU writes into the mailbox, up to #APU_PAIRS_MAX pairs, and writes the
 *  0xFF terminator (rp_system::setPF_APU()). Writes that do not fit stay queued for the
 *  next mailbox. Call once per frame after the heartbeat, as the tutorial's sound job does. */
void fcbus_core_apu_flush(fcbus_core_t *c);

/* --------------------------------------------------------------------------------- */
/* Palette and attribute table                                                       */
/* --------------------------------------------------------------------------------- */

/** Replaces the 32-byte palette and marks it for the next bulk upload, without per-frame
 *  pokes (rp_system::setPalData()). */
void fcbus_core_set_palette(fcbus_core_t *c, const uint8_t pal[FCBUS_PAL_LEN]);

/** Changes one palette entry (rp_system::setPal()); it reaches the console through
 *  fcbus_core_update_tables() or the next bulk upload. */
void fcbus_core_set_pal(fcbus_core_t *c, uint8_t idx, uint8_t val);

/** Replaces the attribute table and marks it for the next bulk upload
 *  (rp_system::setAtrData()). */
void fcbus_core_set_attr_table(fcbus_core_t *c, const uint8_t attr[FCBUS_ATTR_LEN]);

/** Zeroes the attribute table and marks it for the next bulk upload
 *  (rp_system::clearAtrData()). */
void fcbus_core_clear_attr(fcbus_core_t *c);

/** Sets the 2-bit palette of 16x16 block (`lx`, `ly`) (rp_system::setAtr()). */
void fcbus_core_set_attr(fcbus_core_t *c, uint8_t lx, uint8_t ly, uint8_t val);

/** Queues PF_COM_VRAM pokes for palette, then attribute bytes that differ from what the
 *  console holds, stopping when the command area is full (the table part of
 *  rp_system::update()). Call once per frame. */
void fcbus_core_update_tables(fcbus_core_t *c);

/* --------------------------------------------------------------------------------- */
/* Bulk data mode                                                                    */
/* --------------------------------------------------------------------------------- */

/** Requests bulk data mode: PF_COM_DMOD is added to every mailbox until FP_COM_DRQ
 *  arrives. This is rp_system::startDataMode() without its blocking retry loop. */
void fcbus_core_request_data_mode(fcbus_core_t *c);

/** Sets the step sent as PF_DAT_STEP when data mode ends (rp_system::setFcStep()). */
void fcbus_core_set_step(fcbus_core_t *c, uint8_t step);

/* --------------------------------------------------------------------------------- */
/* Receive and frame sync                                                            */
/* --------------------------------------------------------------------------------- */

/** Records the PIO read count to use if the next byte is a heartbeat (the device reads
 *  `SM_TRCNT`; the host backend counts served bytes). */
void fcbus_core_set_read_count(fcbus_core_t *c, uint32_t count);

/** True if byte `b`, received now, would be a heartbeat (a controller byte). */
bool fcbus_core_rx_will_heartbeat(const fcbus_core_t *c, uint8_t b);

/** Dispatches one byte received from the console (rp_system::jobRcvCom()). */
void fcbus_core_rx_byte(fcbus_core_t *c, uint8_t b);

/** The receive FIFO is empty: ends an FP_COM_LOG burst shorter than #FCBUS_LOG_MAX. */
void fcbus_core_rx_idle(fcbus_core_t *c);

/** Pops the oldest pending action. Returns false if none. */
bool fcbus_core_pop_action(fcbus_core_t *c, fcbus_action_t *out);

/** The pure count test of rp_system::ppu_dma(). */
fcbus_sync_t fcbus_sync_decide(uint32_t count, uint32_t expected);

/**
 * @brief One heartbeat (rp_system::ppu_dma()) for read count `count`.
 *
 * On an ARM decision: swaps in a published buffer, copies the mailbox to the front
 * buffer at #VRAM_MAILBOX_OFF, resets the mailbox and queues #FCBUS_ACT_HEARTBEAT.
 * On #FCBUS_STOP: queues #FCBUS_ACT_STOP_DMA and keeps the mailbox and any published
 * buffer for the next heartbeat. fcbus_core_rx_byte() calls this for controller bytes.
 */
fcbus_sync_t fcbus_core_heartbeat(fcbus_core_t *c, uint32_t count);

/** The last controller byte (rp_system::m_key_imp). */
uint8_t fcbus_core_pad(const fcbus_core_t *c);

#ifdef __cplusplus
}
#endif

#endif /* FCBUS_CORE_H */

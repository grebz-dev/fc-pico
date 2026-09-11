/*
 * fcbus_core.h -- backend-independent FC PICO cartridge bus logic.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Pure logic, no I/O, no allocation: everything a `fcbus_core_t` needs lives inside the
 * struct the caller provides (device firmware: a static; host/tests: a stack or global
 * variable). Two backends drive the same core:
 *
 *   - the device backend (fcbus_device.c, not part of this task) samples the PIO's
 *     SM_TRCNT read counter and drives the DMA channel from fcbus_core_stream_back()/
 *     the front buffer the way rp_system::ppu_dma() drives `vram_dma`;
 *   - the host backend (fcbus_host.c/.h) simulates the PPU pulling bytes and the 6502
 *     writing them, entirely in this process, for tests and the higher simulation levels
 *     (see doom/plan/09-testing-ci.md).
 *
 * This header mirrors, function by function, `tutorial_project/tuto1_hw/sys/rp_system.cpp`
 * (mailbox builder, rx dispatcher, sync decision) and the 6502 side in
 * `tutorial_project/BOOTROM/{SysPico,SysNMI}.asm` (what the mailbox and opcodes mean).
 * See doom/plan/02-architecture.md ("Buffer ownership", "API sketches") and
 * doom/plan/03-protocol-v2.md ("Firmware-side state machine") for the design this
 * implements.
 *
 * Dependencies: <stdint.h> <stdbool.h> <string.h> <stddef.h> and fcbus_protocol.h only.
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

/* ------------------------------------------------------------------------ */
/* The four-byte OSR prelude (doom/plan/01-constraints.md, "A four-byte prelude   */
/* from the PIO itself")                                                          */
/* ------------------------------------------------------------------------ */

/**
 * @brief Zero bytes the real `fcppu_r` state machine emits after every DMA re-arm,
 *        before the first word autopulled from the buffer reaches the pins.
 *
 * `fcppu_r` starts with `mov osr, null`, and `pio_sm_restart()` (called by
 * `rp_system::ppu_dma()` at every re-arm) clears the OSR and its shift counter again, so
 * the OSR is "full of zeros" at the moment the state machine resumes. The first four
 * `out pins, 8` instructions after a re-arm therefore emit `0x00` from that cleared OSR,
 * not from the DMA buffer -- the buffer's first byte is only reached on the fifth `out`.
 * `rp_system::ppu_dma()`'s two manual `out pins, 8` nudges (issued when the read count is
 * one or two short of #PPU_COUNT_VAL_V1 / #PPU_COUNT_VAL_V2) consume from this same
 * prelude rather than from the buffer, which is why an ARM_NUDGE1/ARM_NUDGE2 heartbeat
 * only needs 3 or 2 further zero bytes, not a fresh 4, before the buffer proper begins.
 *
 * The host backend (fcbus_host.c) models this explicitly: after an ARM it emits
 * `FCBUS_OSR_PRELUDE_BYTES` zero bytes, after ARM_NUDGE1 it emits
 * `FCBUS_OSR_PRELUDE_BYTES - 1`, after ARM_NUDGE2 `FCBUS_OSR_PRELUDE_BYTES - 2`, then the
 * front stream buffer from byte 0.
 */
#define FCBUS_OSR_PRELUDE_BYTES 4

/** Depth of the pending-command ring `fcbus_core_pop_action()` drains. */
#define FCBUS_ACTION_RING_SIZE 8

/** Bytes an #FP_COM_LOG burst may carry, per `SysPico.asm`'s `for(j=1;j<8;j++)` loop. */
#define FCBUS_LOG_MAX 7

/* ------------------------------------------------------------------------ */
/* Protocol version and state (doom/plan/02-architecture.md "API sketches")       */
/* ------------------------------------------------------------------------ */

typedef enum {
    FCBUS_PROTO_UNKNOWN = 0,
    FCBUS_PROTO_V1 = 1,
    FCBUS_PROTO_V2 = 2,
} fcbus_proto_t;

typedef enum {
    FCBUS_ST_IDLE,
    FCBUS_ST_INIT,
    FCBUS_ST_RUN,
    FCBUS_ST_DATA,
} fcbus_state_t;

/** Frame-sync decision, mirroring `rp_system::ppu_dma()`'s in-phase test and the two
 *  manual `out pins, 8` nudges it issues when the count is one or two short. */
typedef enum {
    FCBUS_ARM,
    FCBUS_ARM_NUDGE1,
    FCBUS_ARM_NUDGE2,
    FCBUS_STOP,
} fcbus_sync_t;

/** Configuration supplied once, at fcbus_core_init() / fcbus_host_init() time. */
typedef struct {
    const uint8_t *rom_image; /**< 32 KB PRG served to FP_COM_VER / FP_COM_ROM, no iNES header. */
    fcbus_proto_t proto_default; /**< What to assume before the first packet (normally V1). */
} fcbus_config_t;

/** Counters mirroring doom/plan/02-architecture.md's `fcbus_stats_t` sketch. */
typedef struct {
    uint32_t frames;
    uint32_t resyncs;   /**< A #FCBUS_STOP immediately followed by an ARM decision. */
    uint32_t dma_stops;
    uint32_t hb_timeouts;
    uint32_t torn;      /**< Reserved for parity with the plan's sketch: the magic-byte
                          *   check this would count runs on the 6502, not observable
                          *   from the cartridge side, so this core never increments it. */
    uint32_t last_count;
    uint16_t count_hist[9]; /**< index 4 = expected; index 0 = expected-4, 8 = expected+4. */
    uint32_t isr_max_us; /**< Device-backend only; this core has no clock, always 0 here. */
    uint32_t proto_errors; /**< v2 addition beyond the plan's original sketch: see
                             *   fcbus_core_rx_byte()'s FCBUS_ACT_PROTO_ERROR cases. */
} fcbus_stats_t;

/* ------------------------------------------------------------------------ */
/* Actions: what the backend must do as a result of processing one byte,         */
/* one heartbeat, or one tick. Pulled out with fcbus_core_pop_action().          */
/* ------------------------------------------------------------------------ */

typedef enum {
    /** `data[0..len)` are the next bytes the PPU should read, in order; once exhausted
     *  the backend serves filler (0xFF) until the next heartbeat re-arms streaming from
     *  the front buffer. Covers FP_COM_VER/ROM/DRQ/DLD replies -- everything
     *  rp_system answers by loading the TX FIFO instead of DMA-ing the picture buffer. */
    FCBUS_ACT_STREAM_RESPONSE,
    /** A heartbeat resolved to one of the ARM variants; `arg` is the fcbus_sync_t value.
     *  Mailbox already copied to the front buffer and swap already applied by the time
     *  this is queued -- see fcbus_core_heartbeat(). */
    FCBUS_ACT_HEARTBEAT,
    /** A heartbeat resolved to FCBUS_STOP, or the 2-second heartbeat timeout fired. */
    FCBUS_ACT_STOP_DMA,
    /** FP_COM_INI was received; `arg` is the stage byte. */
    FCBUS_ACT_INIT,
    /** FP_COM_RST was received. */
    FCBUS_ACT_RESET,
    /** An FP_COM_LOG burst ended (7 bytes collected, or fcbus_core_rx_idle() called);
     *  `data[0..len)` is the burst, len in 0..FCBUS_LOG_MAX. */
    FCBUS_ACT_LOG,
    /** A byte could not be interpreted under the current protocol; `arg` is that byte. */
    FCBUS_ACT_PROTO_ERROR,
} fcbus_action_kind_t;

typedef struct {
    fcbus_action_kind_t kind;
    uint32_t arg;
    const uint8_t *data; /**< NULL unless kind is STREAM_RESPONSE or LOG. Points into
                           *   storage owned by the fcbus_core_t (rom_image, or a scratch
                           *   buffer inside it) that stays valid until the *next* byte is
                           *   fed to the core -- consume it before then. */
    uint16_t len;
} fcbus_action_t;

/* ------------------------------------------------------------------------ */
/* The core struct itself.                                                       */
/* ------------------------------------------------------------------------ */

/** Internal: what fcbus_core_rx_byte() is waiting for before it can act. */
typedef enum {
    FCBUS_RXW_NONE = 0,
    FCBUS_RXW_ROM_PAGE,
    FCBUS_RXW_DLD_PAGE,
    FCBUS_RXW_INI_STAGE,
    FCBUS_RXW_HELLO_VER,
    FCBUS_RXW_KEY_PAD1,
    FCBUS_RXW_KEY_PAD2,
    FCBUS_RXW_LOG,
} fcbus_rxwait_t;

/**
 * @brief All state for one cartridge bus link. Allocation-free: embed by value.
 *
 * Fields are grouped by the tutorial concept they mirror. Reading fields directly
 * (`core.state`, `core.stats.frames`, ...) is fine and expected from tests; mutate only
 * through the functions below so the invariants they keep (mailbox cursors, the
 * publish-pending flag, the action ring) stay consistent.
 */
typedef struct fcbus_core {
    /* --- configuration (fcbus_core_init) --- */
    const uint8_t *rom_image;
    fcbus_proto_t proto_default;

    /* --- protocol / state machine (doom/plan/03-protocol-v2.md) --- */
    fcbus_proto_t proto;
    fcbus_state_t state;
    fcbus_rxwait_t rxwait;

    /* --- stream buffers: rp_system::vram_buf0/vram_buf1, always sized for v2 --- */
    uint32_t stream[2][VRAM_BUF_BYTES_V2 / sizeof(uint32_t)];
    int front;               /**< Index (0/1) of the buffer currently "streamed". */
    bool publish_pending;    /**< fcbus_core_publish() was called; swap at the next ARM. */

    /* --- mailbox being built for the next heartbeat: rp_system::FC_COM_BUF --- */
    uint8_t mailbox_next[FC_COM_BUF_SIZE_V2];
    uint8_t cmd_cursor;  /**< Next free command byte; mirrors m_FC_COM_IDX. */
    uint8_t apu_cursor;  /**< Next free APU-pair byte, at or after MBX_APU. */
    uint8_t apu_pairs;   /**< Pairs queued so far this mailbox, for the per-proto cap. */

    /* --- attribute table and palette: rp_system's m_ATR_W / m_ATR_W_old pair --- */
    /* Two copies, exactly as the tutorial keeps them, and for the same reason:
     * `*_want` is what the application has asked the console to display, `*_sent` is
     * what the console is believed to hold already. rp_system::update() diffs the two
     * and emits PF_COM_VRAM pokes for the difference, while bulk data mode uploads
     * `*_want` wholesale (jobFP_COM_DRQ() points m_pDRQ at m_PAL_W / m_ATR_W, not at
     * the *_old copies). Collapsing them into one array would make a data-mode upload
     * send whatever the trickle of pokes had managed to sync so far, which is wrong
     * precisely when the upload matters most -- the first frame after a scene change.
     *
     * The v1 command area holds at most four pokes per frame (3 bytes each in 14), so a
     * full 64-byte attribute change takes 16 frames to trickle through in v1; that is
     * the limitation protocol v2's whole-table mailbox field exists to remove
     * (doom/plan/03-protocol-v2.md) and the reason data mode exists for bulk changes.
     *
     * fcbus_core_palette() only ever supplies the 16 BG bytes the v2 mailbox carries, so
     * pal_want[16..31] (the sprite palette, which the tutorial's 32-byte $3F00 upload
     * also covers) stays zero: there is no API surface here that carries sprite palette
     * data, and Doom has no sprites to colour. */
    uint8_t attr_want[MBX_ATTR_LEN];
    uint8_t attr_sent[MBX_ATTR_LEN];
    uint8_t pal_want[32];
    uint8_t pal_sent[32];

    /* --- controller latch: rp_system::m_key_imp (v1 single byte) / FP_COM_KEY pair --- */
    uint8_t pad1;
    uint8_t pad2;

    /* --- data mode: rp_system::m_waitFP_COM_DRQ, m_PAL_CHG, m_ATR_CHG, m_FC_STEP --- */
    bool data_mode_requested;
    bool dm_pal_pending;
    bool dm_attr_pending;
    uint8_t dm_step;

    /* --- FP_COM_LOG burst collection --- */
    uint8_t log_buf[FCBUS_LOG_MAX];
    uint8_t log_len;

    /* --- heartbeat bookkeeping --- */
    uint32_t pending_read_count; /**< Set by the backend via fcbus_core_set_read_count()
                                   *   before a byte that may turn out to be a heartbeat
                                   *   trigger; consumed by fcbus_core_rx_byte(). */
    uint32_t frame_no;
    bool stopped_last_heartbeat; /**< For resync counting: a STOP followed by an ARM. */
    bool heartbeat_seen;         /**< Since the last fcbus_core_tick_ms() call. */
    bool have_heartbeat_ms;
    uint32_t last_heartbeat_ms;

    fcbus_stats_t stats;

    /* --- scratch response buffers, referenced by queued FCBUS_ACT_STREAM_RESPONSE
     *     actions; sized for the largest reply this core ever produces on its own
     *     (FP_COM_ROM pages point straight into rom_image and need no scratch space) --- */
    uint8_t ver_scratch[8 + 16];  /**< Sync word + boot-ROM stamp (ver_dma()). */
    uint8_t drq_header[8];        /**< drq_ret()'s two little-endian words. */
    uint8_t data_payload[256];    /**< Current data-mode payload, zero-padded to a page. */

    /* --- pending action ring --- */
    fcbus_action_t action_ring[FCBUS_ACTION_RING_SIZE];
    uint8_t action_head, action_tail, action_count;
} fcbus_core_t;

/* ------------------------------------------------------------------------ */
/* Lifecycle                                                                      */
/* ------------------------------------------------------------------------ */

/** Zeroes *c and applies cfg. Mirrors rp_system::init2() (soft reset, no hardware). */
void fcbus_core_init(fcbus_core_t *c, const fcbus_config_t *cfg);

/* ------------------------------------------------------------------------ */
/* Stream and mailbox: the converter writes the back buffer, then publishes.     */
/* Mirrors rp_system::vram_bufDraw / convVram() / FC_COM_BUF.                    */
/* ------------------------------------------------------------------------ */

/** VRAM_BUF_BYTES_V2 bytes, word-addressed: the buffer the converter is filling. */
uint16_t *fcbus_core_stream_back(fcbus_core_t *c);

/** The buffer currently being streamed to the PPU (read-only from the caller's side;
 *  exposed so the host backend can serve bytes from it). */
const uint16_t *fcbus_core_stream_front(const fcbus_core_t *c);

/** FC_COM_BUF_SIZE_V2 bytes: the mailbox being built for the *next* heartbeat. */
uint8_t *fcbus_core_mailbox_next(fcbus_core_t *c);

static inline uint16_t *fcbus_stream_word(uint16_t *buf, int line, int tile) {
    return buf + VRAM_HEAD_WORDS + line * VRAM_LINE_WORDS + tile;
}

/** False while a publish is pending (the back buffer is not free to reuse). */
bool fcbus_core_back_is_free(const fcbus_core_t *c);

/** Marks the back buffer as "swap in at the next heartbeat". Mirrors the swap
 *  convVram() used to do inline, moved to the heartbeat so the swap only ever happens
 *  where the picture is not mid-stream (doom/plan/02-architecture.md, "Buffer ownership"). */
void fcbus_core_publish(fcbus_core_t *c);

/* ------------------------------------------------------------------------ */
/* Mailbox builder: rp_system::setPF_COM / setPF_VRAM / setPF_APU / update().     */
/* ------------------------------------------------------------------------ */

/** Appends a PF_COM_* byte. Returns true if there was room (cursor < FC_COM_BUF_SIZE16),
 *  false if the command area is full -- note this is the opposite sense of
 *  rp_system::setPF_COM()'s bool, which returns true on overflow. */
bool fcbus_core_cmd(fcbus_core_t *c, uint8_t com);

/** Appends a PF_COM_VRAM poke (3 bytes: opcode|adrH, adrL, data). Mirrors
 *  rp_system::setPF_VRAM(): addr is masked to 14 bits first. Returns false if fewer than
 *  3 bytes remain in the command area. */
bool fcbus_core_cmd_vram(fcbus_core_t *c, uint16_t addr, uint8_t val);

/** Appends one (reg, value) pair, keeping the 0xFF terminator rp_system::setPF_APU()
 *  writes. Capped at APU_PAIRS_MAX_V1 pairs in v1 (and while proto is still unknown) or
 *  APU_PAIRS_MAX_V2 in v2; sets MBX_FLAG_APU_VALID in v2. False when the cap is reached. */
bool fcbus_core_apu_write(fcbus_core_t *c, uint8_t reg, uint8_t val);

/** v2: copies the 64-byte attribute table into MBX_ATTR and sets MBX_FLAG_ATTR_VALID.
 *  v1 (and unknown proto): diffs against attr_sent and queues fcbus_core_cmd_vram()
 *  pokes at $23C0+i for the bytes that changed, exactly like rp_system::update()'s
 *  attribute loop -- stopping (and leaving the remaining diffs for the next call) if the
 *  command area fills, which in v1 it will after four bytes. Either way the requested
 *  table is remembered in attr_want, which is what a bulk data-mode upload sends. */
void fcbus_core_attr_table(fcbus_core_t *c, const uint8_t attr[64]);

/** As fcbus_core_attr_table(), but for the 16-byte BG palette (MBX_PAL / $3F00-$3F0F). */
void fcbus_core_palette(fcbus_core_t *c, const uint8_t pal[16]);

/* ------------------------------------------------------------------------ */
/* Receive dispatcher: rp_system::jobRcvCom() / getRcvCom().                     */
/* ------------------------------------------------------------------------ */

/** Backend hook: record the qualifying-read count for the frame that just ended. The
 *  device backend reads this from the SM_TRCNT PIO counter; the host backend counts
 *  bytes served by fcbus_host_ppu_read(). fcbus_core_rx_byte() uses the most recently
 *  recorded value when a byte turns out to be a heartbeat trigger (a raw v1 byte, or the
 *  second byte of an FP_COM_KEY packet) and calls fcbus_core_heartbeat() with it --
 *  mirroring how rp_system::jobRcvCom()'s default case samples SM_TRCNT synchronously,
 *  inside the very interrupt that received the byte. */
void fcbus_core_set_read_count(fcbus_core_t *c, uint32_t count);

/** Dispatches one byte received from the 6502. See doom/plan/03-protocol-v2.md,
 *  "Firmware-side state machine", for the full rule set this implements. */
void fcbus_core_rx_byte(fcbus_core_t *c, uint8_t b);

/** Call when the receive FIFO goes empty / a write burst ends. Only meaningful mid an
 *  FP_COM_LOG burst shorter than FCBUS_LOG_MAX bytes: flushes it as FCBUS_ACT_LOG. */
void fcbus_core_rx_idle(fcbus_core_t *c);

/** Pops one pending action, oldest first. Returns false if none are pending. */
bool fcbus_core_pop_action(fcbus_core_t *c, fcbus_action_t *out);

/* ------------------------------------------------------------------------ */
/* Frame sync: rp_system::ppu_dma().                                             */
/* ------------------------------------------------------------------------ */

/** Pure decision table, no side effects: FCBUS_STOP if |count-expected| exceeds
 *  PPU_COUNT_WINDOW; FCBUS_ARM_NUDGE2 / _NUDGE1 if count is 2 / 1 short; else FCBUS_ARM. */
fcbus_sync_t fcbus_sync_decide(uint32_t count, uint32_t expected);

/**
 * @brief Runs one heartbeat: the frame-sync decision plus everything rp_system::ppu_dma()
 *        does when it fires.
 *
 * `expected` is chosen from the current protocol (#PPU_COUNT_VAL_V2 for V2, else
 * #PPU_COUNT_VAL_V1 -- V1 is also used while proto is still #FCBUS_PROTO_UNKNOWN). On any
 * ARM variant: swaps front/back if fcbus_core_publish() left a swap pending, copies
 * mailbox_next onto the (possibly just-swapped) front buffer's tail at
 * VRAM_MAILBOX_OFF_V1/V2, then resets mailbox_next for the next frame. On FCBUS_STOP,
 * neither the swap nor the copy happens. Always: state IDLE/INIT -> RUN, frame_no++,
 * stats updated (frames, last_count, count_hist, dma_stops, resyncs), and one
 * FCBUS_ACT_HEARTBEAT or FCBUS_ACT_STOP_DMA action queued.
 */
fcbus_sync_t fcbus_core_heartbeat(fcbus_core_t *c, uint32_t count);

/** (pad2 << 8) | pad1. */
uint16_t fcbus_core_pads(const fcbus_core_t *c);

/** Heartbeat-loss watchdog: if in FCBUS_ST_RUN and more than 2000 ms have passed since
 *  the last heartbeat, moves to FCBUS_ST_INIT, counts stats.hb_timeouts and queues
 *  FCBUS_ACT_STOP_DMA. The device backend calls this from a periodic timer; the host
 *  backend from fcbus_host_tick_ms(). */
void fcbus_core_tick_ms(fcbus_core_t *c, uint32_t now_ms);

/* ------------------------------------------------------------------------ */
/* Bulk data mode: rp_system::startDataMode() / jobFP_COM_DRQ() / setFcStep().    */
/* ------------------------------------------------------------------------ */

/** Requests bulk data mode: queues PF_COM_DMOD into every mailbox built from now on
 *  (mirroring startDataMode()'s retry loop, but non-blocking -- one attempt per
 *  heartbeat rather than one attempt every 50 ms) until FP_COM_DRQ arrives and the
 *  state becomes FCBUS_ST_DATA. Also marks the shadowed palette and attribute table as
 *  pending upload, so the DRQ sequence has something to serve (see fcbus_core.c). */
void fcbus_core_request_data_mode(fcbus_core_t *c);

/** Queues a step change, delivered as the PF_DAT_STEP data-mode response once the
 *  pending palette/attribute uploads (if any) have been served. Mirrors setFcStep(). */
void fcbus_core_set_step(fcbus_core_t *c, uint8_t step);

/** Read-only stats access, for API parity with doom/plan/02-architecture.md's
 *  fcbus_stats(); reading `c->stats` directly works just as well. */
const fcbus_stats_t *fcbus_core_stats(const fcbus_core_t *c);

#ifdef __cplusplus
}
#endif

#endif /* FCBUS_CORE_H */

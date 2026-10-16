/**
 * @file fcbus_host.h
 * @brief Host backend for fcbus_core: a byte-level model of the PPU bus.
 * @ingroup fcbus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Replaces PIO and DMA with function calls: fcbus_host_ppu_read() is one qualifying
 * PPU pattern fetch, fcbus_host_ppu_write() is one 6502 write to `$2007`. Use it for
 * tests and emulators. Each ::fcbus_host_t owns its own ::fcbus_core_t.
 */
#ifndef FCBUS_HOST_H
#define FCBUS_HOST_H

#include <stddef.h>
#include <stdint.h>

#include "fcbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Capacity of the observable action log. */
#define FCBUS_HOST_LOG_CAP 32

/** An action the host backend does not consume itself (INIT, RESET, LOG). */
typedef struct {
    fcbus_action_kind_t kind;
    uint32_t arg;
    uint8_t data[FCBUS_LOG_MAX];
    uint16_t len;
} fcbus_host_log_entry_t;

/** @internal What fcbus_host_ppu_read() serves next. */
typedef enum {
    FCBUS_HOST_STOPPED,  /**< DMA stopped. */
    FCBUS_HOST_RESPONSE, /**< Bytes of a queued FCBUS_ACT_STREAM_RESPONSE. */
    FCBUS_HOST_FILLER,   /**< Response exhausted: 0xFF until the next heartbeat. */
    FCBUS_HOST_STREAM,   /**< The front picture buffer. */
} fcbus_host_mode_t;

/** One simulated cartridge bus. Embed by value; no allocation. */
typedef struct {
    fcbus_core_t core;
    uint32_t read_count;
    fcbus_host_mode_t mode;
    const uint8_t *resp;
    uint16_t resp_len;
    uint16_t resp_pos;
    uint32_t stream_pos;
    fcbus_host_log_entry_t log[FCBUS_HOST_LOG_CAP];
    size_t log_len;
} fcbus_host_t;

/** Initialises `h` and its core. The DMA starts stopped, as after rp_system::init(). */
void fcbus_host_init(fcbus_host_t *h, const fcbus_config_t *cfg);

/**
 * @brief One qualifying PPU read: the byte on the bus, or -1 while the DMA is stopped.
 *
 * Every read is counted, including reads while stopped: the PIO counter keeps counting
 * then too, which is how the link regains phase.
 */
int fcbus_host_ppu_read(fcbus_host_t *h);

/** One `$2007` write from the console. Reports the read count the way `fcppu_rna`
 *  does (the zero-based index of the last read), dispatches the byte and applies
 *  the resulting actions. */
void fcbus_host_ppu_write(fcbus_host_t *h, uint8_t b);

/** Writes a burst, then signals that the receive FIFO is empty. */
void fcbus_host_write_burst(fcbus_host_t *h, const uint8_t *data, size_t len);

/** The core, for the application side of the API. */
fcbus_core_t *fcbus_host_core(fcbus_host_t *h);

/** Logged INIT, RESET and LOG actions, oldest first. */
const fcbus_host_log_entry_t *fcbus_host_log(const fcbus_host_t *h, size_t *count);

/** Empties the action log. */
void fcbus_host_log_clear(fcbus_host_t *h);

#ifdef __cplusplus
}
#endif

#endif /* FCBUS_HOST_H */

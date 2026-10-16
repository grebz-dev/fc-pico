/**
 * @file fcpico_cart.h
 * @brief Host model of an FC PICO cartridge for emulators: ABI version 1.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * An emulator mapper creates one ::fcpico_cart_t per loaded cartridge and forwards
 * each CS1-qualified PPU read and write to it. Inside, the shared bus core
 * (`platform/fcbus`) runs the tutorial protocol, and an application *provider*
 * stands in for the firmware's application code: it is told about `FP_COM_INI`
 * and runs once per heartbeat, like the tutorial's `loop()` running `ap.main()`.
 * After each provider frame the cartridge sends palette/attribute changes and queued
 * APU writes, as `rp_system::update()` and the sound job do.
 *
 * The cartridge does not emulate the RP2350, PIO timing or the bus electrically.
 * Save states are not supported by ABI version 1.
 */
#ifndef FCPICO_CART_H
#define FCPICO_CART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fcbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Version of this header's types and functions. A mapper built against another
 *  version must not use the library; see fcpico_cart_abi_version(). */
#define FCPICO_CART_ABI_VERSION 1

/** Value a CS1-qualified read returns while the cartridge drives nothing. */
#define FCPICO_CART_OPEN_BUS 0xFF

/**
 * @brief Application code running "on" the simulated cartridge.
 *
 * Providers draw into fcbus_core_stream_back() and call fcbus_core_publish(), and
 * use the rest of the fcbus_core API (palette, attributes, commands, APU writes,
 * data mode) exactly as firmware would use rp_system.
 */
typedef struct {
    uint32_t abi_version; /**< #FCPICO_CART_ABI_VERSION. */
    const char *name;
    /** Creates provider state for `core`. `options` is provider-specific text and
     *  may be NULL. Returns NULL on invalid options. */
    void *(*create)(fcbus_core_t *core, const char *options);
    void (*destroy)(void *state);
    /** The console sent FP_COM_INI with `stage` (the core has already reset the link). */
    void (*console_init)(void *state, uint8_t stage);
    /** Once per heartbeat that restarted the stream; `pad` is the controller byte. */
    void (*frame)(void *state, uint8_t pad, uint32_t frame_number);
} fcpico_provider_t;

/** Cartridge configuration. */
typedef struct {
    uint32_t abi_version;          /**< #FCPICO_CART_ABI_VERSION. */
    const fcpico_provider_t *provider;
    const char *provider_options;  /**< Passed to fcpico_provider_t::create. */
    const uint8_t *served_prg;     /**< 32 KiB boot ROM PRG the firmware serves (copied). */
    size_t served_prg_len;         /**< Must be #FCBUS_ROM_PRG_BYTES. */
} fcpico_cart_config_t;

/** Counters the cartridge keeps besides the protocol statistics. */
typedef struct {
    uint64_t ppu_reads;
    uint64_t ppu_writes;
    uint64_t open_bus_reads;  /**< Reads while the DMA was stopped. */
    uint32_t console_inits;   /**< FP_COM_INI packets. */
    uint32_t firmware_resets; /**< FP_COM_RST packets. */
    uint32_t provider_frames;
} fcpico_cart_metrics_t;

typedef struct fcpico_cart fcpico_cart_t;

/** The ABI version this library was built with. */
uint32_t fcpico_cart_abi_version(void);

/** A built-in provider by name ("pattern", "demo"), or NULL. */
const fcpico_provider_t *fcpico_cart_find_provider(const char *name);

/**
 * Creates a cartridge. Returns NULL, and writes a reason to `error` when it is not
 * NULL, for an ABI mismatch, a missing provider, a ROM that is not 32 KiB, or
 * options the provider rejects.
 */
fcpico_cart_t *fcpico_cart_create(const fcpico_cart_config_t *config,
                                  char *error, size_t error_size);

/** Destroys a cartridge and its provider state. NULL is ignored. */
void fcpico_cart_destroy(fcpico_cart_t *cart);

/** Power cycle: the firmware restarts with the same configuration. */
void fcpico_cart_reset(fcpico_cart_t *cart);

/** One CS1-qualified PPU read; #FCPICO_CART_OPEN_BUS while the DMA is stopped. */
uint8_t fcpico_cart_ppu_read(fcpico_cart_t *cart);

/** One CS1-qualified PPU write (a 6502 `$2007` write). */
void fcpico_cart_ppu_write(fcpico_cart_t *cart, uint8_t value);

/** Protocol statistics of the bus core. */
const fcbus_stats_t *fcpico_cart_stats(const fcpico_cart_t *cart);

/** Cartridge counters. */
const fcpico_cart_metrics_t *fcpico_cart_metrics(const fcpico_cart_t *cart);

/** The bus core, for tests and diagnostics. */
const fcbus_core_t *fcpico_cart_core(const fcpico_cart_t *cart);

#ifdef __cplusplus
}
#endif

#endif /* FCPICO_CART_H */

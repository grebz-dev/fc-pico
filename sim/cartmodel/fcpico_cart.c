/**
 * @file fcpico_cart.c
 * @brief Host model of an FC PICO cartridge for emulators.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fcpico_cart.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fcbus_host.h"

struct fcpico_cart {
    fcbus_host_t host;
    const fcpico_provider_t *provider;
    char *provider_options;
    void *provider_state;
    uint8_t prg[FCBUS_ROM_PRG_BYTES];
    fcpico_cart_metrics_t metrics;
};

extern const fcpico_provider_t fcpico_provider_pattern;
extern const fcpico_provider_t fcpico_provider_demo;

uint32_t fcpico_cart_abi_version(void) {
    return FCPICO_CART_ABI_VERSION;
}

const fcpico_provider_t *fcpico_cart_find_provider(const char *name) {
    static const fcpico_provider_t *const providers[] = {
        &fcpico_provider_pattern,
        &fcpico_provider_demo,
    };
    for (size_t i = 0; name != NULL && i < sizeof providers / sizeof providers[0]; i++) {
        if (strcmp(providers[i]->name, name) == 0) {
            return providers[i];
        }
    }
    return NULL;
}

static void fail(char *error, size_t size, const char *format, ...) {
    if (error == NULL || size == 0) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

/* Firmware start: rp_system::init() and the application's setup. */
static bool start_firmware(fcpico_cart_t *cart) {
    if (cart->provider_state != NULL) {
        cart->provider->destroy(cart->provider_state);
    }
    fcbus_config_t bus = {cart->prg};
    fcbus_host_init(&cart->host, &bus);
    cart->provider_state = cart->provider->create(&cart->host.core, cart->provider_options);
    return cart->provider_state != NULL;
}

fcpico_cart_t *fcpico_cart_create(const fcpico_cart_config_t *config,
                                  char *error, size_t error_size) {
    if (config == NULL || config->abi_version != FCPICO_CART_ABI_VERSION) {
        fail(error, error_size, "cartridge ABI version %u requested, library has %u",
             config ? (unsigned)config->abi_version : 0u, (unsigned)FCPICO_CART_ABI_VERSION);
        return NULL;
    }
    const fcpico_provider_t *p = config->provider;
    if (p == NULL || p->abi_version != FCPICO_CART_ABI_VERSION || p->create == NULL ||
        p->destroy == NULL || p->console_init == NULL || p->frame == NULL) {
        fail(error, error_size, "missing or incompatible application provider");
        return NULL;
    }
    if (config->served_prg == NULL || config->served_prg_len != FCBUS_ROM_PRG_BYTES) {
        fail(error, error_size, "served boot ROM must be %u bytes of PRG, got %zu",
             (unsigned)FCBUS_ROM_PRG_BYTES, config->served_prg ? config->served_prg_len : 0);
        return NULL;
    }
    fcpico_cart_t *cart = calloc(1, sizeof *cart);
    if (cart == NULL) {
        fail(error, error_size, "out of memory");
        return NULL;
    }
    cart->provider = p;
    if (config->provider_options != NULL) {
        size_t n = strlen(config->provider_options) + 1;
        cart->provider_options = malloc(n);
        if (cart->provider_options == NULL) {
            free(cart);
            fail(error, error_size, "out of memory");
            return NULL;
        }
        memcpy(cart->provider_options, config->provider_options, n);
    }
    memcpy(cart->prg, config->served_prg, FCBUS_ROM_PRG_BYTES);
    if (!start_firmware(cart)) {
        fail(error, error_size, "provider '%s' rejected options '%s'", p->name,
             config->provider_options ? config->provider_options : "");
        fcpico_cart_destroy(cart);
        return NULL;
    }
    return cart;
}

void fcpico_cart_destroy(fcpico_cart_t *cart) {
    if (cart == NULL) {
        return;
    }
    if (cart->provider_state != NULL) {
        cart->provider->destroy(cart->provider_state);
    }
    free(cart->provider_options);
    free(cart);
}

void fcpico_cart_reset(fcpico_cart_t *cart) {
    memset(&cart->metrics, 0, sizeof cart->metrics);
    (void)start_firmware(cart);
}

uint8_t fcpico_cart_ppu_read(fcpico_cart_t *cart) {
    cart->metrics.ppu_reads++;
    int value = fcbus_host_ppu_read(&cart->host);
    if (value < 0) {
        cart->metrics.open_bus_reads++;
        return FCPICO_CART_OPEN_BUS;
    }
    return (uint8_t)value;
}

void fcpico_cart_ppu_write(fcpico_cart_t *cart, uint8_t value) {
    cart->metrics.ppu_writes++;
    fcbus_core_t *core = &cart->host.core;
    uint32_t frames = core->stats.frames;
    fcbus_host_ppu_write(&cart->host, value);

    size_t count;
    const fcbus_host_log_entry_t *log = fcbus_host_log(&cart->host, &count);
    for (size_t i = 0; i < count; i++) {
        if (log[i].kind == FCBUS_ACT_INIT) {
            cart->metrics.console_inits++;
            cart->provider->console_init(cart->provider_state, (uint8_t)log[i].arg);
        } else if (log[i].kind == FCBUS_ACT_RESET) {
            /* rp_system::soft_reset(): the firmware starts over. */
            cart->metrics.firmware_resets++;
            fcbus_host_log_clear(&cart->host);
            (void)start_firmware(cart);
            return;
        }
    }
    fcbus_host_log_clear(&cart->host);

    if (core->stats.frames != frames) {
        /* The tutorial's loop() runs once per heartbeat: ap.main(), then update();
         * the sound job then moves APU writes into the mailbox. */
        cart->metrics.provider_frames++;
        cart->provider->frame(cart->provider_state, fcbus_core_pad(core), core->stats.frames);
        fcbus_core_update_tables(core);
        fcbus_core_apu_flush(core);
    }
}

const fcbus_stats_t *fcpico_cart_stats(const fcpico_cart_t *cart) {
    return &cart->host.core.stats;
}

const fcpico_cart_metrics_t *fcpico_cart_metrics(const fcpico_cart_t *cart) {
    return &cart->metrics;
}

const fcbus_core_t *fcpico_cart_core(const fcpico_cart_t *cart) {
    return &cart->host.core;
}

/* SPDX-License-Identifier: BSD-3-Clause */
/* The cartridge model driven the way the tutorial boot ROM drives a cartridge. */
#include <stdio.h>
#include <string.h>

#include "ctest_lite.h"
#include "fcpico_cart.h"

static uint8_t prg[FCBUS_ROM_PRG_BYTES];

/* --- a minimal console ---------------------------------------------------- */

typedef struct {
    fcpico_cart_t *cart;
    uint8_t picture[VRAM_MAILBOX_OFF]; /* bytes read before the mailbox */
    uint8_t mailbox[FC_COM_BUF_SIZE];
    uint8_t pal[FCBUS_PAL_LEN];
    uint8_t attr[FCBUS_ATTR_LEN];
    int data_uploads;
} console_t;

static fcpico_cart_t *make(const char *provider, const char *options) {
    fcpico_cart_config_t cfg = {FCPICO_CART_ABI_VERSION, fcpico_cart_find_provider(provider),
                                options, prg, sizeof prg};
    char error[128] = "";
    fcpico_cart_t *cart = fcpico_cart_create(&cfg, error, sizeof error);
    if (cart == NULL) fprintf(stderr, "create failed: %s\n", error);
    return cart;
}

static void open_console(console_t *c, const char *provider, const char *options) {
    memset(c, 0, sizeof *c);
    c->cart = make(provider, options);
}

static uint32_t read_word(fcpico_cart_t *cart) {
    uint32_t w = 0;
    for (int i = 0; i < 4; i++) w |= (uint32_t)fcpico_cart_ppu_read(cart) << (8 * i);
    return w;
}

/* The boot ROM's data-mode loop: FP_COM_DRQ, then FP_COM_DLD per VRAM block. */
static void data_mode(console_t *c) {
    for (;;) {
        fcpico_cart_ppu_write(c->cart, FP_COM_DRQ);
        uint32_t w0 = read_word(c->cart), size = read_word(c->cart);
        uint8_t com = (uint8_t)(w0 - FCBUS_DRQ_MAGIC_BASE);
        uint16_t adr = (uint16_t)(w0 >> 16);
        if (com != PF_DAT_VRAM) return;
        fcpico_cart_ppu_write(c->cart, FP_COM_DLD);
        fcpico_cart_ppu_write(c->cart, 0);
        uint8_t page[256];
        for (int i = 0; i < 256; i++) page[i] = fcpico_cart_ppu_read(c->cart);
        if (adr == 0x3F00) memcpy(c->pal, page, size);
        if (adr == 0x23C0) memcpy(c->attr, page, size);
        c->data_uploads++;
    }
}

/* One frame: the PPU's qualifying reads, then the NMI's controller byte. */
static void frame(console_t *c, uint8_t pad) {
    for (int i = 0; i < VRAM_MAILBOX_OFF; i++) c->picture[i] = fcpico_cart_ppu_read(c->cart);
    for (int i = 0; i < FC_COM_BUF_SIZE; i++) c->mailbox[i] = fcpico_cart_ppu_read(c->cart);
    fcpico_cart_ppu_read(c->cart); /* the NMI's dummy read */
    fcpico_cart_ppu_write(c->cart, pad);
    if (c->mailbox[MBX_MAGIC] == PF_MAGIC_NO) {
        for (int i = MBX_CMD; i < FC_COM_BUF_SIZE16 && c->mailbox[i] != PF_COM_NONE; i++) {
            if (c->mailbox[i] == PF_COM_DMOD) {
                data_mode(c);
                break;
            }
        }
    }
}

static bool mailbox_has(const console_t *c, uint8_t com) {
    for (int i = MBX_CMD; i < FC_COM_BUF_SIZE16 && c->mailbox[i] != PF_COM_NONE; i++) {
        if (c->mailbox[i] == com) return true;
    }
    return false;
}

/* FP_COM_INI, then frames until the link streams cleanly. The data-mode exchange
 * happens between heartbeats, so its reads make the next frame long: that heartbeat
 * stops the DMA and the following one resynchronises (hardware logs show the same
 * startup stop and resync). A PF_COM_DMOD already in the pending mailbox when
 * FP_COM_DRQ arrives goes out once more, as in rp_system, and ends at once. */
static void boot(console_t *c) {
    fcpico_cart_ppu_write(c->cart, FP_COM_INI);
    fcpico_cart_ppu_write(c->cart, 0);
    int clean = 0;
    for (int i = 0; i < 20 && clean < 2; i++) {
        uint32_t stops = fcpico_cart_stats(c->cart)->dma_stops;
        frame(c, 0);
        bool ok = c->mailbox[MBX_MAGIC] == PF_MAGIC_NO && !mailbox_has(c, PF_COM_DMOD) &&
                  fcpico_cart_stats(c->cart)->dma_stops == stops;
        clean = ok ? clean + 1 : 0;
    }
    CHECK_EQ(clean, 2);
}

/* 2-bit pixel (x, y) of the last picture, decoded from the stream bytes. */
static int pixel(const console_t *c, int x, int y) {
    int word = VRAM_HEAD_WORDS + y * VRAM_TILE_COLS + x / 8;
    int bit = 7 - (x & 7);
    return ((c->picture[2 * word] >> bit) & 1) | (((c->picture[2 * word + 1] >> bit) & 1) << 1);
}

/* --- tests ----------------------------------------------------------------- */

static void test_creation_is_validated(void) {
    char error[128];
    fcpico_cart_config_t cfg = {FCPICO_CART_ABI_VERSION + 1, fcpico_cart_find_provider("demo"),
                                NULL, prg, sizeof prg};
    CHECK(fcpico_cart_create(&cfg, error, sizeof error) == NULL);
    CHECK(strstr(error, "ABI") != NULL);
    cfg.abi_version = FCPICO_CART_ABI_VERSION;
    cfg.served_prg_len = 100;
    CHECK(fcpico_cart_create(&cfg, error, sizeof error) == NULL);
    cfg.served_prg_len = sizeof prg;
    cfg.provider = fcpico_cart_find_provider("no-such-provider");
    CHECK(fcpico_cart_create(&cfg, error, sizeof error) == NULL);
    CHECK(make("pattern", "7") == NULL);
    CHECK_EQ(fcpico_cart_abi_version(), FCPICO_CART_ABI_VERSION);
}

static void test_stopped_bus_reads_open_bus(void) {
    fcpico_cart_t *cart = make("pattern", NULL);
    CHECK_EQ(fcpico_cart_ppu_read(cart), FCPICO_CART_OPEN_BUS);
    CHECK_EQ(fcpico_cart_metrics(cart)->open_bus_reads, 1);
    fcpico_cart_destroy(cart);
}

static void test_pattern_boots_through_data_mode(void) {
    console_t c;
    open_console(&c, "pattern", "0");
    boot(&c);
    CHECK_EQ(c.data_uploads, 2);
    CHECK_EQ(c.pal[1], 0x00);
    CHECK_EQ(c.pal[5], 0x06);
    CHECK_EQ(c.attr[0], 0xE4);
    CHECK_EQ(c.attr[1], 0x1B);
    CHECK_EQ(fcpico_cart_stats(c.cart)->resyncs, fcpico_cart_stats(c.cart)->dma_stops);
    CHECK_EQ(pixel(&c, 0, 0), 0);
    CHECK_EQ(pixel(&c, 40, 10), 1);
    CHECK_EQ(pixel(&c, 100, 239), 3);
    CHECK_EQ(fcpico_cart_metrics(c.cart)->console_inits, 1);
    fcpico_cart_destroy(c.cart);
}

static void test_demo_responds_to_input(void) {
    console_t c;
    open_console(&c, "demo", NULL);
    boot(&c);
    CHECK_EQ(pixel(&c, 120, 112), 3);
    CHECK_EQ(pixel(&c, 140, 112), 1);
    for (int i = 0; i < 10; i++) frame(&c, KEY_RIGHT);
    frame(&c, 0);
    CHECK_EQ(pixel(&c, 140, 112), 3); /* moved 20 pixels right */
    CHECK_EQ(pixel(&c, 120, 112), 1);

    /* A changes the colour through a data-mode upload; the link resyncs after it. */
    int uploads = c.data_uploads;
    frame(&c, KEY_A);
    for (int i = 0; i < 8; i++) frame(&c, 0);
    CHECK_EQ(c.data_uploads, uploads + 1);
    CHECK_EQ(c.pal[3], 0x2A);

    /* Input at heartbeat N changes the mailbox sent at N+1, read in the frame after. */
    frame(&c, KEY_B);
    frame(&c, 0);
    frame(&c, 0);
    CHECK_EQ(c.mailbox[MBX_APU], 0x15);
    CHECK_EQ(c.mailbox[MBX_APU + 1], 0x01);
    CHECK_EQ(c.mailbox[MBX_APU + 8], 0xFF);
    fcpico_cart_destroy(c.cart);
}

static void test_instances_are_independent(void) {
    console_t a, b;
    open_console(&a, "demo", NULL);
    open_console(&b, "demo", NULL);
    boot(&a);
    boot(&b);
    for (int i = 0; i < 8; i++) frame(&a, KEY_DOWN);
    frame(&a, 0);
    frame(&b, 0);
    CHECK_EQ(pixel(&a, 129, 131), 3);
    CHECK_EQ(pixel(&b, 129, 131), 1);
    CHECK(fcpico_cart_stats(a.cart)->frames != fcpico_cart_stats(b.cart)->frames);
    fcpico_cart_destroy(a.cart);
    fcpico_cart_destroy(b.cart);
}

static void test_firmware_reset_and_power_cycle(void) {
    console_t c;
    open_console(&c, "demo", NULL);
    boot(&c);
    for (int i = 0; i < 5; i++) frame(&c, KEY_LEFT);
    fcpico_cart_ppu_write(c.cart, FP_COM_RST);
    CHECK_EQ(fcpico_cart_metrics(c.cart)->firmware_resets, 1);
    CHECK_EQ(fcpico_cart_stats(c.cart)->frames, 0);
    CHECK_EQ(fcpico_cart_ppu_read(c.cart), FCPICO_CART_OPEN_BUS);
    boot(&c);
    CHECK_EQ(pixel(&c, 120, 112), 3); /* square back in the middle */
    fcpico_cart_reset(c.cart);
    CHECK_EQ(fcpico_cart_metrics(c.cart)->ppu_reads, 0);
    CHECK_EQ(fcpico_cart_core(c.cart)->state, FCBUS_ST_IDLE);
    fcpico_cart_destroy(c.cart);
}

static void test_version_reply_serves_the_configured_rom(void) {
    memcpy(prg + FCBUS_ROM_STAMP_OFF, "2099-0101-0000", 14);
    fcpico_cart_t *cart = make("pattern", NULL);
    fcpico_cart_ppu_write(cart, FP_COM_VER);
    for (int i = 0; i < 8; i++) fcpico_cart_ppu_read(cart);
    char stamp[15] = {0};
    for (int i = 0; i < 14; i++) stamp[i] = (char)fcpico_cart_ppu_read(cart);
    CHECK(strcmp(stamp, "2099-0101-0000") == 0);
    fcpico_cart_destroy(cart);
}

int main(void) {
    test_creation_is_validated();
    test_stopped_bus_reads_open_bus();
    test_pattern_boots_through_data_mode();
    test_demo_responds_to_input();
    test_instances_are_independent();
    test_firmware_reset_and_power_cycle();
    test_version_reply_serves_the_configured_rom();
    return ctest_lite_result();
}

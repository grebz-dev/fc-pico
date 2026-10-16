/* SPDX-License-Identifier: BSD-3-Clause */
/* Mailbox builder: rp_system::initFC_COM_BUF / setPF_COM / setPF_VRAM / setAPU / setPF_APU. */
#include "ctest_lite.h"
#include "fcbus_core.h"

static uint8_t rom[FCBUS_ROM_PRG_BYTES];
static fcbus_core_t core;

static void init(void) {
    fcbus_config_t cfg = {rom};
    fcbus_core_init(&core, &cfg);
}

static void test_reset_layout(void) {
    init();
    const uint8_t *m = fcbus_core_mailbox(&core);
    CHECK_EQ(m[MBX_MAGIC], PF_MAGIC_NO);
    for (int i = 0; i < FC_COM_BUF_SIZE; i++) {
        if (i != MBX_MAGIC) {
            CHECK_EQ(m[i], 0);
        }
    }
}

static void test_cmd_fills_bytes_2_to_15(void) {
    init();
    for (int i = 0; i < FC_COM_BUF_SIZE16 - MBX_CMD; i++) {
        CHECK(fcbus_core_cmd(&core, (uint8_t)(0x10 + i)));
    }
    CHECK(!fcbus_core_cmd(&core, 0x7F));
    const uint8_t *m = fcbus_core_mailbox(&core);
    for (int i = MBX_CMD; i < FC_COM_BUF_SIZE16; i++) {
        CHECK_EQ(m[i], 0x10 + i - MBX_CMD);
    }
    CHECK_EQ(m[FC_COM_BUF_SIZE16], 0);
}

static void test_cmd_vram_layout_and_limit(void) {
    init();
    CHECK(fcbus_core_cmd_vram(&core, 0xE3C5, 0xAB)); /* masked to $23C5 */
    const uint8_t *m = fcbus_core_mailbox(&core);
    CHECK_EQ(m[2], PF_COM_VRAM | 0x23);
    CHECK_EQ(m[3], 0xC5);
    CHECK_EQ(m[4], 0xAB);
    /* setPF_VRAM accepts a poke while the index is at most 13: four pokes fit. */
    CHECK(fcbus_core_cmd_vram(&core, 0x2000, 1));
    CHECK(fcbus_core_cmd_vram(&core, 0x2001, 2));
    CHECK(fcbus_core_cmd_vram(&core, 0x2002, 3));
    CHECK_EQ(core.cmd_cursor, 14);
    CHECK(!fcbus_core_cmd_vram(&core, 0x2003, 4));
    CHECK(fcbus_core_cmd(&core, PF_COM_FDIN)); /* one plain byte still fits */
}

static void test_apu_flush_caps_and_carries_over(void) {
    init();
    for (int i = 0; i < 30; i++) {
        fcbus_core_apu_write(&core, (uint8_t)i, (uint8_t)(0x80 + i));
    }
    fcbus_core_apu_flush(&core);
    const uint8_t *m = fcbus_core_mailbox(&core);
    for (int i = 0; i < APU_PAIRS_MAX; i++) {
        CHECK_EQ(m[MBX_APU + 2 * i], i);
        CHECK_EQ(m[MBX_APU + 2 * i + 1], 0x80 + i);
    }
    CHECK_EQ(m[MBX_APU + 2 * APU_PAIRS_MAX], 0xFF);
    CHECK_EQ(fcbus_core_heartbeat(&core, PPU_COUNT_VAL), FCBUS_ARM);
    fcbus_core_apu_flush(&core);
    m = fcbus_core_mailbox(&core);
    for (int i = 0; i < 30 - APU_PAIRS_MAX; i++) {
        CHECK_EQ(m[MBX_APU + 2 * i], APU_PAIRS_MAX + i);
    }
    CHECK_EQ(m[MBX_APU + 2 * (30 - APU_PAIRS_MAX)], 0xFF);
}

static void test_apu_flush_without_writes_terminates(void) {
    init();
    fcbus_core_apu_flush(&core);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_APU], 0xFF);
}

static void test_apu_ring_wraps(void) {
    init();
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 20; i++) {
            fcbus_core_apu_write(&core, (uint8_t)i, (uint8_t)round);
        }
        fcbus_core_apu_flush(&core);
        const uint8_t *m = fcbus_core_mailbox(&core);
        CHECK_EQ(m[MBX_APU + 2 * 19 + 1], round);
        CHECK_EQ(m[MBX_APU + 40], 0xFF);
        CHECK_EQ(fcbus_core_heartbeat(&core, PPU_COUNT_VAL), FCBUS_ARM);
    }
}

static void test_data_mode_request_repeats_until_drq(void) {
    init();
    fcbus_core_request_data_mode(&core);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD], PF_COM_DMOD);
    fcbus_core_request_data_mode(&core); /* not duplicated in one mailbox */
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD + 1], 0);
    fcbus_core_heartbeat(&core, PPU_COUNT_VAL);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD], PF_COM_DMOD);
    fcbus_core_rx_byte(&core, FP_COM_DRQ);
    fcbus_core_heartbeat(&core, PPU_COUNT_VAL);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD], 0);
}

int main(void) {
    test_reset_layout();
    test_cmd_fills_bytes_2_to_15();
    test_cmd_vram_layout_and_limit();
    test_apu_flush_caps_and_carries_over();
    test_apu_flush_without_writes_terminates();
    test_apu_ring_wraps();
    test_data_mode_request_repeats_until_drq();
    return ctest_lite_result();
}

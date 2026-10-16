/* SPDX-License-Identifier: BSD-3-Clause */
/* End to end through the host backend: reads, writes and the read counter. */
#include "ctest_lite.h"
#include "fcbus_host.h"

static uint8_t rom[FCBUS_ROM_PRG_BYTES];
static fcbus_host_t host;

static void init(void) {
    for (int i = 0; i < FCBUS_ROM_PRG_BYTES; i++) rom[i] = (uint8_t)i;
    fcbus_config_t cfg = {rom};
    fcbus_host_init(&host, &cfg);
}

/* Performs n qualifying reads; returns the last value read. */
static int reads(int n) {
    int v = -2;
    for (int i = 0; i < n; i++) v = fcbus_host_ppu_read(&host);
    return v;
}

static void test_cold_start_locks_on(void) {
    init();
    CHECK_EQ(fcbus_host_ppu_read(&host), -1); /* stopped, but counted */
    reads(PPU_COUNT_VAL);                     /* the counter reports N-1 for N reads */
    fcbus_host_ppu_write(&host, 0x00);
    CHECK_EQ(host.core.stats.last_count, PPU_COUNT_VAL);
    CHECK_EQ(host.mode, FCBUS_HOST_STREAM);
    CHECK_EQ(fcbus_host_ppu_read(&host), 0); /* byte 0 of a cleared buffer */
}

static void test_published_picture_and_mailbox_on_the_wire(void) {
    init();
    reads(PPU_COUNT_VAL + 1);
    fcbus_host_ppu_write(&host, 0x00);
    fcbus_core_t *c = fcbus_host_core(&host);
    uint16_t *back = fcbus_core_stream_back(c);
    *fcbus_stream_word(back, 0, 0) = 0xA55A;
    fcbus_core_publish(c);
    fcbus_core_cmd(c, PF_COM_FDIN);
    reads(PPU_COUNT_VAL + 1);
    fcbus_host_ppu_write(&host, KEY_B);
    CHECK_EQ(fcbus_core_pad(c), KEY_B);
    reads(VRAM_HEAD_WORDS * 2);
    CHECK_EQ(fcbus_host_ppu_read(&host), 0x5A);
    CHECK_EQ(fcbus_host_ppu_read(&host), 0xA5);
    reads(VRAM_MAILBOX_OFF - VRAM_HEAD_WORDS * 2 - 2);
    CHECK_EQ(fcbus_host_ppu_read(&host), 0x00);        /* mailbox byte 0 */
    CHECK_EQ(fcbus_host_ppu_read(&host), PF_MAGIC_NO);
    CHECK_EQ(fcbus_host_ppu_read(&host), PF_COM_FDIN);
}

static void test_nudge_skips_bytes(void) {
    init();
    reads(PPU_COUNT_VAL + 1);
    fcbus_host_ppu_write(&host, 0x00);
    uint8_t *front = (uint8_t *)host.core.stream[host.core.front];
    front[0] = 0x11;
    front[1] = 0x22;
    front[2] = 0x33;
    reads(PPU_COUNT_VAL - 1); /* two short: reported count is VAL - 2 */
    fcbus_host_ppu_write(&host, 0x00);
    CHECK_EQ(host.core.stats.last_count, PPU_COUNT_VAL - 2);
    CHECK_EQ(fcbus_host_ppu_read(&host), 0x33);
}

static void test_out_of_phase_stops_then_recovers(void) {
    init();
    reads(PPU_COUNT_VAL + 1);
    fcbus_host_ppu_write(&host, 0x00);
    reads(1000);
    fcbus_host_ppu_write(&host, 0x00);
    CHECK_EQ(fcbus_host_ppu_read(&host), -1);
    reads(PPU_COUNT_VAL);
    fcbus_host_ppu_write(&host, 0x00);
    CHECK_EQ(host.mode, FCBUS_HOST_STREAM);
    CHECK_EQ(host.core.stats.resyncs, 1);
}

static void test_version_reply_then_filler(void) {
    init();
    fcbus_host_ppu_write(&host, FP_COM_VER);
    CHECK_EQ(reads(8), 0x43); /* 'C' ends the sync */
    CHECK_EQ(fcbus_host_ppu_read(&host), rom[FCBUS_ROM_STAMP_OFF]);
    reads(15);
    CHECK_EQ(fcbus_host_ppu_read(&host), 0xFF);
}

static void test_log_entries(void) {
    init();
    static const uint8_t burst[] = {FP_COM_LOG, 'o', 'k'};
    fcbus_host_write_burst(&host, burst, sizeof burst);
    size_t n;
    const fcbus_host_log_entry_t *log = fcbus_host_log(&host, &n);
    CHECK_EQ(n, 1);
    CHECK_EQ(log[0].kind, FCBUS_ACT_LOG);
    CHECK_EQ(log[0].len, 2);
    CHECK_EQ(log[0].data[0], 'o');
    fcbus_host_log_clear(&host);
    fcbus_host_log(&host, &n);
    CHECK_EQ(n, 0);
}

int main(void) {
    test_cold_start_locks_on();
    test_published_picture_and_mailbox_on_the_wire();
    test_nudge_skips_bytes();
    test_out_of_phase_stops_then_recovers();
    test_version_reply_then_filler();
    test_log_entries();
    return ctest_lite_result();
}

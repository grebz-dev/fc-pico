/* SPDX-License-Identifier: BSD-3-Clause */
/* Frame sync and buffer ownership: rp_system::ppu_dma() and convVram()'s swap. */
#include "ctest_lite.h"
#include "fcbus_core.h"

static uint8_t rom[FCBUS_ROM_PRG_BYTES];
static fcbus_core_t core;

static void init(void) {
    fcbus_config_t cfg = {rom};
    fcbus_core_init(&core, &cfg);
}

static void test_decision_table(void) {
    CHECK_EQ(fcbus_sync_decide(PPU_COUNT_VAL - 3, PPU_COUNT_VAL), FCBUS_STOP);
    CHECK_EQ(fcbus_sync_decide(PPU_COUNT_VAL - 2, PPU_COUNT_VAL), FCBUS_ARM_NUDGE2);
    CHECK_EQ(fcbus_sync_decide(PPU_COUNT_VAL - 1, PPU_COUNT_VAL), FCBUS_ARM_NUDGE1);
    CHECK_EQ(fcbus_sync_decide(PPU_COUNT_VAL, PPU_COUNT_VAL), FCBUS_ARM);
    CHECK_EQ(fcbus_sync_decide(PPU_COUNT_VAL + 2, PPU_COUNT_VAL), FCBUS_ARM);
    CHECK_EQ(fcbus_sync_decide(PPU_COUNT_VAL + 3, PPU_COUNT_VAL), FCBUS_STOP);
    CHECK_EQ(fcbus_sync_decide(0, PPU_COUNT_VAL), FCBUS_STOP);
    CHECK_EQ(PPU_COUNT_VAL, 15490);
}

static void test_mailbox_reaches_the_front_tail_on_arm(void) {
    init();
    fcbus_core_cmd(&core, PF_COM_FDIN);
    CHECK_EQ(fcbus_core_heartbeat(&core, PPU_COUNT_VAL), FCBUS_ARM);
    const uint8_t *front = (const uint8_t *)fcbus_core_stream_front(&core);
    CHECK_EQ(front[VRAM_MAILBOX_OFF + MBX_MAGIC], PF_MAGIC_NO);
    CHECK_EQ(front[VRAM_MAILBOX_OFF + MBX_CMD], PF_COM_FDIN);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD], 0); /* reset for the next frame */
    fcbus_action_t a;
    CHECK(fcbus_core_pop_action(&core, &a));
    CHECK_EQ(a.kind, FCBUS_ACT_HEARTBEAT);
    CHECK_EQ(a.arg, FCBUS_ARM);
}

static void test_stop_keeps_mailbox_and_publication(void) {
    init();
    fcbus_core_cmd(&core, PF_COM_FDOT);
    fcbus_core_stream_back(&core)[0] = 0x1234;
    fcbus_core_publish(&core);
    CHECK(!fcbus_core_back_is_free(&core));
    CHECK_EQ(fcbus_core_heartbeat(&core, 100), FCBUS_STOP);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD], PF_COM_FDOT);
    CHECK_EQ(fcbus_core_stream_front(&core)[0], 0);
    fcbus_action_t a;
    CHECK(fcbus_core_pop_action(&core, &a));
    CHECK_EQ(a.kind, FCBUS_ACT_STOP_DMA);
    CHECK_EQ(fcbus_core_heartbeat(&core, PPU_COUNT_VAL + 1), FCBUS_ARM);
    CHECK_EQ(fcbus_core_stream_front(&core)[0], 0x1234);
    CHECK(fcbus_core_back_is_free(&core));
    CHECK_EQ(core.stats.dma_stops, 1);
    CHECK_EQ(core.stats.resyncs, 1);
}

static void test_stats_and_state(void) {
    init();
    CHECK_EQ(core.state, FCBUS_ST_IDLE);
    fcbus_core_heartbeat(&core, PPU_COUNT_VAL - 1);
    fcbus_core_heartbeat(&core, PPU_COUNT_VAL + 9);
    fcbus_core_heartbeat(&core, PPU_COUNT_VAL);
    CHECK_EQ(core.state, FCBUS_ST_RUN);
    CHECK_EQ(core.stats.frames, 3);
    CHECK_EQ(core.stats.last_count, PPU_COUNT_VAL);
    CHECK_EQ(core.stats.count_hist[3], 1);
    CHECK_EQ(core.stats.count_hist[8], 1);
    CHECK_EQ(core.stats.count_hist[4], 1);
}

static void test_stream_word_layout(void) {
    init();
    uint16_t *back = fcbus_core_stream_back(&core);
    CHECK(fcbus_stream_word(back, 0, 0) == back + 31);
    CHECK(fcbus_stream_word(back, 1, 0) == back + 63);
    CHECK(fcbus_stream_word(back, 239, 31) == back + 31 + 239 * 32 + 31);
    CHECK(fcbus_core_stream_back(&core) != (uint16_t *)fcbus_core_stream_front(&core));
}

int main(void) {
    test_decision_table();
    test_mailbox_reaches_the_front_tail_on_arm();
    test_stop_keeps_mailbox_and_publication();
    test_stats_and_state();
    test_stream_word_layout();
    return ctest_lite_result();
}

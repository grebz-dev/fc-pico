/* SPDX-License-Identifier: BSD-3-Clause */
/* Receive dispatcher: rp_system::jobRcvCom() and the replies it starts. */
#include "ctest_lite.h"
#include "fcbus_core.h"

static uint8_t rom[FCBUS_ROM_PRG_BYTES];
static fcbus_core_t core;

static void init(void) {
    for (int i = 0; i < FCBUS_ROM_PRG_BYTES; i++) rom[i] = (uint8_t)(i * 7);
    memcpy(rom + FCBUS_ROM_STAMP_OFF, "2025-0101-1200\0\0", 16);
    fcbus_config_t cfg = {rom};
    fcbus_core_init(&core, &cfg);
}

static fcbus_action_t pop(void) {
    fcbus_action_t a;
    memset(&a, 0xEE, sizeof a);
    CHECK(fcbus_core_pop_action(&core, &a));
    return a;
}

static void test_heartbeat_bytes(void) {
    init();
    CHECK(fcbus_core_rx_will_heartbeat(&core, KEY_A | KEY_RIGHT));
    CHECK(fcbus_core_rx_will_heartbeat(&core, 0x00));
    CHECK(!fcbus_core_rx_will_heartbeat(&core, FP_COM_VER));
    CHECK(!fcbus_core_rx_will_heartbeat(&core, FP_COM_INI));
    fcbus_core_set_read_count(&core, PPU_COUNT_VAL);
    fcbus_core_rx_byte(&core, KEY_A | KEY_RIGHT);
    CHECK_EQ(fcbus_core_pad(&core), KEY_A | KEY_RIGHT);
    CHECK_EQ(pop().kind, FCBUS_ACT_HEARTBEAT);
    /* An opcode argument is never a heartbeat. */
    fcbus_core_rx_byte(&core, FP_COM_ROM);
    CHECK(!fcbus_core_rx_will_heartbeat(&core, 0x05));
}

static void test_version_reply(void) {
    init();
    fcbus_core_rx_byte(&core, FP_COM_VER);
    fcbus_action_t a = pop();
    static const uint8_t sync[8] = {0x21, 0x21, 0x21, 0x21, 0x21, 0x23, 0x46, 0x43};
    CHECK_EQ(a.kind, FCBUS_ACT_STREAM_RESPONSE);
    CHECK_EQ(a.len, 24);
    CHECK_MEM(a.data, sync, 8);
    CHECK_MEM(a.data + 8, rom + FCBUS_ROM_STAMP_OFF, 16);
}

static void test_rom_page_reply_streams_on(void) {
    init();
    fcbus_core_rx_byte(&core, FP_COM_ROM);
    CHECK(!fcbus_core_pop_action(&core, &(fcbus_action_t){0}));
    fcbus_core_rx_byte(&core, 0x85); /* bit 7 ignored: page 5 */
    fcbus_action_t a = pop();
    CHECK_EQ(a.kind, FCBUS_ACT_STREAM_RESPONSE);
    CHECK(a.data == rom + 0x500);
    CHECK_EQ(a.len, FCBUS_ROM_PRG_BYTES - 0x500);
    fcbus_core_rx_byte(&core, FP_COM_ROM);
    fcbus_core_rx_byte(&core, FP_COM_INI); /* an argument, not an opcode */
    a = pop();
    CHECK(a.data == rom + 0x7F00);
    CHECK_EQ(a.len, 0x100);
}

static void test_log_bursts(void) {
    init();
    fcbus_core_rx_byte(&core, FP_COM_LOG);
    for (int i = 0; i < FCBUS_LOG_MAX; i++) fcbus_core_rx_byte(&core, (uint8_t)(FP_COM_VER + i));
    fcbus_action_t a = pop();
    CHECK_EQ(a.kind, FCBUS_ACT_LOG);
    CHECK_EQ(a.len, FCBUS_LOG_MAX);
    CHECK_EQ(a.data[0], FP_COM_VER);
    fcbus_core_rx_byte(&core, FP_COM_LOG);
    fcbus_core_rx_byte(&core, 'h');
    fcbus_core_rx_byte(&core, 'i');
    fcbus_core_rx_idle(&core);
    a = pop();
    CHECK_EQ(a.len, 2);
    CHECK_EQ(a.data[1], 'i');
    fcbus_core_rx_idle(&core); /* no burst open */
    CHECK(!fcbus_core_pop_action(&core, &a));
}

static void test_init_resets_like_init2(void) {
    uint8_t pal[FCBUS_PAL_LEN] = {0x0F, 0x30};
    init();
    fcbus_core_set_palette(&core, pal);
    fcbus_core_set_attr(&core, 0, 0, 2);
    fcbus_core_set_step(&core, 4);
    fcbus_core_cmd(&core, PF_COM_FDIN);
    fcbus_core_stream_back(&core)[5] = 0xBEEF;
    fcbus_core_publish(&core);
    fcbus_core_set_read_count(&core, PPU_COUNT_VAL);
    fcbus_core_rx_byte(&core, 0x00);
    (void)pop();
    fcbus_core_rx_byte(&core, FP_COM_INI);
    fcbus_core_rx_byte(&core, 0x02);
    fcbus_action_t a = pop();
    CHECK_EQ(a.kind, FCBUS_ACT_INIT);
    CHECK_EQ(a.arg, 2);
    CHECK_EQ(core.state, FCBUS_ST_IDLE);
    CHECK_EQ(core.stats.init_events, 1);
    CHECK_EQ(core.attr_want[0], 0);
    CHECK(core.attr_chg);
    CHECK(!core.pal_chg);
    CHECK_EQ(core.pal_want[1], 0x30); /* init2() keeps the palette */
    CHECK_EQ(core.step, 0);
    CHECK_EQ(fcbus_core_mailbox(&core)[MBX_CMD], 0);
    CHECK_EQ(fcbus_core_stream_front(&core)[5], 0);
    CHECK(fcbus_core_back_is_free(&core));
}

static void test_reset(void) {
    init();
    fcbus_core_rx_byte(&core, FP_COM_RST);
    CHECK_EQ(pop().kind, FCBUS_ACT_RESET);
    CHECK_EQ(core.state, FCBUS_ST_IDLE);
}

static void check_drq(fcbus_action_t a, uint8_t com, uint16_t adr, uint16_t size) {
    uint32_t w0 = FCBUS_DRQ_MAGIC_BASE + com + ((uint32_t)adr << 16);
    CHECK_EQ(a.kind, FCBUS_ACT_STREAM_RESPONSE);
    CHECK_EQ(a.len, 8);
    CHECK_EQ(a.data[0] | (a.data[1] << 8) | (a.data[2] << 16) | ((uint32_t)a.data[3] << 24), w0);
    CHECK_EQ(a.data[4] | (a.data[5] << 8), size);
    CHECK_EQ(a.data[6] | a.data[7], 0);
}

static void test_data_mode_sequence(void) {
    uint8_t pal[FCBUS_PAL_LEN], attr[FCBUS_ATTR_LEN];
    for (int i = 0; i < FCBUS_PAL_LEN; i++) pal[i] = (uint8_t)(0x20 + i);
    for (int i = 0; i < FCBUS_ATTR_LEN; i++) attr[i] = (uint8_t)(0x80 + i);
    init();
    fcbus_core_set_palette(&core, pal);
    fcbus_core_set_attr_table(&core, attr);
    fcbus_core_set_step(&core, 7);
    fcbus_core_request_data_mode(&core);

    fcbus_core_rx_byte(&core, FP_COM_DRQ);
    CHECK_EQ(core.state, FCBUS_ST_DATA);
    check_drq(pop(), PF_DAT_VRAM, 0x3F00, FCBUS_PAL_LEN);
    fcbus_core_rx_byte(&core, FP_COM_DLD);
    fcbus_core_rx_byte(&core, 0);
    fcbus_action_t a = pop();
    CHECK_EQ(a.len, 256);
    CHECK_MEM(a.data, pal, FCBUS_PAL_LEN);
    CHECK_EQ(a.data[FCBUS_PAL_LEN], 0);

    fcbus_core_rx_byte(&core, FP_COM_DRQ);
    check_drq(pop(), PF_DAT_VRAM, 0x23C0, FCBUS_ATTR_LEN);
    fcbus_core_rx_byte(&core, FP_COM_DLD);
    fcbus_core_rx_byte(&core, 0);
    CHECK_MEM(pop().data, attr, FCBUS_ATTR_LEN);
    fcbus_core_rx_byte(&core, FP_COM_DLD);
    fcbus_core_rx_byte(&core, 1); /* beyond the table: zeros */
    CHECK_EQ(pop().data[0], 0);

    fcbus_core_rx_byte(&core, FP_COM_DRQ);
    check_drq(pop(), PF_DAT_STEP, 7, 0);
    CHECK_EQ(core.state, FCBUS_ST_RUN);
    fcbus_core_rx_byte(&core, FP_COM_DRQ);
    check_drq(pop(), PF_COM_NONE, 0, 0);
}

int main(void) {
    test_heartbeat_bytes();
    test_version_reply();
    test_rom_page_reply_streams_on();
    test_log_bursts();
    test_init_resets_like_init2();
    test_reset();
    test_data_mode_sequence();
    return ctest_lite_result();
}

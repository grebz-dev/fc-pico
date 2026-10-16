/* SPDX-License-Identifier: BSD-3-Clause */
/* Palette and attribute table: rp_system::setPalData / setPal / setAtrData / clearAtrData /
 * setAtr and the table diff in rp_system::update(). */
#include "ctest_lite.h"
#include "fcbus_core.h"

static uint8_t rom[FCBUS_ROM_PRG_BYTES];
static fcbus_core_t core;

static void init(void) {
    fcbus_config_t cfg = {rom};
    fcbus_core_init(&core, &cfg);
    core.attr_chg = false; /* init marks the cleared table for upload, like init2() */
}

static int pokes(void) {
    return (core.cmd_cursor - MBX_CMD) / 3;
}

static void test_bulk_setters_mark_for_upload_without_pokes(void) {
    uint8_t pal[FCBUS_PAL_LEN], attr[FCBUS_ATTR_LEN];
    for (int i = 0; i < FCBUS_PAL_LEN; i++) pal[i] = (uint8_t)(i + 1);
    for (int i = 0; i < FCBUS_ATTR_LEN; i++) attr[i] = (uint8_t)(0x40 + i);
    init();
    fcbus_core_set_palette(&core, pal);
    fcbus_core_set_attr_table(&core, attr);
    CHECK(core.pal_chg);
    CHECK(core.attr_chg);
    fcbus_core_update_tables(&core);
    CHECK_EQ(pokes(), 0);
}

static void test_single_entry_changes_are_poked_palette_first(void) {
    init();
    fcbus_core_set_attr(&core, 0, 0, 3);
    fcbus_core_set_pal(&core, 5, 0x21);
    CHECK(core.pal_chg);
    fcbus_core_update_tables(&core);
    const uint8_t *m = fcbus_core_mailbox(&core);
    CHECK_EQ(pokes(), 2);
    CHECK_EQ(m[2], PF_COM_VRAM | 0x3F);
    CHECK_EQ(m[3], 0x05);
    CHECK_EQ(m[4], 0x21);
    CHECK_EQ(m[5], PF_COM_VRAM | 0x23);
    CHECK_EQ(m[6], 0xC0);
    CHECK_EQ(m[7], 0x03);
    fcbus_core_update_tables(&core); /* nothing left to send */
    CHECK_EQ(pokes(), 2);
}

static void test_diff_resumes_after_a_full_mailbox(void) {
    init();
    for (int i = 0; i < 6; i++) {
        fcbus_core_set_pal(&core, (uint8_t)i, (uint8_t)(0x10 + i));
    }
    fcbus_core_update_tables(&core);
    CHECK_EQ(pokes(), 4);
    fcbus_core_heartbeat(&core, PPU_COUNT_VAL);
    fcbus_core_update_tables(&core);
    const uint8_t *m = fcbus_core_mailbox(&core);
    CHECK_EQ(pokes(), 2);
    CHECK_EQ(m[3], 0x04);
    CHECK_EQ(m[6], 0x05);
}

static void test_set_attr_packs_quadrants(void) {
    init();
    fcbus_core_set_attr(&core, 0, 0, 1);
    fcbus_core_set_attr(&core, 1, 0, 2);
    fcbus_core_set_attr(&core, 0, 1, 3);
    fcbus_core_set_attr(&core, 1, 1, 1);
    CHECK_EQ(core.attr_want[0], 0x01 | (2 << 2) | (3 << 4) | (1 << 6));
    fcbus_core_set_attr(&core, 1, 0, 0);
    CHECK_EQ(core.attr_want[0], 0x01 | (3 << 4) | (1 << 6));
    fcbus_core_set_attr(&core, 3, 2, 2); /* byte 1 + 8, upper-right quadrant */
    CHECK_EQ(core.attr_want[9], 2 << 2);
    fcbus_core_set_attr(&core, 15, 15, 1); /* last byte, lower-right quadrant */
    CHECK_EQ(core.attr_want[63], 0x40);
}

static void test_clear_attr(void) {
    init();
    fcbus_core_set_attr(&core, 0, 0, 3);
    fcbus_core_clear_attr(&core);
    CHECK_EQ(core.attr_want[0], 0);
    CHECK(core.attr_chg);
    fcbus_core_update_tables(&core);
    CHECK_EQ(pokes(), 0);
}

int main(void) {
    test_bulk_setters_mark_for_upload_without_pokes();
    test_single_entry_changes_are_poked_palette_first();
    test_diff_resumes_after_a_full_mailbox();
    test_set_attr_packs_quadrants();
    test_clear_attr();
    return ctest_lite_result();
}

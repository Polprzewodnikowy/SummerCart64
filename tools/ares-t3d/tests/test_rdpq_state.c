/*
 * test_rdpq_state.c — Unit tests for RDP state tracking
 */

#include "rdpq_state.h"
#include "test_helpers.h"

static void test_set_color_image(void) {
    RDPQState s;
    rdpq_state_init(&s);

    /* SET_COLOR_IMAGE: format=0(RGBA), size=2(16bit), width=320-1=319 */
    uint32_t hi = (RDP_CMD_SET_COLOR_IMAGE << 24) | (0 << 21) | (2 << 19) | 319;
    uint32_t lo = 0x00100000; /* RDRAM address */

    bool changed = rdpq_state_process(&s, hi, lo);
    ASSERT(changed, "SET_COLOR_IMAGE updates state");
    ASSERT_EQ(s.color_image_format, 0, "format = RGBA");
    ASSERT_EQ(s.color_image_size, 2, "size = 16-bit");
    ASSERT_EQ(s.color_image_width, 320, "width = 320");
    ASSERT_EQ(s.color_image_addr, 0x00100000, "addr correct");
}

static void test_set_other_modes(void) {
    RDPQState s;
    rdpq_state_init(&s);

    /* SET_OTHER_MODES: cycle_type=0 (1-cycle), z_cmp=1, z_upd=1 */
    uint32_t hi = (RDP_CMD_SET_OTHER_MODES << 24) | (0 << 20); /* cycle_type=0 */
    uint32_t lo = (1 << 4) | (1 << 5); /* Z_CMP | Z_UPD */

    rdpq_state_process(&s, hi, lo);
    ASSERT_EQ(s.cycle_type, 0, "cycle_type = 1-cycle");
    ASSERT(s.depth_compare, "depth compare enabled");
    ASSERT(s.depth_update, "depth update enabled");
}

static void test_set_scissor(void) {
    RDPQState s;
    rdpq_state_init(&s);

    /* SET_SCISSOR: x0=10, y0=20, x1=310, y1=230 (in 10.2 fixed point) */
    uint32_t hi = (RDP_CMD_SET_SCISSOR << 24) | ((10 << 2) << 12) | (20 << 2);
    uint32_t lo = ((310 << 2) << 12) | (230 << 2);

    rdpq_state_process(&s, hi, lo);
    ASSERT_EQ(s.scissor_x0, 10, "scissor x0");
    ASSERT_EQ(s.scissor_y0, 20, "scissor y0");
    ASSERT_EQ(s.scissor_x1, 310, "scissor x1");
    ASSERT_EQ(s.scissor_y1, 230, "scissor y1");
}

static void test_set_colors(void) {
    RDPQState s;
    rdpq_state_init(&s);

    rdpq_state_process(&s, RDP_CMD_SET_FOG_COLOR << 24, 0xFF804020);
    ASSERT_EQ(s.fog_color[0], 0xFF, "fog r");
    ASSERT_EQ(s.fog_color[1], 0x80, "fog g");
    ASSERT_EQ(s.fog_color[2], 0x40, "fog b");
    ASSERT_EQ(s.fog_color[3], 0x20, "fog a");

    rdpq_state_process(&s, RDP_CMD_SET_PRIM_COLOR << 24, 0x11223344);
    ASSERT_EQ(s.prim_color[0], 0x11, "prim r");
    ASSERT_EQ(s.prim_color[3], 0x44, "prim a");
}

static void test_sync_full(void) {
    RDPQState s;
    rdpq_state_init(&s);

    ASSERT(!s.sync_full_pending, "no sync initially");
    rdpq_state_process(&s, RDP_CMD_SYNC_FULL << 24, 0);
    ASSERT(s.sync_full_pending, "sync_full set");
    ASSERT_EQ(s.sync_fulls, 1, "sync count = 1");
}

static void test_unknown_opcode(void) {
    RDPQState s;
    rdpq_state_init(&s);

    bool changed = rdpq_state_process(&s, 0x01000000, 0); /* opcode 1 = unknown */
    ASSERT(!changed, "unknown opcode returns false");
}

static void test_cmd_name(void) {
    ASSERT_EQ(strcmp(rdpq_cmd_name(RDP_CMD_SET_COLOR_IMAGE), "SET_COLOR_IMAGE"), 0, "color image name");
    ASSERT_EQ(strcmp(rdpq_cmd_name(RDP_CMD_SYNC_FULL), "SYNC_FULL"), 0, "sync full name");
    ASSERT_EQ(strcmp(rdpq_cmd_name(0x01), "UNKNOWN"), 0, "unknown name");
}

int main(void) {
    fprintf(stderr, "=== test_rdpq_state ===\n");
    test_set_color_image();
    test_set_other_modes();
    test_set_scissor();
    test_set_colors();
    test_sync_full();
    test_unknown_opcode();
    test_cmd_name();
    TEST_SUMMARY();
}

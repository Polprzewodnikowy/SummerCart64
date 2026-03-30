/*
 * test_t3d_commands.c — Unit tests for T3D command parsing
 *
 * Tests command decoding, size calculation, validation, and edge cases.
 * Uses synthetic command buffers in a fake RDRAM.
 */

#include "t3d_hle.h"
#include "test_helpers.h"

#define RDRAM_SIZE (8 * 1024 * 1024)
static uint8_t fake_rdram[4096]; /* small buffer for tests */

/* Encode an rspq command word: overlay_id, cmd_id, args */
static uint32_t rspq_word(int ovl, int cmd, uint32_t args) {
    return ((ovl & 0xF) << 28) | ((cmd & 0xF) << 24) | (args & 0x00FFFFFF);
}

static void test_cmd_name(void) {
    ASSERT_EQ(strcmp(t3d_hle_cmd_name(T3D_CMD_TRI_DRAW), "TRI_DRAW"), 0, "TRI_DRAW name");
    ASSERT_EQ(strcmp(t3d_hle_cmd_name(T3D_CMD_VERT_LOAD), "VERT_LOAD"), 0, "VERT_LOAD name");
    ASSERT_EQ(strcmp(t3d_hle_cmd_name(T3D_CMD_TRI_SYNC), "TRI_SYNC"), 0, "TRI_SYNC name");
    ASSERT_EQ(strcmp(t3d_hle_cmd_name(99), "UNKNOWN"), 0, "out of range name");
    ASSERT_EQ(strcmp(t3d_hle_cmd_name(-1), "UNKNOWN"), 0, "negative name");
}

static void test_rspq_macros(void) {
    uint32_t w = rspq_word(6, 4, 0x001234);
    ASSERT_EQ(RSPQ_OVL_ID(w), 6, "overlay ID extraction");
    ASSERT_EQ(RSPQ_CMD_ID(w), 4, "command ID extraction");
    ASSERT_EQ(RSPQ_ARGS(w), 0x001234, "args extraction");

    /* Edge case: max overlay, max cmd */
    w = rspq_word(15, 15, 0xFFFFFF);
    ASSERT_EQ(RSPQ_OVL_ID(w), 15, "max overlay ID");
    ASSERT_EQ(RSPQ_CMD_ID(w), 15, "max cmd ID");
    ASSERT_EQ(RSPQ_ARGS(w), 0xFFFFFF, "max args");
}

static void test_drawflags_parse(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false; /* suppress per-cmd logs */
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    /* Simulate: first detect overlay 6 as T3D via SCREEN_SIZE */
    memset(fake_rdram, 0, sizeof(fake_rdram));
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_SCREEN_SIZE, 0));
    /* Fill remaining 4 words of SCREEN_SIZE (total 5 words) */
    put_be32(fake_rdram, 4, 0x00010000);
    put_be32(fake_rdram, 8, 0x01400000);
    put_be32(fake_rdram, 12, 0x01400F00);
    put_be32(fake_rdram, 16, 0x7FFF0000);
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    /* Now send DRAWFLAGS (2 words: cull_mask + rdp_cmd) */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_DRAWFLAGS, T3D_FLAG_DEPTH | T3D_FLAG_SHADED));
    put_be32(fake_rdram, 4, 0x12345678); /* rdp command word */
    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(consumed, 2, "DRAWFLAGS consumes 2 words");
    ASSERT_EQ(state.draw_flags, T3D_FLAG_DEPTH | T3D_FLAG_SHADED, "draw_flags set");
    ASSERT_EQ(state.frame.state_changes, 2, "2 state changes (screen_size + drawflags)");

    fclose(devnull);
}

static void test_tri_draw_validation(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    /* Force overlay detection */
    state.detect.overlay_id = 6;
    state.detect.confirmed = true;
    state.vert_count = 10;

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* TRI_DRAW is 2 words: w0=[ovl|cmd|v0_dmem], w1=[v1_dmem(16)|v2_dmem(16)]
     * The values are DMEM addresses, not indices. We just count tris for now. */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_TRI_DRAW, 0x100));
    put_be32(fake_rdram, 4, (0x124 << 16) | 0x148); /* v1, v2 dmem addrs */
    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);
    ASSERT_EQ(consumed, 2, "TRI_DRAW consumes 2 words");
    ASSERT_EQ(state.frame.tris_drawn, 1, "tri drawn counted");

    /* Another tri */
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);
    ASSERT_EQ(state.frame.tris_drawn, 2, "second tri drawn");

    fclose(devnull);
}

static void test_overlay_0_skipped(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* rspq internal command (overlay 0) should return 0 (not handled) */
    put_be32(fake_rdram, 0, rspq_word(0, RSPQ_CMD_JUMP, 0x100));
    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);
    ASSERT_EQ(consumed, 0, "overlay 0 returns 0 (not handled)");
    ASSERT_EQ(state.frame.cmds_rspq, 1, "rspq cmd counted");

    fclose(devnull);
}

static void test_unknown_cmd_id(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    /* Force overlay */
    state.detect.overlay_id = 6;
    state.detect.confirmed = true;

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* cmd_id = 15 which is >= T3D_CMD_COUNT (13) */
    put_be32(fake_rdram, 0, rspq_word(6, 15, 0));
    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);
    ASSERT_EQ(consumed, -1, "unknown cmd returns -1 (stop scanning)");
    ASSERT_EQ(state.frame.cmds_unsupported, 1, "unsupported counted");

    fclose(devnull);
}

static void test_out_of_bounds_addr(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* Address beyond RDRAM */
    int consumed = t3d_hle_process_command(&state, fake_rdram, 64, 0xFFFFFF);
    ASSERT_EQ(consumed, 0, "out of bounds returns 0");
    ASSERT_EQ(state.frame.errors, 1, "error counted");

    fclose(devnull);
}

int main(void) {
    fprintf(stderr, "=== test_t3d_commands ===\n");
    test_cmd_name();
    test_rspq_macros();
    test_drawflags_parse();
    test_tri_draw_validation();
    test_overlay_0_skipped();
    test_unknown_cmd_id();
    test_out_of_bounds_addr();
    TEST_SUMMARY();
}

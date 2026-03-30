/*
 * test_detection.c — Tests for T3D overlay detection via confidence scoring
 */

#include "t3d_hle.h"
#include "test_helpers.h"

static uint8_t fake_rdram[4096];

static uint32_t rspq_word(int ovl, int cmd, uint32_t args) {
    return ((ovl & 0xF) << 28) | ((cmd & 0xF) << 24) | (args & 0x00FFFFFF);
}

static void test_detection_requires_threshold(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* Single SCREEN_SIZE from overlay 6 — should be candidate but not confirmed */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_SCREEN_SIZE, 0));
    put_be32(fake_rdram, 4, 0); put_be32(fake_rdram, 8, 0);
    put_be32(fake_rdram, 12, 0); put_be32(fake_rdram, 16, 0);
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(state.detect.overlay_id, 6, "candidate set to 6");
    ASSERT_EQ(state.detect.confirmed, false, "not yet confirmed (only 1 signal)");
    ASSERT_EQ(state.detect.confidence, 1, "confidence = 1");

    fclose(devnull);
}

static void test_detection_confirms_at_threshold(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* SCREEN_SIZE → confidence 1 */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_SCREEN_SIZE, 0));
    put_be32(fake_rdram, 4, 0); put_be32(fake_rdram, 8, 0);
    put_be32(fake_rdram, 12, 0); put_be32(fake_rdram, 16, 0);
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    /* DRAWFLAGS → confidence 2 */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_DRAWFLAGS, T3D_FLAG_DEPTH));
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);
    ASSERT_EQ(state.detect.confirmed, false, "2 signals: not yet confirmed");

    /* VERT_LOAD with valid RDRAM address → confidence 3 → confirmed */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_VERT_LOAD, 64));
    put_be32(fake_rdram, 4, 0x200); /* valid RDRAM addr */
    put_be32(fake_rdram, 8, 0);
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(state.detect.confirmed, true, "3 signals: confirmed!");
    ASSERT_EQ(state.detect.overlay_id, 6, "overlay 6 confirmed");
    ASSERT(state.detect.confidence >= T3D_DETECT_CONFIDENCE_THRESHOLD,
           "confidence at threshold");

    fclose(devnull);
}

static void test_different_overlay_ignored_after_candidate(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* First command from overlay 6 → becomes candidate */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_SCREEN_SIZE, 0));
    put_be32(fake_rdram, 4, 0); put_be32(fake_rdram, 8, 0);
    put_be32(fake_rdram, 12, 0); put_be32(fake_rdram, 16, 0);
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    /* Command from overlay 3 — should NOT bump confidence */
    put_be32(fake_rdram, 0, rspq_word(3, T3D_CMD_DRAWFLAGS, 0));
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(state.detect.overlay_id, 6, "candidate still 6");
    ASSERT_EQ(state.detect.confidence, 1, "confidence unchanged by overlay 3");

    fclose(devnull);
}

static void test_frame_summary_resets(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    state.detect.overlay_id = 6;
    state.detect.confirmed = true;

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* Process a few commands */
    put_be32(fake_rdram, 0, rspq_word(6, T3D_CMD_DRAWFLAGS, 0));
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);
    t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(state.frame.cmds_t3d, 2, "2 cmds before frame_end");

    t3d_hle_frame_end(&state);

    ASSERT_EQ(state.frame.cmds_t3d, 0, "counters reset after frame_end");
    ASSERT_EQ(state.total_frames, 1, "total_frames incremented");

    fclose(devnull);
}

int main(void) {
    fprintf(stderr, "=== test_detection ===\n");
    test_detection_requires_threshold();
    test_detection_confirms_at_threshold();
    test_different_overlay_ignored_after_candidate();
    test_frame_summary_resets();
    TEST_SUMMARY();
}

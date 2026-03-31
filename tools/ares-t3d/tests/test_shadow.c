/*
 * test_shadow.c — Tests for shadow render comparison
 */

#include "shadow_render.h"
#include "test_helpers.h"
#include <math.h>

static void test_identical_frames(void) {
    uint8_t fb[16 * 4]; /* 4x4 pixels */
    for (int i = 0; i < 16 * 4; i++) fb[i] = 128;

    T3DShadowResult r = {0};
    t3d_shadow_compare_pixels(&r, fb, fb, 4, 4, 0);

    ASSERT(r.pixel_compared, "compared flag set");
    ASSERT_EQ(r.pixel_total, 16, "16 pixels");
    ASSERT_EQ(r.pixel_mismatch, 0, "no mismatches");
    ASSERT(r.mismatch_pct < 0.01f, "0% mismatch");
    ASSERT_EQ(r.max_channel_diff, 0, "zero diff");
}

static void test_completely_different(void) {
    uint8_t fb_a[4 * 4];
    uint8_t fb_b[4 * 4];
    memset(fb_a, 0, sizeof(fb_a));
    memset(fb_b, 255, sizeof(fb_b));

    T3DShadowResult r = {0};
    t3d_shadow_compare_pixels(&r, fb_a, fb_b, 1, 1, 0);

    ASSERT_EQ(r.pixel_total, 1, "1 pixel");
    ASSERT_EQ(r.pixel_mismatch, 1, "1 mismatch");
    ASSERT_EQ(r.max_channel_diff, 255, "max diff = 255");
    ASSERT(fabsf(r.mismatch_pct - 100.0f) < 0.1f, "100% mismatch");
}

static void test_tolerance(void) {
    uint8_t fb_a[4] = {100, 100, 100, 255};
    uint8_t fb_b[4] = {103, 97, 100, 255}; /* within tolerance 5 */

    T3DShadowResult r = {0};
    t3d_shadow_compare_pixels(&r, fb_a, fb_b, 1, 1, 5);

    ASSERT_EQ(r.pixel_mismatch, 0, "within tolerance = no mismatch");
    ASSERT_EQ(r.max_channel_diff, 3, "max diff = 3");
}

static void test_tolerance_exceeded(void) {
    uint8_t fb_a[4] = {100, 100, 100, 255};
    uint8_t fb_b[4] = {110, 100, 100, 255}; /* exceeds tolerance 5 */

    T3DShadowResult r = {0};
    t3d_shadow_compare_pixels(&r, fb_a, fb_b, 1, 1, 5);

    ASSERT_EQ(r.pixel_mismatch, 1, "exceeds tolerance = mismatch");
    ASSERT_EQ(r.max_channel_diff, 10, "max diff = 10");
}

static void test_structural_match(void) {
    T3DShadowResult r = {
        .frame = 42,
        .hle_verts = 100, .emu_verts = 100,
        .hle_tris = 50, .emu_tris = 50,
        .hle_drawcalls = 5, .emu_drawcalls = 5,
    };
    r.verts_match = (r.hle_verts == r.emu_verts);
    r.tris_match = (r.hle_tris == r.emu_tris);
    r.drawcalls_match = (r.hle_drawcalls == r.emu_drawcalls);
    r.structural_match = r.verts_match && r.tris_match && r.drawcalls_match;

    ASSERT(r.structural_match, "all match = structural match");

    /* Log to /dev/null just to exercise the logger */
    FILE *devnull = fopen("/dev/null", "w");
    t3d_shadow_log_result(&r, devnull);
    fclose(devnull);
}

int main(void) {
    fprintf(stderr, "=== test_shadow ===\n");
    test_identical_frames();
    test_completely_different();
    test_tolerance();
    test_tolerance_exceeded();
    test_structural_match();
    TEST_SUMMARY();
}

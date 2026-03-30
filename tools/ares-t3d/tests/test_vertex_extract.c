/*
 * test_vertex_extract.c — Unit tests for big-endian vertex extraction
 *
 * Builds a synthetic T3DVertPacked buffer in fake RDRAM (big-endian)
 * and verifies the VERT_LOAD handler reads it correctly.
 */

#include "t3d_hle.h"
#include "test_helpers.h"

/* 16KB fake RDRAM — enough for vertex buffers at high offsets */
static uint8_t fake_rdram[16384];

static void test_single_packed_pair(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    /* Force overlay detection */
    state.detect.overlay_id = 5;
    state.detect.confirmed = true;

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* Build one T3DVertPacked (32 bytes) at RDRAM offset 0x2000.
     * Must be >= 0x1000 to pass VERT_LOAD address validation.
     * Vertex A: pos=(100, -200, 300), normal=0x1234, uv=(10, 20), rgba=(255,128,64,255)
     * Vertex B: pos=(-50, 75, -100), normal=0x5678, uv=(30, 40), rgba=(32,64,96,128) */
    uint32_t vaddr = 0x2000;
    /* posA: x=100, y=-200, z=300 */
    put_be16(fake_rdram, vaddr + 0, (uint16_t)100);
    put_be16(fake_rdram, vaddr + 2, (uint16_t)(int16_t)-200);
    put_be16(fake_rdram, vaddr + 4, (uint16_t)300);
    put_be16(fake_rdram, vaddr + 6, 0x1234); /* normA */
    /* posB: x=-50, y=75, z=-100 */
    put_be16(fake_rdram, vaddr + 8, (uint16_t)(int16_t)-50);
    put_be16(fake_rdram, vaddr + 10, (uint16_t)75);
    put_be16(fake_rdram, vaddr + 12, (uint16_t)(int16_t)-100);
    put_be16(fake_rdram, vaddr + 14, 0x5678); /* normB */
    /* uvA: s=10, t=20 */
    put_be16(fake_rdram, vaddr + 16, 10);
    put_be16(fake_rdram, vaddr + 18, 20);
    /* rgbaA */
    fake_rdram[vaddr + 20] = 255;
    fake_rdram[vaddr + 21] = 128;
    fake_rdram[vaddr + 22] = 64;
    fake_rdram[vaddr + 23] = 255;
    /* uvB: s=30, t=40 */
    put_be16(fake_rdram, vaddr + 24, 30);
    put_be16(fake_rdram, vaddr + 26, 40);
    /* rgbaB */
    fake_rdram[vaddr + 28] = 32;
    fake_rdram[vaddr + 29] = 64;
    fake_rdram[vaddr + 30] = 96;
    fake_rdram[vaddr + 31] = 128;

    /* Build VERT_LOAD command at offset 0.
     * Word 0: [ovl=5][cmd=4][input_size=32] (32 bytes = 1 packed pair = 2 verts)
     * Word 1: RDRAM address of vertex data
     * Word 2: [dest_offset << 16 | input_offset] */
    put_be32(fake_rdram, 0, ((5 << 28) | (T3D_CMD_VERT_LOAD << 24) | 32));
    put_be32(fake_rdram, 4, vaddr);
    put_be32(fake_rdram, 8, 0x00000000);

    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(consumed, 3, "VERT_LOAD consumes 3 words");
    ASSERT_EQ(state.vert_count, 2, "2 vertices loaded");

    /* Check vertex A */
    ASSERT_EQ((int)state.verts[0].pos[0], 100, "vertA x");
    ASSERT_EQ((int)state.verts[0].pos[1], -200, "vertA y");
    ASSERT_EQ((int)state.verts[0].pos[2], 300, "vertA z");
    ASSERT_EQ(state.verts[0].rgba[0], 255, "vertA r");
    ASSERT_EQ(state.verts[0].rgba[1], 128, "vertA g");
    ASSERT_EQ(state.verts[0].rgba[2], 64, "vertA b");
    ASSERT_EQ(state.verts[0].rgba[3], 255, "vertA a");
    ASSERT_EQ((int)state.verts[0].uv[0], 10, "vertA u");
    ASSERT_EQ((int)state.verts[0].uv[1], 20, "vertA v");

    /* Check vertex B */
    ASSERT_EQ((int)state.verts[1].pos[0], -50, "vertB x");
    ASSERT_EQ((int)state.verts[1].pos[1], 75, "vertB y");
    ASSERT_EQ((int)state.verts[1].pos[2], -100, "vertB z");
    ASSERT_EQ(state.verts[1].rgba[0], 32, "vertB r");
    ASSERT_EQ(state.verts[1].rgba[1], 64, "vertB g");
    ASSERT_EQ(state.verts[1].rgba[2], 96, "vertB b");
    ASSERT_EQ(state.verts[1].rgba[3], 128, "vertB a");
    ASSERT_EQ((int)state.verts[1].uv[0], 30, "vertB u");
    ASSERT_EQ((int)state.verts[1].uv[1], 40, "vertB v");

    ASSERT_EQ(state.frame.verts_loaded, 2, "frame counter updated");

    fclose(devnull);
}

static void test_vert_load_zero_size(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    state.detect.overlay_id = 5;
    state.detect.confirmed = true;

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* VERT_LOAD with input_size=0 */
    put_be32(fake_rdram, 0, ((5 << 28) | (T3D_CMD_VERT_LOAD << 24) | 0));
    put_be32(fake_rdram, 4, 0x100);
    put_be32(fake_rdram, 8, 0);

    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(consumed, -1, "zero-size VERT_LOAD: rejected (stop scan)");
    ASSERT_EQ(state.vert_count, 0, "zero-size VERT_LOAD: no verts");

    fclose(devnull);
}

static void test_vert_load_overflow(void) {
    T3DHLEState state;
    t3d_hle_init(&state, 0);
    state.verbose = false;
    FILE *devnull = fopen("/dev/null", "w");
    t3d_hle_set_log(&state, devnull);

    state.detect.overlay_id = 5;
    state.detect.confirmed = true;

    memset(fake_rdram, 0, sizeof(fake_rdram));

    /* VERT_LOAD with input_size = 2560 (>1120 max) — rejected as implausible */
    put_be32(fake_rdram, 0, ((5 << 28) | (T3D_CMD_VERT_LOAD << 24) | 2560));
    put_be32(fake_rdram, 4, 0x100);
    put_be32(fake_rdram, 8, 0);

    int consumed = t3d_hle_process_command(&state, fake_rdram, sizeof(fake_rdram), 0);

    ASSERT_EQ(consumed, -1, "oversized VERT_LOAD rejected (stop scan)");
    ASSERT_EQ(state.vert_count, 0, "no verts loaded on reject");

    fclose(devnull);
}

int main(void) {
    fprintf(stderr, "=== test_vertex_extract ===\n");
    test_single_packed_pair();
    test_vert_load_zero_size();
    test_vert_load_overflow();
    TEST_SUMMARY();
}

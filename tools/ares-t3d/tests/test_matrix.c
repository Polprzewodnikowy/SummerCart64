/*
 * test_matrix.c — Unit tests for T3DMat4FP conversion
 */

#include "matrix_convert.h"
#include "test_helpers.h"
#include <math.h>

static uint8_t fake_rdram[4096];

static void test_identity_matrix(void) {
    /* Build identity matrix in T3DMat4FP format:
     * Integer parts: diag=1, rest=0
     * Fraction parts: all 0 */
    memset(fake_rdram, 0, sizeof(fake_rdram));
    uint32_t addr = 0x100;
    /* Row-major 4x4 identity — int parts at indices 0,5,10,15 = 1 */
    put_be16(fake_rdram, addr + 0 * 2, 1);   /* [0][0] */
    put_be16(fake_rdram, addr + 5 * 2, 1);   /* [1][1] */
    put_be16(fake_rdram, addr + 10 * 2, 1);  /* [2][2] */
    put_be16(fake_rdram, addr + 15 * 2, 1);  /* [3][3] */
    /* Fraction parts at addr+32: all zero (already zeroed) */

    float out[16];
    bool ok = t3d_matrix_from_rdram(out, fake_rdram, sizeof(fake_rdram), addr);
    ASSERT(ok, "identity matrix read succeeds");

    /* Column-major identity: out[0]=1, out[5]=1, out[10]=1, out[15]=1 */
    ASSERT(fabsf(out[0] - 1.0f) < 0.001f, "m[0][0] = 1");
    ASSERT(fabsf(out[5] - 1.0f) < 0.001f, "m[1][1] = 1");
    ASSERT(fabsf(out[10] - 1.0f) < 0.001f, "m[2][2] = 1");
    ASSERT(fabsf(out[15] - 1.0f) < 0.001f, "m[3][3] = 1");
    ASSERT(fabsf(out[1]) < 0.001f, "m[1][0] = 0");
    ASSERT(fabsf(out[4]) < 0.001f, "m[0][1] = 0");
}

static void test_fractional_values(void) {
    memset(fake_rdram, 0, sizeof(fake_rdram));
    uint32_t addr = 0x200;

    /* Element [0][0]: int=2, frac=0x8000 (0.5) → 2.5 */
    put_be16(fake_rdram, addr + 0, 2);        /* int[0] = 2 */
    put_be16(fake_rdram, addr + 32 + 0, 0x8000); /* frac[0] = 0.5 */

    /* Element [1][1]: int=-1, frac=0x4000 (0.25) → -0.75 */
    put_be16(fake_rdram, addr + 5 * 2, (uint16_t)(int16_t)-1);
    put_be16(fake_rdram, addr + 32 + 5 * 2, 0x4000);

    float out[16];
    t3d_matrix_from_rdram(out, fake_rdram, sizeof(fake_rdram), addr);

    /* Column-major: out[0] = row[0][0], out[5] = row[1][1] */
    ASSERT(fabsf(out[0] - 2.5f) < 0.01f, "2 + 0.5 = 2.5");
    ASSERT(fabsf(out[5] - (-0.75f)) < 0.01f, "-1 + 0.25 = -0.75");
}

static void test_matrix_multiply(void) {
    float A[16], B[16], C[16];
    t3d_matrix_identity(A);
    t3d_matrix_identity(B);

    /* A = scale(2,3,4) */
    A[0] = 2.0f; A[5] = 3.0f; A[10] = 4.0f;

    /* B = identity */
    t3d_matrix_mul(C, A, B);

    ASSERT(fabsf(C[0] - 2.0f) < 0.001f, "A*I = A [0]");
    ASSERT(fabsf(C[5] - 3.0f) < 0.001f, "A*I = A [5]");
    ASSERT(fabsf(C[10] - 4.0f) < 0.001f, "A*I = A [10]");
}

static void test_out_of_bounds(void) {
    float out[16];
    bool ok = t3d_matrix_from_rdram(out, fake_rdram, 64, 0x100); /* too small */
    ASSERT(!ok, "out of bounds returns false");
}

int main(void) {
    fprintf(stderr, "=== test_matrix ===\n");
    test_identity_matrix();
    test_fractional_values();
    test_matrix_multiply();
    test_out_of_bounds();
    TEST_SUMMARY();
}

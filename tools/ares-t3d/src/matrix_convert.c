/*
 * matrix_convert.c — T3DMat4FP fixed-point ↔ float conversion
 */

#include "matrix_convert.h"
#include "t3d_hle.h" /* for t3d_hle_read16 */
#include <string.h>

bool t3d_matrix_from_rdram(float *out,
                           const uint8_t *rdram, uint32_t rdram_size,
                           uint32_t addr) {
    addr &= 0x1FFFFFFF;
    if (addr + 63 >= rdram_size) return false;

    /* T3DMat4FP layout (big-endian):
     * Bytes  0-31: 16 x int16_t (integer parts, row-major)
     * Bytes 32-63: 16 x uint16_t (fraction parts, row-major) */
    float row_major[16];
    for (int i = 0; i < 16; i++) {
        int16_t int_part  = (int16_t)t3d_hle_read16(rdram, rdram_size, addr + i * 2);
        uint16_t frac_part = t3d_hle_read16(rdram, rdram_size, addr + 32 + i * 2);
        row_major[i] = (float)int_part + (float)frac_part / 65536.0f;
    }

    /* Convert row-major to column-major for OpenGL:
     * row_major[row * 4 + col] → col_major[col * 4 + row] */
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[c * 4 + r] = row_major[r * 4 + c];

    return true;
}

void t3d_matrix_mul(float *C, const float *A, const float *B) {
    float tmp[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            tmp[c * 4 + r] =
                A[0 * 4 + r] * B[c * 4 + 0] +
                A[1 * 4 + r] * B[c * 4 + 1] +
                A[2 * 4 + r] * B[c * 4 + 2] +
                A[3 * 4 + r] * B[c * 4 + 3];
        }
    }
    memcpy(C, tmp, sizeof(tmp));
}

void t3d_matrix_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

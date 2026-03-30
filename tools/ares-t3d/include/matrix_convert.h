/*
 * matrix_convert.h — T3DMat4FP fixed-point to float conversion
 *
 * T3D stores matrices in RDRAM as T3DMat4FP:
 *   - 32 bytes: integer parts (s16 x 16, row-major)
 *   - 32 bytes: fraction parts (u16 x 16, row-major)
 *   Total: 64 bytes
 *
 * Each element: float = (float)int_part + (float)frac_part / 65536.0f
 *
 * The output is a 4x4 float matrix in column-major order (OpenGL convention).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Read a T3DMat4FP from big-endian RDRAM and convert to float[16] (column-major).
 * Returns false if address is out of bounds. */
bool t3d_matrix_from_rdram(float *out_col_major,
                           const uint8_t *rdram, uint32_t rdram_size,
                           uint32_t addr);

/* Multiply two 4x4 matrices (column-major): C = A * B */
void t3d_matrix_mul(float *C, const float *A, const float *B);

/* Set to identity */
void t3d_matrix_identity(float *m);

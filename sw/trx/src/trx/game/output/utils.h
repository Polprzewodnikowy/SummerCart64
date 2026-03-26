#pragma once

#include <trx/game/matrix.h>

#include <GL/glew.h>

#define OUTPUT_QUAD_VERTICES 6
#define OUTPUT_TRI_VERTICES 3

#define OUTPUT_TRI_TO_FAN(i) ((int32_t[]) { 0, 2, 1 }[i])
// |＼|
#define OUTPUT_QUAD_TO_FAN(i) ((int32_t[]) { 0, 2, 1, 0, 3, 2 }[i])
// |＼| Opposite winding
#define OUTPUT_QUAD_TO_FAN_CW(i) ((int32_t[]) { 0, 1, 2, 0, 2, 3 }[i])
// |／|
#define OUTPUT_QUAD_TO_FAN_BACK(i) ((int32_t[]) { 0, 3, 1, 1, 3, 2 }[i])

#define L_ATI_FIX 1

#if L_ATI_FIX
    // Evergreen‐generation GPUs (HD 5000/6000 – such as HD 5570 – have a
    // long-standing driver/firm-ware quirk: an integer vertex attribute whose
    // storage size is smaller than 32 bit is fetched as if it were 32-bit,
    // even when you declare it as 8- or 16-bit in glVertexAttribIPointer. The
    // call succeeds, no error is raised, but the hardware fetch unit still
    // steps four bytes, not one or two. If the stride you give is tighter than
    // that, the fetch unit starts reading the next vertex in the middle of the
    // current one and every attribute that follows is garbage – positions turn
    // into huge values, so the whole mesh explodes. NVIDIA, Intel and all
    // post-GCN AMD GPUs fixed the bug.
    //
    // To deal with this, we simply pack our data to increments of 4.
    #define OUTPUT_USHORT uint32_t
    #define OUTPUT_USHORT_GL GL_UNSIGNED_INT
    #define OUTPUT_SHORT int32_t
    #define OUTPUT_SHORT_GL GL_INT
#else
    #define OUTPUT_SHORT int16_t
    #define OUTPUT_SHORT_GL GL_SHORT
    #define OUTPUT_USHORT uint16_t
    #define OUTPUT_USHORT_GL GL_UNSIGNED_SHORT
#endif

#undef L_ATI_FIX

void Output_FillMatrix(GLfloat m[4][4], const MATRIX *source);

#pragma once

#include <stdint.h>

typedef struct {
    int32_t buffer_transfer_count;
    int32_t buffer_total_bytes;
    int32_t uniform_changes;
    int32_t opaque_vert_count;
    int32_t trans_vert_count;
    int32_t blend_add_vert_count;
} TRX_GL_METRICS;

extern TRX_GL_METRICS g_TRX_GL_Metrics;

#define TRX_GL_TRACK_UNIFORM(fn, ...)                                          \
    do {                                                                       \
        g_TRX_GL_Metrics.uniform_changes++;                                    \
        fn(__VA_ARGS__);                                                       \
    } while (0);

#define TRX_GL_TRACK_DATA(fn, a, b, c, d)                                      \
    do {                                                                       \
        g_TRX_GL_Metrics.buffer_total_bytes += b;                              \
        g_TRX_GL_Metrics.buffer_transfer_count++;                              \
        fn(a, b, c, d);                                                        \
    } while (0);

#define TRX_GL_TRACK_SUBDATA(fn, a, b, c, d)                                   \
    do {                                                                       \
        g_TRX_GL_Metrics.buffer_total_bytes += c;                              \
        g_TRX_GL_Metrics.buffer_transfer_count++;                              \
        fn(a, b, c, d);                                                        \
    } while (0);

void TRX_GL_Track_Reset(void);
TRX_GL_METRICS TRX_GL_Track_GetMetrics(void);

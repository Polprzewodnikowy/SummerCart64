#pragma once

#include <trx/core/vector.h>

typedef struct {
    int32_t vertex_start;
    int32_t vertex_count;
} OUTPUT_VERTEX_RANGE;

void Output_GlueVertexRanges(VECTOR *vertex_range);

#include <trx/game/output/vertex_range.h>

#include <trx/debug.h>

#include <stdlib.h>

static int M_CompareRanges(const void *const a, const void *const b)
{
    const OUTPUT_VERTEX_RANGE *const range_a = (OUTPUT_VERTEX_RANGE *)a;
    const OUTPUT_VERTEX_RANGE *const range_b = (OUTPUT_VERTEX_RANGE *)b;
    return range_a->vertex_start - range_b->vertex_start;
}

void Output_GlueVertexRanges(VECTOR *const target)
{
    ASSERT(target != nullptr);
    if (target->count == 0) {
        return;
    }

    OUTPUT_VERTEX_RANGE *const ranges =
        (OUTPUT_VERTEX_RANGE *)Vector_Get(target, 0);

    qsort(ranges, target->count, sizeof(OUTPUT_VERTEX_RANGE), M_CompareRanges);

    // Initialize a new index to store the merged ranges
    int32_t new_range_count = 0;

    // Iterate over sorted ranges and merge them
    for (int32_t i = 0; i < target->count; i++) {
        if (new_range_count == 0) {
            // First range - just copy it
            ranges[new_range_count] = ranges[i];
            new_range_count++;
        } else {
            // Check if the previous range can be merged with the current one
            OUTPUT_VERTEX_RANGE *const last_range =
                &ranges[new_range_count - 1];
            const int32_t last_start = last_range->vertex_start;
            const int32_t last_end =
                last_range->vertex_start + last_range->vertex_count;
            const int32_t current_start = ranges[i].vertex_start;
            const int32_t current_end =
                ranges[i].vertex_start + ranges[i].vertex_count;

            if (current_start >= last_start && current_start <= last_end) {
                last_range->vertex_count =
                    current_end - last_range->vertex_start;
            } else if (current_end >= last_start && current_end <= last_end) {
                last_range->vertex_start = ranges[i].vertex_start;
            } else {
                ranges[new_range_count++] = ranges[i];
            }
        }
    }

    // Update the range vertex_count with the new number of merged ranges
    target->count = new_range_count;
}

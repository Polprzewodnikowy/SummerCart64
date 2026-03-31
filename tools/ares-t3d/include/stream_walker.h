/*
 * stream_walker.h — Correct mixed rspq stream walker
 *
 * Walks a rspq command buffer using known command sizes for
 * rspq internal, rdpq, and T3D overlays. Correctly steps over
 * variable-length commands instead of guessing.
 *
 * Requires knowing which overlay slot is rdpq and which is T3D.
 * Both are detected at runtime from the command stream itself.
 */

#pragma once

#include "t3d_hle.h"
#include <stdint.h>

/* Overlay slot mapping — learned at runtime */
typedef struct {
    int rdpq_slot;   /* -1 = unknown */
    int t3d_slot;    /* -1 = unknown */
    bool rdpq_confirmed;
    bool t3d_confirmed;
} T3DOverlayMap;

/* Walk a DMA buffer correctly and dispatch T3D commands.
 * Returns number of T3D commands found. */
int t3d_stream_walk(T3DHLEState *state,
                    const uint8_t *rdram, uint32_t rdram_size,
                    uint32_t buf_addr, uint32_t buf_len,
                    T3DOverlayMap *ovl_map);

/* Attempt to identify overlay slots from command patterns.
 * Call on early DMA buffers to build the overlay map. */
void t3d_stream_identify_overlays(const uint8_t *rdram, uint32_t rdram_size,
                                  uint32_t buf_addr, uint32_t buf_len,
                                  T3DOverlayMap *ovl_map);

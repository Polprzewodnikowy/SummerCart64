/*
 * segment_classifier.h — Per-DMA-buffer segment classification
 *
 * Instead of "overlay 1 = T3D", classify each DMA buffer segment
 * independently based on command sequence patterns.
 *
 * A segment is T3D if it contains a recognizable T3D sequence:
 *   strong: DRAWFLAGS + VERT_LOAD + TRI_DRAW + TRI_SYNC
 *   medium: SCREEN_SIZE + PROJ_SET nearby
 *   weak:   isolated TRI_DRAW (could be rdpq, don't trust alone)
 *
 * This allows the same overlay slot to be T3D in one DMA buffer
 * and rdpq in another, without global state confusion.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* Classification result for one DMA buffer segment */
typedef enum {
    SEG_UNKNOWN = 0,
    SEG_RSPQ_INTERNAL,   /* overlay 0 commands only */
    SEG_T3D_STRONG,      /* has DRAWFLAGS + VERT_LOAD or VERT_LOAD + TRI_DRAW */
    SEG_T3D_MEDIUM,      /* has SCREEN_SIZE or PROJ_SET + another T3D cmd */
    SEG_T3D_WEAK,        /* has isolated T3D-looking commands (untrusted) */
    SEG_RDPQ_LIKELY,     /* looks like rdpq (RDP state commands) */
    SEG_MIXED,           /* multiple overlays, can't classify */
} T3DSegmentClass;

/* Per-DMA-buffer analysis */
typedef struct {
    T3DSegmentClass classification;
    uint32_t dram_addr;
    uint32_t length;

    /* Command signature flags (which T3D commands were seen) */
    bool saw_drawflags;
    bool saw_vert_load;    /* with sane size AND address */
    bool saw_tri_draw;
    bool saw_tri_sync;
    bool saw_screen_size;
    bool saw_proj_set;
    bool saw_matrix_stack;
    bool saw_light_set;
    bool saw_fog;

    /* Overlay slots seen in this segment */
    uint16_t overlay_slots_seen;  /* bitmask: bit N = overlay N seen */
    int dominant_overlay;          /* overlay with most commands */

    /* Command counts by type */
    int cmd_count;
    int rspq_count;
    int t3d_candidate_count;  /* commands that COULD be T3D */
    int other_count;
} T3DSegmentAnalysis;

/* Analyze a DMA buffer and classify it.
 * Does NOT modify any parser state — pure observation.
 * t3d_overlay_hint: -1 for no hint, or suspected T3D overlay ID */
T3DSegmentAnalysis t3d_segment_classify(
    const uint8_t *rdram, uint32_t rdram_size,
    uint32_t dram_addr, uint32_t length,
    int t3d_overlay_hint);

/* Get classification name string */
const char *t3d_segment_class_name(T3DSegmentClass cls);

/* Log a segment analysis (machine-readable) */
void t3d_segment_log(const T3DSegmentAnalysis *seg, FILE *fp, uint64_t frame);

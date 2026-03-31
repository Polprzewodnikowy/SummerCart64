/*
 * segment_classifier.c — Per-DMA-buffer T3D segment classification
 *
 * Scans a DMA buffer word by word and builds a feature vector of
 * which command signatures are present. Then classifies the segment
 * based on the combination of features, not individual command matches.
 */

#include "segment_classifier.h"
#include "t3d_hle.h"
#include <string.h>

/* ----------------------------------------------------------------
 * VERT_LOAD plausibility check (standalone, no parser state)
 * ---------------------------------------------------------------- */
static bool vert_load_plausible(uint32_t w0, uint32_t w1, uint32_t rdram_size) {
    uint32_t input_size = RSPQ_ARGS(w0);
    uint32_t phys_addr = w1 & 0x1FFFFFFF;
    (void)rdram_size;

    /* Must be: nonzero, multiple of 16, max 1120 bytes */
    if (input_size == 0 || input_size > 1120 || (input_size % 16) != 0)
        return false;
    /* Broad address check — any non-zero RDRAM address */
    if (phys_addr == 0)
        return false;
    return true;
}

/* ----------------------------------------------------------------
 * Classify one DMA buffer segment
 * ---------------------------------------------------------------- */
T3DSegmentAnalysis t3d_segment_classify(
    const uint8_t *rdram, uint32_t rdram_size,
    uint32_t dram_addr, uint32_t length,
    int t3d_overlay_hint)
{
    T3DSegmentAnalysis seg;
    memset(&seg, 0, sizeof(seg));
    seg.dram_addr = dram_addr;
    seg.length = length;
    seg.dominant_overlay = -1;

    uint32_t phys = dram_addr & 0x1FFFFFFF;
    uint32_t end = phys + length;
    if (end > rdram_size) end = rdram_size;

    /* Overlay command counts for dominance detection */
    int ovl_counts[16] = {0};

    /* Scan word by word — pure classification, no parsing */
    for (uint32_t pos = phys; pos + 3 < end; pos += 4) {
        uint32_t w0 = t3d_hle_read32(rdram, rdram_size, pos);
        if (w0 == 0) continue;

        int ovl = RSPQ_OVL_ID(w0);
        int cmd = RSPQ_CMD_ID(w0);

        seg.cmd_count++;
        if (ovl < 16) {
            seg.overlay_slots_seen |= (1 << ovl);
            ovl_counts[ovl]++;
        }

        if (ovl == 0) {
            seg.rspq_count++;
            continue;
        }

        /* Check for T3D command signatures in ANY overlay slot.
         * The key insight: check cmd_id patterns regardless of overlay,
         * then correlate with the overlay slot to identify which slot
         * is actually T3D in this segment. */

        /* VERT_LOAD (cmd=4): check plausibility with next word */
        if (cmd == T3D_CMD_VERT_LOAD && pos + 7 < end) {
            uint32_t w1 = t3d_hle_read32(rdram, rdram_size, pos + 4);
            if (vert_load_plausible(w0, w1, rdram_size)) {
                seg.saw_vert_load = true;
                seg.t3d_candidate_count++;
                /* Note which overlay this came from */
                if (t3d_overlay_hint < 0) t3d_overlay_hint = ovl;
            }
        }

        /* DRAWFLAGS (cmd=6): always 2 words */
        if (cmd == T3D_CMD_DRAWFLAGS) {
            seg.saw_drawflags = true;
            seg.t3d_candidate_count++;
        }

        /* TRI_DRAW (cmd=0): too weak alone, but note it */
        if (cmd == T3D_CMD_TRI_DRAW) {
            seg.saw_tri_draw = true;
        }

        /* TRI_SYNC (cmd=0xA) */
        if (cmd == T3D_CMD_TRI_SYNC) {
            seg.saw_tri_sync = true;
            seg.t3d_candidate_count++;
        }

        /* SCREEN_SIZE (cmd=1): distinctive — 5 words */
        if (cmd == T3D_CMD_SCREEN_SIZE) {
            seg.saw_screen_size = true;
            seg.t3d_candidate_count++;
        }

        /* PROJ_SET (cmd=7) */
        if (cmd == T3D_CMD_PROJ_SET) {
            seg.saw_proj_set = true;
            seg.t3d_candidate_count++;
        }

        /* MATRIX_STACK (cmd=2) */
        if (cmd == T3D_CMD_MATRIX_STACK) {
            seg.saw_matrix_stack = true;
            seg.t3d_candidate_count++;
        }

        /* LIGHT_SET (cmd=5) */
        if (cmd == T3D_CMD_LIGHT_SET) {
            seg.saw_light_set = true;
            seg.t3d_candidate_count++;
        }

        /* FOG (cmd=8 or 9) */
        if (cmd == T3D_CMD_FOG_RANGE || cmd == T3D_CMD_FOG_STATE) {
            seg.saw_fog = true;
            seg.t3d_candidate_count++;
        }

        if (ovl != 0) seg.other_count++;
    }

    /* Find dominant overlay (excluding overlay 0) */
    int max_count = 0;
    for (int i = 1; i < 16; i++) {
        if (ovl_counts[i] > max_count) {
            max_count = ovl_counts[i];
            seg.dominant_overlay = i;
        }
    }

    /* ----------------------------------------------------------------
     * Classification based on feature combination
     * ---------------------------------------------------------------- */

    /* STRONG: has the block recording signature */
    if (seg.saw_vert_load && (seg.saw_tri_draw || seg.saw_tri_sync)) {
        seg.classification = SEG_T3D_STRONG;
    }
    else if (seg.saw_drawflags && seg.saw_vert_load) {
        seg.classification = SEG_T3D_STRONG;
    }
    /* MEDIUM: has viewport/projection setup */
    else if (seg.saw_screen_size && (seg.saw_proj_set || seg.saw_matrix_stack)) {
        seg.classification = SEG_T3D_MEDIUM;
    }
    else if (seg.saw_matrix_stack && seg.saw_light_set) {
        seg.classification = SEG_T3D_MEDIUM;
    }
    /* WEAK: isolated T3D-ish commands */
    else if (seg.t3d_candidate_count > 0) {
        seg.classification = SEG_T3D_WEAK;
    }
    /* RSPQ only */
    else if (seg.rspq_count > 0 && seg.other_count == 0) {
        seg.classification = SEG_RSPQ_INTERNAL;
    }
    /* Unknown */
    else {
        seg.classification = SEG_UNKNOWN;
    }

    return seg;
}

const char *t3d_segment_class_name(T3DSegmentClass cls) {
    switch (cls) {
    case SEG_UNKNOWN:       return "UNKNOWN";
    case SEG_RSPQ_INTERNAL: return "RSPQ";
    case SEG_T3D_STRONG:    return "T3D_STRONG";
    case SEG_T3D_MEDIUM:    return "T3D_MEDIUM";
    case SEG_T3D_WEAK:      return "T3D_WEAK";
    case SEG_RDPQ_LIKELY:   return "RDPQ";
    case SEG_MIXED:         return "MIXED";
    default:                return "?";
    }
}

void t3d_segment_log(const T3DSegmentAnalysis *seg, FILE *fp, uint64_t frame) {
    fprintf(fp,
        "T3D_SEGMENT frame=%llu dram=0x%08X len=%u class=%s "
        "cmds=%d rspq=%d t3d_cand=%d other=%d "
        "dom_ovl=%d ovl_mask=0x%04X "
        "vload=%d dflags=%d tdraw=%d tsync=%d ssize=%d proj=%d mtx=%d light=%d fog=%d\n",
        (unsigned long long)frame,
        seg->dram_addr, seg->length,
        t3d_segment_class_name(seg->classification),
        seg->cmd_count, seg->rspq_count, seg->t3d_candidate_count, seg->other_count,
        seg->dominant_overlay, seg->overlay_slots_seen,
        seg->saw_vert_load, seg->saw_drawflags, seg->saw_tri_draw, seg->saw_tri_sync,
        seg->saw_screen_size, seg->saw_proj_set, seg->saw_matrix_stack,
        seg->saw_light_set, seg->saw_fog);
}

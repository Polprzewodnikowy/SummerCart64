/*
 * shadow_render.h — Tier 2: Shadow render mode
 *
 * Renders T3D geometry via host GL alongside normal N64 emulation.
 * After each frame, compares:
 *   - Vertex count
 *   - Triangle count
 *   - Draw call count
 *   - Frame bounds (scissor/viewport)
 *   - State changes
 *   - Optionally: pixel diff of rendered output
 *
 * Output is machine-readable:
 *   T3D_SHADOW_FRAME frame=N match=<yes/no> verts=N/N tris=N/N ...
 *   T3D_SHADOW_DIFF pixel_diff=N max_diff=N mismatch_pct=N
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* Comparison results for a single frame */
typedef struct {
    uint32_t frame;

    /* Structural comparison */
    uint32_t hle_verts, emu_verts;
    uint32_t hle_tris, emu_tris;
    uint32_t hle_drawcalls, emu_drawcalls;
    uint32_t hle_state_changes;

    /* Match flags */
    bool verts_match;
    bool tris_match;
    bool drawcalls_match;
    bool structural_match;  /* all structural checks pass */

    /* Pixel comparison (when available) */
    bool pixel_compared;
    uint32_t pixel_total;
    uint32_t pixel_mismatch;
    float mismatch_pct;
    uint32_t max_channel_diff;
} T3DShadowResult;

/* Log a shadow comparison result (machine-readable) */
void t3d_shadow_log_result(const T3DShadowResult *r, FILE *fp);

/* Compare two RGBA8888 framebuffers and fill pixel comparison fields.
 * tolerance: max allowed per-channel difference (0 = exact match) */
void t3d_shadow_compare_pixels(T3DShadowResult *r,
                               const uint8_t *hle_fb,
                               const uint8_t *emu_fb,
                               int width, int height,
                               int tolerance);

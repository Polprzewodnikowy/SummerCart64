/*
 * shadow_render.c — Tier 2: Shadow render comparison
 */

#include "shadow_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void t3d_shadow_log_result(const T3DShadowResult *r, FILE *fp) {
    fprintf(fp,
            "T3D_SHADOW_FRAME frame=%u "
            "verts=%u/%u tris=%u/%u drawcalls=%u/%u "
            "verts_match=%d tris_match=%d dc_match=%d structural=%d",
            r->frame,
            r->hle_verts, r->emu_verts,
            r->hle_tris, r->emu_tris,
            r->hle_drawcalls, r->emu_drawcalls,
            r->verts_match, r->tris_match,
            r->drawcalls_match, r->structural_match);

    if (r->pixel_compared) {
        fprintf(fp,
                " pixel_total=%u pixel_mismatch=%u mismatch_pct=%.2f max_diff=%u",
                r->pixel_total, r->pixel_mismatch,
                r->mismatch_pct, r->max_channel_diff);
    }
    fprintf(fp, "\n");
}

void t3d_shadow_compare_pixels(T3DShadowResult *r,
                               const uint8_t *hle_fb,
                               const uint8_t *emu_fb,
                               int width, int height,
                               int tolerance) {
    r->pixel_compared = true;
    r->pixel_total = width * height;
    r->pixel_mismatch = 0;
    r->max_channel_diff = 0;

    for (int i = 0; i < width * height; i++) {
        const uint8_t *h = &hle_fb[i * 4];
        const uint8_t *e = &emu_fb[i * 4];

        bool match = true;
        for (int c = 0; c < 4; c++) {
            int diff = abs((int)h[c] - (int)e[c]);
            if ((uint32_t)diff > r->max_channel_diff)
                r->max_channel_diff = diff;
            if (diff > tolerance)
                match = false;
        }
        if (!match) r->pixel_mismatch++;
    }

    r->mismatch_pct = r->pixel_total > 0
        ? (float)r->pixel_mismatch / (float)r->pixel_total * 100.0f
        : 0.0f;
}

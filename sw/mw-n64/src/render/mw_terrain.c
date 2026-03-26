/*
 * MW-N64: Terrain renderer — implementation
 *
 * Converts heightmaps to triangle strips at selectable LOD.
 * Vertex colors from VCLR are preserved exactly.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_terrain.h"

#include <string.h>

/* External: bump allocator from platform layer */
extern void *n64_mem_alloc(uint32_t size);

/* Height scale factor (exact Morrowind value) */
#define HEIGHT_SCALE    8.0f

void terrain_build_mesh(const esm_land_t *land,
                         int lod,
                         terrain_mesh_t *out) {
    memset(out, 0, sizeof(*out));
    out->grid_x = land->grid_x;
    out->grid_y = land->grid_y;
    out->lod = lod;

    /* Step size based on LOD */
    int step;
    switch (lod) {
        case TERRAIN_LOD_HALF:    step = 2; break;
        case TERRAIN_LOD_QUARTER: step = 4; break;
        default:                  step = 1; break;
    }

    int verts_per_edge = (64 / step) + 1;
    out->vertex_count = verts_per_edge * verts_per_edge;
    out->vertices = (terrain_vertex_t *)n64_mem_alloc(
        out->vertex_count * sizeof(terrain_vertex_t));
    if (!out->vertices) {
        out->vertex_count = 0;
        return;
    }

    /*
     * Build vertex grid from heightmap.
     * World position of vertex (ix, iy) in cell (grid_x, grid_y):
     *   world_x = grid_x * MW_CELL_SIZE + ix * (MW_CELL_SIZE / 64)
     *   world_y = grid_y * MW_CELL_SIZE + iy * (MW_CELL_SIZE / 64)
     *   world_z = heights[iy * 65 + ix] * HEIGHT_SCALE
     *
     * Morrowind uses X=east, Y=north, Z=up.
     * N64 uses X=right, Y=down, Z=into screen.
     * We map: MW_X → N64_X, MW_Y → N64_Z, MW_Z → N64_(-Y)
     */
    float cell_origin_x = (float)land->grid_x * MW_CELL_SIZE;
    float cell_origin_y = (float)land->grid_y * MW_CELL_SIZE;
    float vert_spacing = (float)MW_CELL_SIZE / 64.0f;

    int vi = 0;
    for (int iy = 0; iy < 65; iy += step) {
        for (int ix = 0; ix < 65; ix += step) {
            terrain_vertex_t *v = &out->vertices[vi++];

            /* World position (stored as floats, projected later) */
            /* We pack world coords into int16 with scale factor.
             * Actual projection happens in terrain_draw(). */
            float wx = cell_origin_x + ix * vert_spacing;
            float wy = cell_origin_y + iy * vert_spacing;
            float wz = land->heights[iy * 65 + ix] * HEIGHT_SCALE;

            /* Store world coords packed (divided by 4 to fit int16 range) */
            v->x = (int16_t)(wx / 4.0f);
            v->y = (int16_t)(-wz / 4.0f);  /* MW Z-up → N64 Y-down */
            v->z = (int16_t)(wy / 4.0f);

            /* Vertex color from VCLR (exact Morrowind values) */
            if (land->has_colors) {
                int ci = (iy * 65 + ix) * 3;
                v->r = land->colors[ci + 0];
                v->g = land->colors[ci + 1];
                v->b = land->colors[ci + 2];
            } else {
                /* Default: moderate grey-green for Morrowind landscape */
                v->r = 140;
                v->g = 140;
                v->b = 120;
            }

            v->u = 0;  /* Texture coords set during draw */
            v->v = 0;
            v->clip = 0;
        }
    }

    out->vertex_count = vi;
    out->valid = true;
}

int terrain_select_lod(int32_t cell_x, int32_t cell_y,
                        float cam_x, float cam_y) {
    /* Cell center in world coords */
    float cx = (cell_x + 0.5f) * MW_CELL_SIZE;
    float cy = (cell_y + 0.5f) * MW_CELL_SIZE;

    float dx = cam_x - cx;
    float dy = cam_y - cy;
    float dist_sq = dx * dx + dy * dy;

    /* LOD thresholds (in world units squared) */
    float near_sq = (float)MW_CELL_SIZE * MW_CELL_SIZE;       /* 1 cell */
    float mid_sq = near_sq * 4.0f;                             /* 2 cells */

    if (dist_sq < near_sq) return TERRAIN_LOD_FULL;
    if (dist_sq < mid_sq)  return TERRAIN_LOD_HALF;
    return TERRAIN_LOD_QUARTER;
}

void terrain_draw(const terrain_mesh_t *mesh,
                   const void *transform_state,
                   void *rdp_buf) {
    if (!mesh || !mesh->valid || !mesh->vertices) return;

    /*
     * TODO: Project terrain vertices through VP matrix,
     * emit triangle strips to RDP. Uses the same pipeline
     * as TR1's room renderer (n64_render.h / n64_transform.h).
     *
     * Triangle strip order for grid:
     *   For each row y:
     *     For each col x:
     *       tri1: (x,y), (x+1,y), (x,y+1)
     *       tri2: (x+1,y), (x+1,y+1), (x,y+1)
     */
    (void)transform_state;
    (void)rdp_buf;
}

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
                v->r = 140;
                v->g = 140;
                v->b = 120;
            }

            /* UV: tile the terrain texture across the cell.
             * 16x16 texture grid per cell → each quad gets 0..1 UV range.
             * In 10.5 fixed: 1 texture tile = 32 units. */
            v->u = (int16_t)((ix % 4) * 32 / 4);
            v->v = (int16_t)((iy % 4) * 32 / 4);
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

/*
 * Emit a flat-shaded RDP triangle from 3 screen-space vertices.
 * Simplified version of TR1's emit_rdp_triangle — shade only, no texture.
 * Used for terrain rendering when Z-buffer is off (Lambert technique).
 */
static void emit_terrain_tri(void *rdp_buf_ptr,
                              int16_t x0, int16_t y0, uint8_t r0, uint8_t g0, uint8_t b0,
                              int16_t x1, int16_t y1, uint8_t r1, uint8_t g1, uint8_t b1,
                              int16_t x2, int16_t y2, uint8_t r2, uint8_t g2, uint8_t b2) {
    /* Sort by Y (top to bottom) */
    if (y1 < y0) { int16_t t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t;
                    uint8_t u; u=r0;r0=r1;r1=u; u=g0;g0=g1;g1=u; u=b0;b0=b1;b1=u; }
    if (y2 < y0) { int16_t t; t=x0;x0=x2;x2=t; t=y0;y0=y2;y2=t;
                    uint8_t u; u=r0;r0=r2;r2=u; u=g0;g0=g2;g2=u; u=b0;b0=b2;b2=u; }
    if (y2 < y1) { int16_t t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t;
                    uint8_t u; u=r1;r1=r2;r2=u; u=g1;g1=g2;g2=u; u=b1;b1=b2;b2=u; }

    if (y0 == y2) return;  /* Degenerate */

    /* Flat-shade with average color */
    uint8_t ar = ((int)r0+r1+r2)/3, ag = ((int)g0+g1+g2)/3, ab = ((int)b0+b1+b2)/3;

    /* Build RDP Shade Triangle (opcode 0x0C) — same as TR1 */
    int32_t yh = y0 << 2, ym = y1 << 2, yl = y2 << 2;
    int32_t xh = x0 << 16, xm = x0 << 16, xl = x1 << 16;
    int32_t dxhdy = 0, dxmdy = 0, dxldy = 0;
    int32_t dy_hl = y2-y0, dy_hm = y1-y0, dy_ml = y2-y1;
    if (dy_hl > 0) dxhdy = ((x2-x0) << 16) / dy_hl;
    if (dy_hm > 0) dxmdy = ((x1-x0) << 16) / dy_hm;
    if (dy_ml > 0) dxldy = ((x2-x1) << 16) / dy_ml;

    int32_t mid_x = (x0 << 16) + dxhdy * dy_hm;
    int lm = (x1 << 16) > mid_x ? 1 : 0;

    /* We're passed a void* for the rdp buffer — cast and use inline emit */
    typedef struct { uint64_t *cmds; uint32_t cmd_count; uint32_t cmd_capacity; } rdb_t;
    rdb_t *buf = (rdb_t *)rdp_buf_ptr;
    #define EMIT(cmd) do { if(buf->cmd_count < buf->cmd_capacity) buf->cmds[buf->cmd_count++]=(cmd); } while(0)

    EMIT(((uint64_t)0x0C<<56) | ((uint64_t)(lm&1)<<55) | ((uint64_t)(yl&0x3FFF)<<32) | ((uint64_t)(ym&0x3FFF)<<16) | (uint64_t)(yh&0x3FFF));
    EMIT(((uint64_t)(xl&0xFFFFFFFF)<<32) | (uint64_t)(dxldy&0xFFFFFFFF));
    EMIT(((uint64_t)(xh&0xFFFFFFFF)<<32) | (uint64_t)(dxhdy&0xFFFFFFFF));
    EMIT(((uint64_t)(xm&0xFFFFFFFF)<<32) | (uint64_t)(dxmdy&0xFFFFFFFF));
    /* Shade: flat color */
    EMIT(((uint64_t)ar<<56)|((uint64_t)ag<<40)|((uint64_t)ab<<24)|((uint64_t)0xFF<<8));
    EMIT(0); /* dRGBAdX */
    EMIT(0); /* dRGBAdE */
    EMIT(0); /* dRGBAdY */
    #undef EMIT
}

void terrain_draw(const terrain_mesh_t *mesh,
                   const void *transform_state,
                   void *rdp_buf) {
    if (!mesh || !mesh->valid || !mesh->vertices) return;
    (void)transform_state;  /* TODO: project through VP matrix */

    int step = 1;
    switch (mesh->lod) {
        case TERRAIN_LOD_HALF:    step = 2; break;
        case TERRAIN_LOD_QUARTER: step = 4; break;
        default:                  step = 1; break;
    }
    int verts_per_edge = (64 / step) + 1;

    /*
     * Emit triangle grid: for each cell in the heightmap grid,
     * emit 2 triangles. Back-to-front order (far rows first)
     * for Z-buffer-off rendering (Lambert technique).
     *
     * TODO: actual vertex projection through VP matrix.
     * Currently passes raw packed coords — needs the transform
     * pipeline from TR1's renderer to project to screen space.
     * The terrain vertices store world coords / 4 in int16.
     */
    for (int y = verts_per_edge - 2; y >= 0; y--) {
        for (int x = 0; x < verts_per_edge - 1; x++) {
            int i00 = y * verts_per_edge + x;
            int i10 = y * verts_per_edge + x + 1;
            int i01 = (y + 1) * verts_per_edge + x;
            int i11 = (y + 1) * verts_per_edge + x + 1;

            terrain_vertex_t *v00 = &mesh->vertices[i00];
            terrain_vertex_t *v10 = &mesh->vertices[i10];
            terrain_vertex_t *v01 = &mesh->vertices[i01];
            terrain_vertex_t *v11 = &mesh->vertices[i11];

            /* Triangle 1: (x,y), (x+1,y), (x,y+1) */
            emit_terrain_tri(rdp_buf,
                v00->x, v00->y, v00->r, v00->g, v00->b,
                v10->x, v10->y, v10->r, v10->g, v10->b,
                v01->x, v01->y, v01->r, v01->g, v01->b);

            /* Triangle 2: (x+1,y), (x+1,y+1), (x,y+1) */
            emit_terrain_tri(rdp_buf,
                v10->x, v10->y, v10->r, v10->g, v10->b,
                v11->x, v11->y, v11->r, v11->g, v11->b,
                v01->x, v01->y, v01->r, v01->g, v01->b);
        }
    }
}

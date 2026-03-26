/*
 * MW-N64: Terrain renderer
 *
 * Converts Morrowind's 65×65 heightmap cells to N64 RDP triangle strips.
 * Each cell = 64×64 quads = 8192 triangles at full detail.
 * At LOD 1 (half res): 32×32 = 2048 triangles — good for N64 budget.
 *
 * Terrain vertex colors come from VCLR data (exact Morrowind values).
 * Terrain texturing uses VTEX indices mapped to pre-converted CI8 tiles.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_TERRAIN_H
#define MW_TERRAIN_H

#include "../esm/mw_esm.h"
#include "../world/mw_cell_manager.h"
#include <stdint.h>

/* LOD levels for terrain */
#define TERRAIN_LOD_FULL    0   /* 64×64 quads = 8192 tris */
#define TERRAIN_LOD_HALF    1   /* 32×32 quads = 2048 tris */
#define TERRAIN_LOD_QUARTER 2   /* 16×16 quads = 512 tris */

/* Terrain vertex (ready for RDP submission) */
typedef struct {
    int16_t x, y, z;       /* Screen-space position */
    uint8_t r, g, b;       /* Vertex color */
    int16_t u, v;           /* Texture coords (10.5 fixed) */
    uint8_t clip;           /* Frustum clip flags */
} terrain_vertex_t;

/* Pre-transformed terrain mesh for one cell at one LOD */
typedef struct {
    terrain_vertex_t *vertices;
    int               vertex_count;
    int               lod;
    int32_t           grid_x, grid_y;
    bool              valid;
} terrain_mesh_t;

/*
 * Build terrain mesh from heightmap data.
 * Allocates vertices from bump allocator.
 * lod: 0=full (65×65), 1=half (33×33), 2=quarter (17×17)
 */
void terrain_build_mesh(const esm_land_t *land,
                         int lod,
                         terrain_mesh_t *out);

/*
 * Transform and draw terrain for one cell.
 * Projects vertices through the VP matrix and emits RDP triangles.
 */
void terrain_draw(const terrain_mesh_t *mesh,
                   const void *transform_state,  /* transform_state_t* */
                   void *rdp_buf);               /* rdp_cmd_buffer_t* */

/*
 * Select LOD based on distance from camera to cell center.
 */
int terrain_select_lod(int32_t cell_x, int32_t cell_y,
                        float cam_x, float cam_y);

#endif /* MW_TERRAIN_H */

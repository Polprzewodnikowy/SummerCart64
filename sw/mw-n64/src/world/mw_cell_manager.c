/*
 * MW-N64: Cell streaming manager — implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_cell_manager.h"
#include <string.h>

/* -------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------- */

/* Convert world position to cell grid coordinates */
static void world_to_cell(float wx, float wy, int32_t *cx, int32_t *cy) {
    *cx = (int32_t)(wx / MW_CELL_SIZE);
    *cy = (int32_t)(wy / MW_CELL_SIZE);
    if (wx < 0 && ((int32_t)wx % MW_CELL_SIZE) != 0) (*cx)--;
    if (wy < 0 && ((int32_t)wy % MW_CELL_SIZE) != 0) (*cy)--;
}

/* Map grid coords to cell array slot (3×3 wrapping) */
static int grid_to_slot(int32_t gx, int32_t gy, int32_t center_x, int32_t center_y) {
    int dx = gx - center_x + CELL_GRID_RADIUS;
    int dy = gy - center_y + CELL_GRID_RADIUS;
    if (dx < 0 || dx >= CELL_GRID_SIZE || dy < 0 || dy >= CELL_GRID_SIZE)
        return -1;
    return dy * CELL_GRID_SIZE + dx;
}

/* -------------------------------------------------------------------
 * Load/unload individual cells
 * ------------------------------------------------------------------- */

static void load_exterior_cell(cell_manager_t *cm, loaded_cell_t *lc,
                                int32_t gx, int32_t gy) {
    memset(lc, 0, sizeof(*lc));
    lc->grid_x = gx;
    lc->grid_y = gy;
    lc->is_interior = false;

    /* Load cell data (object placements, flags, water level) */
    lc->loaded = esm_load_cell(cm->reader, cm->index, gx, gy,
                                &lc->cell, lc->refs, &lc->ref_count,
                                CELL_MAX_REFS);

    /* Load terrain heightmap */
    if (lc->loaded) {
        lc->has_terrain = esm_load_land(cm->reader, cm->index, gx, gy,
                                          &lc->land);
    }

    if (lc->loaded) cm->cells_loaded++;
}

static void unload_cell(cell_manager_t *cm, loaded_cell_t *lc) {
    if (lc->loaded) {
        cm->cells_unloaded++;
    }
    memset(lc, 0, sizeof(*lc));
}

/* -------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------- */

void cell_mgr_init(cell_manager_t *cm, esm_reader_t *reader, esm_index_t *index) {
    memset(cm, 0, sizeof(*cm));
    cm->reader = reader;
    cm->index = index;
    cm->center_x = 0x7FFFFFFF;  /* Force initial load */
    cm->center_y = 0x7FFFFFFF;
}

bool cell_mgr_update(cell_manager_t *cm, const mw_player_pos_t *player) {
    if (player->in_interior) return false;  /* Interior handled separately */

    int32_t new_cx, new_cy;
    world_to_cell(player->x, player->y, &new_cx, &new_cy);

    /* No change — nothing to do */
    if (new_cx == cm->center_x && new_cy == cm->center_y)
        return false;

    /*
     * Player moved to a new cell. We need to:
     *   1. Determine which cells in the 3×3 grid are still valid
     *   2. Unload cells that fell out of range
     *   3. Load new cells that came into range
     *
     * For simplicity (and because N64 has limited time per frame),
     * if the center changed by more than 1 in either axis, reload all.
     */
    int32_t dx = new_cx - cm->center_x;
    int32_t dy = new_cy - cm->center_y;

    if (dx > 1 || dx < -1 || dy > 1 || dy < -1 ||
        cm->center_x == 0x7FFFFFFF) {
        /* Large jump or first load — reload everything */
        for (int i = 0; i < CELL_GRID_TOTAL; i++) {
            unload_cell(cm, &cm->cells[i]);
        }

        cm->center_x = new_cx;
        cm->center_y = new_cy;

        for (int gy = -CELL_GRID_RADIUS; gy <= CELL_GRID_RADIUS; gy++) {
            for (int gx = -CELL_GRID_RADIUS; gx <= CELL_GRID_RADIUS; gx++) {
                int slot = grid_to_slot(new_cx + gx, new_cy + gy, new_cx, new_cy);
                if (slot >= 0) {
                    load_exterior_cell(cm, &cm->cells[slot],
                                        new_cx + gx, new_cy + gy);
                }
            }
        }
    } else {
        /*
         * Moved by exactly 1 cell — shift the grid.
         * Unload the row/column that fell out, load the new one.
         */

        /* Mark cells that are no longer in range */
        loaded_cell_t old_cells[CELL_GRID_TOTAL];
        memcpy(old_cells, cm->cells, sizeof(old_cells));
        memset(cm->cells, 0, sizeof(cm->cells));

        /* Copy still-valid cells to their new slots */
        for (int i = 0; i < CELL_GRID_TOTAL; i++) {
            if (!old_cells[i].loaded) continue;
            int new_slot = grid_to_slot(old_cells[i].grid_x, old_cells[i].grid_y,
                                         new_cx, new_cy);
            if (new_slot >= 0) {
                cm->cells[new_slot] = old_cells[i];
            } else {
                /* Out of range — unload */
                unload_cell(cm, &old_cells[i]);
            }
        }

        cm->center_x = new_cx;
        cm->center_y = new_cy;

        /* Load any empty slots */
        for (int gy = -CELL_GRID_RADIUS; gy <= CELL_GRID_RADIUS; gy++) {
            for (int gx = -CELL_GRID_RADIUS; gx <= CELL_GRID_RADIUS; gx++) {
                int slot = grid_to_slot(new_cx + gx, new_cy + gy, new_cx, new_cy);
                if (slot >= 0 && !cm->cells[slot].loaded) {
                    load_exterior_cell(cm, &cm->cells[slot],
                                        new_cx + gx, new_cy + gy);
                }
            }
        }
    }

    cm->in_interior = false;
    return true;
}

bool cell_mgr_enter_interior(cell_manager_t *cm, const char *cell_name) {
    /* Unload all exterior cells */
    for (int i = 0; i < CELL_GRID_TOTAL; i++) {
        unload_cell(cm, &cm->cells[i]);
    }

    memset(&cm->interior_cell, 0, sizeof(cm->interior_cell));

    /* Load interior */
    cm->interior_cell.is_interior = true;
    cm->interior_cell.loaded = esm_load_interior(
        cm->reader, cm->index, cell_name,
        &cm->interior_cell.cell,
        cm->interior_cell.refs, &cm->interior_cell.ref_count,
        CELL_MAX_REFS);

    if (cm->interior_cell.loaded) {
        cm->in_interior = true;
        strncpy(cm->interior_name, cell_name, 63);
        cm->interior_name[63] = '\0';
        cm->cells_loaded++;
    }

    return cm->interior_cell.loaded;
}

bool cell_mgr_exit_to_exterior(cell_manager_t *cm, float x, float y, float z) {
    (void)z;
    memset(&cm->interior_cell, 0, sizeof(cm->interior_cell));
    cm->in_interior = false;

    /* Force reload at new position */
    cm->center_x = 0x7FFFFFFF;
    cm->center_y = 0x7FFFFFFF;

    mw_player_pos_t pos = { .x = x, .y = y, .z = z, .in_interior = false };
    return cell_mgr_update(cm, &pos);
}

loaded_cell_t *cell_mgr_get_cell(cell_manager_t *cm, int32_t grid_x, int32_t grid_y) {
    int slot = grid_to_slot(grid_x, grid_y, cm->center_x, cm->center_y);
    if (slot < 0 || !cm->cells[slot].loaded) return NULL;
    return &cm->cells[slot];
}

loaded_cell_t *cell_mgr_get_interior(cell_manager_t *cm) {
    if (!cm->in_interior || !cm->interior_cell.loaded) return NULL;
    return &cm->interior_cell;
}

/*
 * Get terrain height at world position by bilinear interpolation.
 * The heightmap is 65×65 covering MW_CELL_SIZE × MW_CELL_SIZE.
 */
float cell_mgr_get_terrain_height(cell_manager_t *cm, float x, float y) {
    int32_t cx, cy;
    world_to_cell(x, y, &cx, &cy);

    loaded_cell_t *lc = cell_mgr_get_cell(cm, cx, cy);
    if (!lc || !lc->has_terrain) return 0.0f;

    /* Local position within cell (0..MW_CELL_SIZE) */
    float local_x = x - (float)(cx * MW_CELL_SIZE);
    float local_y = y - (float)(cy * MW_CELL_SIZE);

    /* Convert to heightmap grid coordinates (0..64) */
    float gx = (local_x / MW_CELL_SIZE) * 64.0f;
    float gy = (local_y / MW_CELL_SIZE) * 64.0f;

    /* Clamp */
    if (gx < 0) gx = 0;
    if (gy < 0) gy = 0;
    if (gx > 63.999f) gx = 63.999f;
    if (gy > 63.999f) gy = 63.999f;

    /* Integer grid coords + fractional parts */
    int ix = (int)gx;
    int iy = (int)gy;
    float fx = gx - ix;
    float fy = gy - iy;

    /* Bilinear interpolation of 4 neighboring height values.
     * Heights stored as int16, multiplied by 8 for world units. */
    float h00 = lc->land.heights[iy * 65 + ix] * 8.0f;
    float h10 = lc->land.heights[iy * 65 + ix + 1] * 8.0f;
    float h01 = lc->land.heights[(iy + 1) * 65 + ix] * 8.0f;
    float h11 = lc->land.heights[(iy + 1) * 65 + ix + 1] * 8.0f;

    float h0 = h00 + (h10 - h00) * fx;
    float h1 = h01 + (h11 - h01) * fx;
    return h0 + (h1 - h0) * fy;
}

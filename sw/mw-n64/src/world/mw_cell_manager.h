/*
 * MW-N64: Cell streaming manager
 *
 * Morrowind's world is divided into cells:
 *   - Exterior: 64×64 world unit grid, keyed by (grid_x, grid_y)
 *   - Interior: named rooms, loaded by string ID
 *
 * We keep a 3×3 ring of exterior cells loaded around the player.
 * As the player moves, old cells are unloaded and new ones streamed
 * from the ESM data on SD card via our asset streaming system.
 *
 * Interior cells are loaded individually (one at a time).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_CELL_MANAGER_H
#define MW_CELL_MANAGER_H

#include "../esm/mw_esm.h"
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------- */

/* Exterior cell grid: 3×3 around player = 9 cells loaded */
#define CELL_GRID_RADIUS    1
#define CELL_GRID_SIZE      (2 * CELL_GRID_RADIUS + 1)  /* 3 */
#define CELL_GRID_TOTAL     (CELL_GRID_SIZE * CELL_GRID_SIZE)  /* 9 */

/* Max object references per loaded cell */
#define CELL_MAX_REFS       512

/* Morrowind world unit constants */
#define MW_CELL_SIZE        8192    /* World units per cell */
#define MW_CELL_VERTS       65      /* Heightmap vertices per cell edge */

/* -------------------------------------------------------------------
 * Loaded cell state
 * ------------------------------------------------------------------- */

typedef struct {
    bool            loaded;
    bool            is_interior;
    int32_t         grid_x, grid_y;

    /* Cell definition */
    esm_cell_t      cell;

    /* Terrain (exterior only) */
    esm_land_t      land;
    bool            has_terrain;

    /* Object references placed in this cell */
    esm_cell_ref_t  refs[CELL_MAX_REFS];
    int             ref_count;
} loaded_cell_t;

/* -------------------------------------------------------------------
 * Cell manager state
 * ------------------------------------------------------------------- */

typedef struct {
    /* ESM data access */
    esm_reader_t    *reader;
    esm_index_t     *index;

    /* Currently loaded cells */
    loaded_cell_t   cells[CELL_GRID_TOTAL];

    /* Current center cell (where the player is) */
    int32_t         center_x, center_y;
    bool            in_interior;
    char            interior_name[64];

    /* The active interior cell (when indoors) */
    loaded_cell_t   interior_cell;

    /* Stats */
    uint32_t        cells_loaded;
    uint32_t        cells_unloaded;
} cell_manager_t;

/* -------------------------------------------------------------------
 * Player position (used to determine which cell to be in)
 * ------------------------------------------------------------------- */

typedef struct {
    float x, y, z;          /* World position */
    int32_t cell_x, cell_y; /* Current cell grid coords */
    bool in_interior;
    char interior_name[64];
} mw_player_pos_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/* Initialize the cell manager */
void cell_mgr_init(cell_manager_t *cm, esm_reader_t *reader, esm_index_t *index);

/*
 * Update loaded cells based on player position.
 * Loads/unloads cells as needed. Called every frame.
 * Returns true if any cells were loaded or unloaded.
 */
bool cell_mgr_update(cell_manager_t *cm, const mw_player_pos_t *player);

/*
 * Enter an interior cell. Unloads all exterior cells,
 * loads the named interior.
 */
bool cell_mgr_enter_interior(cell_manager_t *cm, const char *cell_name);

/*
 * Exit interior back to exterior at given position.
 */
bool cell_mgr_exit_to_exterior(cell_manager_t *cm, float x, float y, float z);

/*
 * Get the loaded cell at grid coordinates.
 * Returns NULL if not loaded.
 */
loaded_cell_t *cell_mgr_get_cell(cell_manager_t *cm, int32_t grid_x, int32_t grid_y);

/*
 * Get the current interior cell (if indoors).
 */
loaded_cell_t *cell_mgr_get_interior(cell_manager_t *cm);

/*
 * Get terrain height at a world position.
 * Interpolates from the heightmap of the cell containing (x, y).
 */
float cell_mgr_get_terrain_height(cell_manager_t *cm, float x, float y);

#endif /* MW_CELL_MANAGER_H */

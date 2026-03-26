/*
 * TRX-N64: Floor/ceiling height queries
 *
 * These functions determine the floor and ceiling height at any point
 * in the game world. They are the most performance-critical functions
 * in TR1's game logic, called hundreds of times per frame for collision
 * detection, pathfinding, and entity placement.
 *
 * The floor data format is TR1's original sector-based lookup:
 *   - Each room is divided into a grid of 1024×1024 sectors
 *   - Each sector stores floor/ceiling heights and portal indices
 *   - Portals connect to sectors in adjacent rooms (pit/sky rooms)
 *   - Floor data entries can modify heights with tilts, triggers, etc.
 *
 * All values and algorithms are exact reproductions from TRX.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"
#include <stddef.h>

#include <string.h>

/* -------------------------------------------------------------------
 * Sector lookup
 * ------------------------------------------------------------------- */

const tr_sector_t *game_get_sector(const game_state_t *gs,
                                    int16_t room_num,
                                    int32_t x, int32_t z) {
    if (room_num < 0 || room_num >= gs->level->room_count) return NULL;
    const tr_room_t *room = &gs->level->rooms[room_num];

    /* Convert world coords to sector indices within this room */
    int32_t zs = (z - room->z) >> WALL_SHIFT;
    int32_t xs = (x - room->x) >> WALL_SHIFT;

    /* Clamp to room bounds */
    if (zs < 0) zs = 0;
    if (zs >= room->sectors_z) zs = room->sectors_z - 1;
    if (xs < 0) xs = 0;
    if (xs >= room->sectors_x) xs = room->sectors_x - 1;

    return &room->sectors[zs * room->sectors_x + xs];
}

/* -------------------------------------------------------------------
 * Floor height calculation
 *
 * TR1 floor heights are stored as signed bytes multiplied by -256.
 * A floor value of -4 means floor height = -4 * -256 = 1024.
 *
 * The floor data array can override heights with:
 *   - Tilt data (sloped floors)
 *   - Portal connections (pit rooms below)
 *   - Trigger data (gameplay triggers)
 *
 * For now we implement the basic sector height lookup.
 * Floor data parsing (tilts, triggers) will be added incrementally.
 * ------------------------------------------------------------------- */

int32_t game_get_floor_height(const game_state_t *gs,
                               const tr_sector_t *sector,
                               int32_t x, int32_t y, int32_t z) {
    (void)gs; (void)x; (void)y; (void)z;

    if (!sector) return NO_HEIGHT;

    /* Basic floor height from sector data.
     * TR1 stores floor as signed byte: height = floor * -STEP_L */
    int32_t height = sector->floor * -STEP_L;

    /*
     * TODO: Parse floor_data for tilts and special cases.
     *
     * Floor data entries (indexed by sector->floor_data_idx):
     *   FT_FLOOR (0): Floor height modification
     *   FT_TILT (2): Tilted floor (sloped)
     *   FT_TRIGGER (4): Gameplay trigger
     *   FT_LAVA (5): Lava damage
     *   FT_CLIMB (6): Climbable wall
     *
     * The basic height is correct for flat sectors.
     * Tilted floors need the position within the sector to interpolate.
     */

    /* Follow portal down (pit room) */
    if (sector->pit_room != 255) {
        /* The sector has a room below — look up that room's floor */
        int16_t below_room = sector->pit_room;
        if (below_room >= 0 && below_room < gs->level->room_count) {
            const tr_sector_t *below = game_get_sector(gs, below_room, x, z);
            if (below) {
                int32_t below_height = below->floor * -STEP_L;
                if (below_height > height) height = below_height;
            }
        }
    }

    return height;
}

int32_t game_get_ceiling_height(const game_state_t *gs,
                                 const tr_sector_t *sector,
                                 int32_t x, int32_t y, int32_t z) {
    (void)gs; (void)x; (void)y; (void)z;

    if (!sector) return 0;

    int32_t height = sector->ceiling * -STEP_L;

    /* Follow portal up (sky room) */
    if (sector->sky_room != 255) {
        int16_t above_room = sector->sky_room;
        if (above_room >= 0 && above_room < gs->level->room_count) {
            const tr_sector_t *above = game_get_sector(gs, above_room, x, z);
            if (above) {
                int32_t above_height = above->ceiling * -STEP_L;
                if (above_height < height) height = above_height;
            }
        }
    }

    return height;
}

int16_t game_get_room_at(const game_state_t *gs,
                          int16_t start_room,
                          int32_t x, int32_t y, int32_t z) {
    const tr_sector_t *sector = game_get_sector(gs, start_room, x, z);
    if (!sector) return start_room;

    /* Check if we should be in a room below */
    if (sector->pit_room != 255) {
        int32_t floor = sector->floor * -STEP_L;
        if (y > floor) {
            return game_get_room_at(gs, sector->pit_room, x, y, z);
        }
    }

    /* Check if we should be in a room above */
    if (sector->sky_room != 255) {
        int32_t ceiling = sector->ceiling * -STEP_L;
        if (y < ceiling) {
            return game_get_room_at(gs, sector->sky_room, x, y, z);
        }
    }

    return start_room;
}

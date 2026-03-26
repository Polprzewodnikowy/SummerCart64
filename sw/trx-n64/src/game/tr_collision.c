/*
 * TRX-N64: Collision detection
 *
 * Determines what surfaces surround an entity and whether movement
 * is blocked. Uses TR1's original collision probe pattern:
 *   - Test mid (at entity center)
 *   - Test front (in movement direction)
 *   - Test left/right (strafe directions)
 *
 * All probe distances, step heights, and collision margins are
 * exact TR1 values.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"

extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

/* -------------------------------------------------------------------
 * Collision probe helpers
 * ------------------------------------------------------------------- */

static void probe_floor_ceiling(const game_state_t *gs,
                                 int16_t room_num,
                                 int32_t x, int32_t y, int32_t z,
                                 coll_side_t *side) {
    const tr_sector_t *sector = game_get_sector(gs, room_num, x, z);
    side->floor = game_get_floor_height(gs, sector, x, y, z);
    side->ceiling = game_get_ceiling_height(gs, sector, x, y, z);
    side->type = 0;  /* TODO: floor data type */
}

/* -------------------------------------------------------------------
 * Main collision info query
 *
 * Probes floor/ceiling at the entity's position and in the
 * movement direction. This is what TRX calls Collide_GetInfo().
 *
 * height: entity collision height (LARA_HEIGHT for Lara)
 * ------------------------------------------------------------------- */

void game_get_collision_info(game_state_t *gs,
                              coll_info_t *coll,
                              const game_item_t *item,
                              int32_t height) {
    int32_t x = item->pos.x;
    int32_t y = item->pos.y;
    int32_t z = item->pos.z;
    int16_t room = item->room_num;

    /* Save old position for shift calculation */
    coll->old.x = x;
    coll->old.y = y;
    coll->old.z = z;

    /* Y position for probing (top of entity) */
    int32_t ypos = y - height;

    /* Mid probe (at entity center) */
    probe_floor_ceiling(gs, room, x, ypos, z, &coll->side_mid);

    /* Front probe (in facing direction) */
    int32_t front_x = x;
    int32_t front_z = z;
    int32_t s = tr_sin(coll->facing) >> 2;
    int32_t c = tr_cos(coll->facing) >> 2;
    front_x += (coll->radius * s) >> 14;
    front_z += (coll->radius * c) >> 14;
    probe_floor_ceiling(gs, room, front_x, ypos, front_z, &coll->side_front);

    /* Left probe */
    int32_t left_angle = coll->facing - DEG_90;
    int32_t left_x = x + ((coll->radius * (tr_sin(left_angle) >> 2)) >> 14);
    int32_t left_z = z + ((coll->radius * (tr_cos(left_angle) >> 2)) >> 14);
    probe_floor_ceiling(gs, room, left_x, ypos, left_z, &coll->side_left);

    /* Right probe */
    int32_t right_angle = coll->facing + DEG_90;
    int32_t right_x = x + ((coll->radius * (tr_sin(right_angle) >> 2)) >> 14);
    int32_t right_z = z + ((coll->radius * (tr_cos(right_angle) >> 2)) >> 14);
    probe_floor_ceiling(gs, room, right_x, ypos, right_z, &coll->side_right);

    /* Tilt calculation (for slope detection) */
    /* TODO: compute tilt from floor data */
    coll->tilt_x = 0;
    coll->tilt_z = 0;

    /* Collision type determination */
    coll->coll_type = 0;
    coll->shift.x = 0;
    coll->shift.y = 0;
    coll->shift.z = 0;

    /* Check for wall collision (front probe blocked) */
    if (coll->side_front.floor - y > coll->bad_pos ||
        coll->side_front.floor - y < coll->bad_neg ||
        coll->side_front.ceiling - ypos > coll->bad_ceiling) {
        /* Blocked in front */
        coll->shift.x = coll->old.x - x;
        coll->shift.z = coll->old.z - z;
        coll->coll_type = 1;  /* COLL_FRONT */
    }

    /* Check for side wall collisions */
    if (coll->side_left.floor - y > coll->bad_pos ||
        coll->side_left.floor - y < coll->bad_neg) {
        coll->shift.x += (x - left_x);
        coll->shift.z += (z - left_z);
        coll->coll_type = 2;  /* COLL_LEFT */
    }

    if (coll->side_right.floor - y > coll->bad_pos ||
        coll->side_right.floor - y < coll->bad_neg) {
        coll->shift.x += (x - right_x);
        coll->shift.z += (z - right_z);
        coll->coll_type = 3;  /* COLL_RIGHT */
    }
}

void game_shift_item(game_item_t *item, const coll_info_t *coll) {
    item->pos.x += coll->shift.x;
    item->pos.y += coll->shift.y;
    item->pos.z += coll->shift.z;
}

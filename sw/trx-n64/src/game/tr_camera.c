/*
 * TRX-N64: Camera system
 *
 * TR1's camera follows Lara with a fixed chase distance, smoothly
 * interpolating position. The camera logic handles:
 *   - Chase mode: behind and above Lara
 *   - Combat mode: locked to enemy target
 *   - Look mode: player-controlled look around
 *   - Cinematic mode: fixed camera positions
 *
 * We implement chase mode first (the primary gameplay camera).
 * All distances and interpolation speeds match TR1 exactly.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"
#include <stddef.h>

extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

/* Camera constants (exact TR1 values) */
#define CHASE_DISTANCE      1536    /* Distance behind Lara */
#define CHASE_HEIGHT        (-512)  /* Height above Lara's origin */
#define CAMERA_SPEED        8       /* Position interpolation (1/8 per frame) */
#define LOOK_SPEED          (DEG_1 * 2)

void game_camera_update(game_state_t *gs) {
    if (!gs->lara_item) return;

    game_item_t *item = gs->lara_item;
    tr_camera_t *cam = &gs->camera;

    /*
     * Chase camera: position behind Lara based on her facing direction.
     *
     * Target position:
     *   X = lara.x - sin(lara.rot_y) * CHASE_DISTANCE
     *   Y = lara.y + CHASE_HEIGHT
     *   Z = lara.z - cos(lara.rot_y) * CHASE_DISTANCE
     *
     * Camera looks at Lara's upper body.
     */
    int32_t s = tr_sin(item->rot.y) >> 2;
    int32_t c = tr_cos(item->rot.y) >> 2;

    int32_t target_x = item->pos.x - ((CHASE_DISTANCE * s) >> 14);
    int32_t target_y = item->pos.y + CHASE_HEIGHT;
    int32_t target_z = item->pos.z - ((CHASE_DISTANCE * c) >> 14);

    /*
     * Smooth interpolation (exact TR1 method):
     *   new_pos = old_pos + (target_pos - old_pos) / CAMERA_SPEED
     *
     * This creates the characteristic smooth TR1 camera lag.
     */
    cam->x += (target_x - cam->x) / CAMERA_SPEED;
    cam->y += (target_y - cam->y) / CAMERA_SPEED;
    cam->z += (target_z - cam->z) / CAMERA_SPEED;

    /* Camera looks at Lara */
    cam->target_x = item->pos.x;
    cam->target_y = item->pos.y + CHASE_HEIGHT;
    cam->target_z = item->pos.z;

    /* Camera rotation follows Lara's facing */
    cam->rot_y = item->rot.y + DEG_180;  /* Face toward Lara */
    cam->rot_x = 0;  /* Level with Lara for chase mode */

    /*
     * Look mode: when player holds R + C-buttons
     * This lets the player look around without moving Lara.
     * TODO: implement look mode (requires separate camera angle tracking)
     */

    /*
     * Collision: ensure camera isn't inside a wall.
     * TODO: trace ray from Lara to camera position, clip if blocked.
     */

    /* Update which room the camera is in */
    gs->camera_room = game_get_room_at(gs, item->room_num,
                                         cam->x, cam->y, cam->z);
}

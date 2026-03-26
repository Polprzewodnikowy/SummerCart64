/*
 * TRX-N64: Game state initialization and main tick
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"
#include "../platform/n64_platform.h"

#include <string.h>

/* Global game state */
game_state_t g_game;

/* -------------------------------------------------------------------
 * Initialize game state from level data
 * ------------------------------------------------------------------- */

void game_init(game_state_t *gs, const tr_level_t *level) {
    memset(gs, 0, sizeof(*gs));
    gs->level = level;
    gs->item_count = level->item_count;
    if (gs->item_count > MAX_ITEMS_COUNT) gs->item_count = MAX_ITEMS_COUNT;

    /* Initialize room item linked lists */
    for (int i = 0; i < TR_MAX_ROOMS; i++) {
        gs->room_item_head[i] = NO_ITEM;
    }

    /* Initialize items from level placement data */
    for (int i = 0; i < gs->item_count; i++) {
        const tr_item_t *src = &level->items[i];
        game_item_t *dst = &gs->items[i];

        dst->object_id = src->object_id;
        dst->room_num = src->room;
        dst->pos = src->pos;
        dst->rot.x = 0;
        dst->rot.y = src->rotation;
        dst->rot.z = 0;
        dst->speed = 0;
        dst->fall_speed = 0;
        dst->flags = src->flags;
        dst->status = IS_INACTIVE;
        dst->active = false;
        dst->gravity = false;
        dst->collidable = true;
        dst->touch_bits = 0;
        dst->mesh_bits = 0xFFFFFFFF;  /* All meshes visible */
        dst->next_item = NO_ITEM;
        dst->next_active = NO_ITEM;

        /* Find object definition and set up animation */
        for (int o = 0; o < level->object_count; o++) {
            if (level->objects[o].object_id == src->object_id) {
                const tr_object_def_t *obj = &level->objects[o];
                dst->anim.anim_num = obj->anim_idx;
                if (obj->anim_idx >= 0 && obj->anim_idx < level->anim_count) {
                    dst->anim.frame_num = level->anims[obj->anim_idx].frame_base;
                    dst->anim.current_state = level->anims[obj->anim_idx].state_id;
                }
                dst->anim.goal_state = dst->anim.current_state;

                /* Set hit points based on object type */
                if (src->object_id == 0) {  /* Lara */
                    dst->hit_points = LARA_HITPOINTS;
                }
                break;
            }
        }

        /* Add to room's item linked list */
        if (dst->room_num >= 0 && dst->room_num < TR_MAX_ROOMS) {
            dst->next_item = gs->room_item_head[dst->room_num];
            gs->room_item_head[dst->room_num] = i;
        }
    }

    /* Find Lara (object_id 0) */
    for (int i = 0; i < gs->item_count; i++) {
        if (gs->items[i].object_id == 0) {
            gs->lara.item_num = i;
            gs->lara_item = &gs->items[i];
            gs->lara.air = LARA_AIR;
            gs->lara.gun_type = LGT_UNARMED;

            /* Initialize camera behind Lara */
            int32_t s = tr_sin(gs->lara_item->rot.y) >> 2;
            int32_t c = tr_cos(gs->lara_item->rot.y) >> 2;
            gs->camera.x = gs->lara_item->pos.x - ((1536 * s) >> 14);
            gs->camera.y = gs->lara_item->pos.y - 512;
            gs->camera.z = gs->lara_item->pos.z - ((1536 * c) >> 14);
            gs->camera.rot_y = gs->lara_item->rot.y + DEG_180;
            gs->camera.fov = 14563;  /* TR1 default ~80° */
            gs->camera_room = gs->lara_item->room_num;

            /* Activate Lara */
            gs->lara_item->active = true;
            gs->lara_item->status = IS_ACTIVE;
            break;
        }
    }

    gs->frame_count = 0;
    gs->flip_status = false;
}

/* -------------------------------------------------------------------
 * Input mapping: N64 controller → game input bits
 * ------------------------------------------------------------------- */

void game_map_input(game_state_t *gs, const void *n64_input_ptr) {
    const n64_input_state_t *pad = (const n64_input_state_t *)n64_input_ptr;

    gs->input_prev = gs->input;
    gs->input = 0;

    if (n64_input_held(pad, TR_INPUT_UP))       gs->input |= IN_FORWARD;
    if (n64_input_held(pad, TR_INPUT_DOWN))     gs->input |= IN_BACK;
    if (n64_input_held(pad, TR_INPUT_LEFT))     gs->input |= IN_LEFT;
    if (n64_input_held(pad, TR_INPUT_RIGHT))    gs->input |= IN_RIGHT;
    if (n64_input_held(pad, TR_INPUT_JUMP))     gs->input |= IN_JUMP;
    if (n64_input_held(pad, TR_INPUT_ACTION))   gs->input |= IN_ACTION;
    if (n64_input_held(pad, TR_INPUT_DRAW))     gs->input |= IN_DRAW;
    if (n64_input_held(pad, TR_INPUT_LOOK))     gs->input |= IN_LOOK;
    if (n64_input_held(pad, TR_INPUT_WALK))     gs->input |= IN_WALK;
    if (n64_input_held(pad, TR_INPUT_STEP_LEFT))  gs->input |= IN_STEP_L;
    if (n64_input_held(pad, TR_INPUT_STEP_RIGHT)) gs->input |= IN_STEP_R;
    if (n64_input_held(pad, TR_INPUT_PAUSE))    gs->input |= IN_OPTION;

    /* Roll = C-Up + C-Down simultaneously */
    if ((pad->buttons & TR_INPUT_LOOK_UP) && (pad->buttons & TR_INPUT_LOOK_DOWN))
        gs->input |= IN_ROLL;
}

/* Forward declarations */
extern void game_control_items(game_state_t *gs);
extern void game_execute_trigger(game_state_t *gs, const tr_sector_t *sector);

/* -------------------------------------------------------------------
 * Main game tick — one frame at 30fps
 *
 * Update order matches TR1 exactly:
 *   1. Map input
 *   2. Control Lara (movement, state machine, physics)
 *   3. Test trigger at Lara's feet
 *   4. Control other items (enemies, traps, etc.)
 *   5. Update camera
 * ------------------------------------------------------------------- */

void game_frame_tick(game_state_t *gs) {
    gs->frame_count++;

    /* Control Lara */
    game_lara_control(gs);

    /* Test trigger at Lara's current sector */
    if (gs->lara_item) {
        const tr_sector_t *sector = game_get_sector(gs, gs->lara_item->room_num,
                                                      gs->lara_item->pos.x,
                                                      gs->lara_item->pos.z);
        if (sector) {
            game_execute_trigger(gs, sector);
        }
    }

    /* Control all other active items (enemies, traps, doors, etc.) */
    game_control_items(gs);

    /* Update camera to follow Lara */
    game_camera_update(gs);
}

/* tr_sin/tr_cos are defined in n64_transform.c */
extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

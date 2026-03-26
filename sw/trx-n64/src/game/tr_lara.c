/*
 * TRX-N64: Lara movement control
 *
 * Implements Lara's core movement physics:
 *   - Gravity and falling
 *   - Ground movement (walk, run, turn)
 *   - Basic state machine (standing, running, jumping, falling)
 *
 * The full TRX Lara state machine has ~50 states. We implement
 * the core states first and add the rest incrementally.
 * All physics constants match TR1 exactly.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"

#include <stddef.h>

extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

/* Lara animation state IDs (exact TR1 values) */
#define AS_WALK         0
#define AS_RUN          1
#define AS_STOP         2
#define AS_FORWARDJUMP  3
#define AS_POSE         4   /* Idle / standing still */
#define AS_FASTBACK     5
#define AS_TURN_R       6
#define AS_TURN_L       7
#define AS_DEATH        8
#define AS_FASTFALL     9
#define AS_HANG         10
#define AS_REACH        11
#define AS_SPLAT        12
#define AS_COMPRESS     14
#define AS_BACK         15
#define AS_SWIM         17
#define AS_GLIDE        18
#define AS_TREAD        33
#define AS_FALLBACK     48
#define AS_SLIDE        24

/* Turn speeds (exact TR1 values, angle units per frame) */
#define LARA_TURN_RATE      (DEG_1 * 2)     /* ~364 per frame */
#define LARA_SLOW_TURN      (DEG_1 * 2)
#define LARA_FAST_TURN      (DEG_1 * 4)     /* ~728 per frame */
#define LARA_WALK_SPEED     16
#define LARA_RUN_SPEED      47

/* -------------------------------------------------------------------
 * Lara physics update — called once per 30fps frame
 *
 * This processes movement based on current animation state,
 * applies gravity, and handles basic floor alignment.
 * ------------------------------------------------------------------- */

void game_lara_control(game_state_t *gs) {
    game_item_t *item = gs->lara_item;
    if (!item) return;

    lara_info_t *lara = &gs->lara;
    uint32_t input = gs->input;

    /* Get floor/ceiling at Lara's position */
    const tr_sector_t *sector = game_get_sector(gs, item->room_num,
                                                  item->pos.x, item->pos.z);
    int32_t floor_height = game_get_floor_height(gs, sector,
                                                   item->pos.x, item->pos.y,
                                                   item->pos.z);
    int32_t ceiling_height = game_get_ceiling_height(gs, sector,
                                                       item->pos.x, item->pos.y,
                                                       item->pos.z);

    item->floor = floor_height;

    /*
     * Goal state management based on input.
     *
     * The animation system handles state transitions via the
     * change/range tables. We just set the goal_state based on
     * what the player wants to do.
     */
    int16_t current_state = item->anim.current_state;

    switch (current_state) {
        case AS_STOP:
        case AS_POSE:
            /* Standing still */
            if (input & IN_FORWARD) {
                if (input & IN_WALK) {
                    item->anim.goal_state = AS_WALK;
                } else {
                    item->anim.goal_state = AS_RUN;
                }
            } else if (input & IN_BACK) {
                item->anim.goal_state = AS_BACK;
            } else if (input & IN_LEFT) {
                item->anim.goal_state = AS_TURN_L;
            } else if (input & IN_RIGHT) {
                item->anim.goal_state = AS_TURN_R;
            } else if (input & IN_JUMP) {
                item->anim.goal_state = AS_COMPRESS;
            } else {
                item->anim.goal_state = AS_STOP;
            }
            break;

        case AS_RUN:
            /* Running */
            if (input & IN_JUMP) {
                item->anim.goal_state = AS_FORWARDJUMP;
            } else if (input & IN_LEFT) {
                item->rot.y -= LARA_FAST_TURN;
            } else if (input & IN_RIGHT) {
                item->rot.y += LARA_FAST_TURN;
            }

            if (!(input & IN_FORWARD)) {
                item->anim.goal_state = AS_STOP;
            } else if (input & IN_WALK) {
                item->anim.goal_state = AS_WALK;
            }
            break;

        case AS_WALK:
            /* Walking */
            if (input & IN_LEFT) {
                item->rot.y -= LARA_SLOW_TURN;
            } else if (input & IN_RIGHT) {
                item->rot.y += LARA_SLOW_TURN;
            }

            if (!(input & IN_FORWARD) || !(input & IN_WALK)) {
                item->anim.goal_state = AS_STOP;
            }
            break;

        case AS_TURN_L:
            item->rot.y -= LARA_TURN_RATE;
            if (!(input & IN_LEFT)) {
                item->anim.goal_state = AS_STOP;
            }
            break;

        case AS_TURN_R:
            item->rot.y += LARA_TURN_RATE;
            if (!(input & IN_RIGHT)) {
                item->anim.goal_state = AS_STOP;
            }
            break;

        case AS_BACK:
            if (input & IN_LEFT) {
                item->rot.y -= LARA_SLOW_TURN;
            } else if (input & IN_RIGHT) {
                item->rot.y += LARA_SLOW_TURN;
            }
            if (!(input & IN_BACK)) {
                item->anim.goal_state = AS_STOP;
            }
            break;

        case AS_FORWARDJUMP:
        case AS_FALLBACK:
            /* In air — apply gravity */
            break;

        case AS_FASTFALL:
            /* Terminal velocity fall */
            break;

        default:
            break;
    }

    /* Apply gravity if falling */
    if (item->gravity) {
        item->fall_speed += GRAVITY;
        if (item->fall_speed > FASTFALL_SPEED) {
            item->fall_speed = FASTFALL_SPEED;
        }
        item->pos.y += item->fall_speed;
    }

    /* Apply forward/back speed */
    if (item->speed != 0) {
        int32_t s = tr_sin(item->rot.y) >> 2;
        int32_t c = tr_cos(item->rot.y) >> 2;
        item->pos.x += (item->speed * s) >> 14;
        item->pos.z += (item->speed * c) >> 14;
    }

    /* Collision with floor */
    if (item->pos.y >= floor_height) {
        item->pos.y = floor_height;
        item->fall_speed = 0;
        item->gravity = false;

        /* Land if we were in the air */
        if (current_state == AS_FORWARDJUMP || current_state == AS_FALLBACK ||
            current_state == AS_FASTFALL) {
            item->anim.goal_state = AS_STOP;
        }
    } else if (item->pos.y < floor_height && !item->gravity &&
               current_state != AS_FORWARDJUMP && current_state != AS_FALLBACK) {
        /* Start falling if there's no floor beneath */
        if (floor_height - item->pos.y > STEP_L * 2) {
            item->gravity = true;
            item->fall_speed = 0;
        } else {
            /* Snap to floor (stepping down) */
            item->pos.y = floor_height;
        }
    }

    /* Ceiling collision */
    if (item->pos.y - LARA_HEIGHT < ceiling_height) {
        item->pos.y = ceiling_height + LARA_HEIGHT;
        if (item->fall_speed < 0) item->fall_speed = 0;
    }

    /* Update room number if Lara moved to a different room */
    int16_t new_room = game_get_room_at(gs, item->room_num,
                                          item->pos.x, item->pos.y, item->pos.z);
    if (new_room != item->room_num) {
        item->room_num = new_room;
    }

    /* Update lara movement angle for camera */
    lara->move_angle = item->rot.y;

    /* Advance animation */
    const tr_object_def_t *obj = NULL;
    for (int o = 0; o < gs->level->object_count; o++) {
        if (gs->level->objects[o].object_id == item->object_id) {
            obj = &gs->level->objects[o];
            break;
        }
    }
    if (obj) {
        tr_anim_advance(&item->anim, gs->level, obj);
    }
}

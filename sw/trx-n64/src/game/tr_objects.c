/*
 * TRX-N64: Object type definitions and control dispatch
 *
 * TR1 has ~200 object types. Each type has:
 *   - A control function (called per frame for active items)
 *   - A collision function (called when Lara is near)
 *   - An activation function (called by triggers)
 *   - Hit points, radius, smartness, etc.
 *
 * We implement the core gameplay objects first:
 *   - Lara (object 0) — handled by tr_lara.c
 *   - Doors (objects 38-42) — open/close on triggers
 *   - Pickups (objects 84-100+) — collect on action
 *   - Switches/keys (objects 55-70) — activate triggers
 *   - Traps (objects 25-37) — dart emitters, boulders, spikes
 *   - Enemies (objects 7-24) — AI-driven creatures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"

/* -------------------------------------------------------------------
 * TR1 object IDs (exact values from the original engine)
 * ------------------------------------------------------------------- */

/* Lara */
#define O_LARA          0
#define O_LARA_PISTOLS  1
#define O_LARA_SHOTGUN  2
#define O_LARA_MAGNUMS  3
#define O_LARA_UZIS     4
#define O_LARA_EXTRA    5

/* Enemies */
#define O_WOLF          7
#define O_BEAR          8
#define O_BAT           9
#define O_CROC_LAND     10
#define O_CROC_WATER    11
#define O_LION_MALE     12
#define O_LION_FEMALE   13
#define O_PUMA          14
#define O_GORILLA       15
#define O_RAT_LAND      16
#define O_RAT_WATER     17
#define O_T_REX         18
#define O_RAPTOR        19
#define O_WARRIOR_1     20
#define O_WARRIOR_2     21
#define O_WARRIOR_3     22
#define O_CENTAUR       23
#define O_MUMMY         24
#define O_LARSON        27
#define O_PIERRE        28
#define O_SKATEKID      29
#define O_COWBOY        30
#define O_BALDY         31
#define O_NATLA         32
#define O_TORSO         33

/* Traps */
#define O_DART_EMITTER  25
#define O_DART          26
#define O_FALLING_BLOCK 34
#define O_PENDULUM      35
#define O_SPIKES        36
#define O_BOULDER       37
#define O_ROLLING_BALL  37  /* Same as boulder in some versions */

/* Doors and switches */
#define O_DOOR_1        38
#define O_DOOR_2        39
#define O_DOOR_3        40
#define O_DOOR_4        41
#define O_DOOR_5        42
#define O_DOOR_6        43
#define O_DOOR_7        44
#define O_DOOR_8        45
#define O_TRAPDOOR_1    46
#define O_TRAPDOOR_2    47
#define O_BRIDGE_FLAT   48
#define O_BRIDGE_TILT_1 49
#define O_BRIDGE_TILT_2 50
#define O_SWITCH_TYPE_1 55
#define O_SWITCH_TYPE_2 56
#define O_KEY_1         57
#define O_KEY_2         58
#define O_KEY_3         59
#define O_KEY_4         60
#define O_LOCK_1        61
#define O_LOCK_2        62
#define O_LOCK_3        63
#define O_LOCK_4        64
#define O_PUZZLE_1      65
#define O_PUZZLE_2      66
#define O_PUZZLE_3      67
#define O_PUZZLE_4      68
#define O_PUZZLE_DONE_1 69
#define O_PUZZLE_DONE_2 70
#define O_PUZZLE_DONE_3 71
#define O_PUZZLE_DONE_4 72

/* Pickups */
#define O_PISTOL_ITEM       84
#define O_SHOTGUN_ITEM      85
#define O_MAGNUM_ITEM       86
#define O_UZI_ITEM          87
#define O_PISTOL_AMMO_ITEM  88
#define O_SHOTGUN_AMMO_ITEM 89
#define O_MAGNUM_AMMO_ITEM  90
#define O_UZI_AMMO_ITEM     91
#define O_MEDI_ITEM         93
#define O_BIGMEDI_ITEM      94
#define O_SCION_ITEM        99
#define O_LEADBAR_ITEM      100
#define O_KEY_ITEM_1        110
#define O_KEY_ITEM_2        111
#define O_KEY_ITEM_3        112
#define O_KEY_ITEM_4        113
#define O_PUZZLE_ITEM_1     114
#define O_PUZZLE_ITEM_2     115
#define O_PUZZLE_ITEM_3     116
#define O_PUZZLE_ITEM_4     117

/* -------------------------------------------------------------------
 * Generic object control functions
 * ------------------------------------------------------------------- */

/* Door: simply plays its animation based on active status */
static void control_door(game_state_t *gs, game_item_t *item) {
    if (item->active && item->status == IS_ACTIVE) {
        /* Door is opening — advance animation */
        /* Goal state is set by trigger activation */
    } else {
        /* Door is closing or idle */
    }
    (void)gs;
}

/* Falling block: waits for trigger, then falls */
static void control_falling_block(game_state_t *gs, game_item_t *item) {
    if (!item->active) return;

    if (item->anim.current_state == 0) {
        /* Waiting to fall */
        item->anim.goal_state = 1;  /* Start falling */
    } else if (item->anim.current_state == 1) {
        /* Falling — apply gravity */
        item->fall_speed += GRAVITY;
        item->pos.y += item->fall_speed;

        const tr_sector_t *sector = game_get_sector(gs, item->room_num,
                                                      item->pos.x, item->pos.z);
        int32_t floor = sector ? (sector->floor * -STEP_L) : item->pos.y;

        if (item->pos.y >= floor) {
            item->pos.y = floor;
            item->fall_speed = 0;
            item->anim.goal_state = 2;  /* Landed */
            item->active = false;
            item->status = IS_DEACTIVATED;
        }
    }
}

/* Boulder/rolling ball: rolls in its facing direction */
static void control_boulder(game_state_t *gs, game_item_t *item) {
    if (!item->active) return;

    /* Accelerate */
    item->speed += 2;
    if (item->speed > 128) item->speed = 128;

    /* Apply gravity */
    item->fall_speed += GRAVITY;
    item->pos.y += item->fall_speed;

    /* Move in facing direction */
    fix16_t s = tr_sin(item->rot.y) >> 2;
    fix16_t c = tr_cos(item->rot.y) >> 2;
    item->pos.x += (item->speed * s) >> 14;
    item->pos.z += (item->speed * c) >> 14;

    /* Floor collision */
    const tr_sector_t *sector = game_get_sector(gs, item->room_num,
                                                  item->pos.x, item->pos.z);
    if (sector) {
        int32_t floor = sector->floor * -STEP_L;
        if (item->pos.y >= floor) {
            item->pos.y = floor;
            item->fall_speed = 0;
        }
    }

    /* Update room */
    int16_t new_room = game_get_room_at(gs, item->room_num,
                                          item->pos.x, item->pos.y, item->pos.z);
    if (new_room != item->room_num) {
        item->room_num = new_room;
    }
}

/* Pickup: bob animation, collect on action */
static void control_pickup(game_state_t *gs, game_item_t *item) {
    /* Pickups are static — collection handled by Lara collision in M6+ */
    (void)gs;
    (void)item;
}

extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

/* -------------------------------------------------------------------
 * Object control dispatch
 *
 * Called once per frame for each active item.
 * Routes to the appropriate type-specific control function.
 * ------------------------------------------------------------------- */

void game_control_item(game_state_t *gs, int item_num) {
    if (item_num < 0 || item_num >= gs->item_count) return;
    game_item_t *item = &gs->items[item_num];
    if (!item->active) return;
    if (item->object_id == O_LARA) return;  /* Lara handled separately */

    /* Find object definition for animation advance */
    const tr_object_def_t *obj = NULL;
    for (int o = 0; o < gs->level->object_count; o++) {
        if (gs->level->objects[o].object_id == item->object_id) {
            obj = &gs->level->objects[o];
            break;
        }
    }

    /* Dispatch to type-specific control */
    switch (item->object_id) {
        case O_DOOR_1: case O_DOOR_2: case O_DOOR_3: case O_DOOR_4:
        case O_DOOR_5: case O_DOOR_6: case O_DOOR_7: case O_DOOR_8:
        case O_TRAPDOOR_1: case O_TRAPDOOR_2:
            control_door(gs, item);
            break;

        case O_FALLING_BLOCK:
            control_falling_block(gs, item);
            break;

        case O_BOULDER:
            control_boulder(gs, item);
            break;

        case O_PISTOL_ITEM: case O_SHOTGUN_ITEM: case O_MAGNUM_ITEM:
        case O_UZI_ITEM: case O_MEDI_ITEM: case O_BIGMEDI_ITEM:
        case O_SCION_ITEM: case O_LEADBAR_ITEM:
        case O_KEY_ITEM_1: case O_KEY_ITEM_2: case O_KEY_ITEM_3: case O_KEY_ITEM_4:
        case O_PUZZLE_ITEM_1: case O_PUZZLE_ITEM_2:
        case O_PUZZLE_ITEM_3: case O_PUZZLE_ITEM_4:
            control_pickup(gs, item);
            break;

        case O_WOLF: case O_BEAR: case O_BAT: case O_CROC_LAND:
        case O_CROC_WATER: case O_LION_MALE: case O_LION_FEMALE:
        case O_PUMA: case O_GORILLA: case O_RAT_LAND: case O_RAT_WATER:
        case O_T_REX: case O_RAPTOR: case O_WARRIOR_1: case O_WARRIOR_2:
        case O_WARRIOR_3: case O_CENTAUR: case O_MUMMY:
        case O_LARSON: case O_PIERRE: case O_SKATEKID:
        case O_COWBOY: case O_BALDY: case O_NATLA: case O_TORSO:
            /*
             * Enemy AI — requires pathfinding (box/overlap system),
             * mood system, and per-creature state machines.
             *
             * For now: enemies stand in place and play idle animations.
             * Full AI is the next sub-milestone.
             */
            break;

        default:
            /* Unknown object type — just advance animation */
            break;
    }

    /* Advance animation for this item */
    if (obj) {
        tr_anim_advance(&item->anim, gs->level, obj);
    }

    /* Timer countdown */
    if (item->timer > 0) {
        item->timer--;
        if (item->timer == 0) {
            item->active = false;
            item->status = IS_DEACTIVATED;
        }
    }
}

/* -------------------------------------------------------------------
 * Update all active items (called from game_frame_tick)
 * ------------------------------------------------------------------- */

void game_control_items(game_state_t *gs) {
    for (int i = 0; i < gs->item_count; i++) {
        game_control_item(gs, i);
    }
}

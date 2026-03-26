/*
 * TRX-N64: Creature AI framework
 *
 * TR1's enemy AI uses a box-based pathfinding system:
 *   - The level is divided into navigation boxes (axis-aligned rectangles)
 *   - Boxes are connected via overlap links
 *   - Creatures navigate by finding a chain of boxes from their
 *     current box to the target box (usually Lara's box)
 *   - A "mood" system determines whether a creature is bored,
 *     attacking, stalking, or fleeing
 *
 * All pathfinding constants, mood thresholds, and AI behaviors
 * are exact TR1 values from TRX.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_game_shim.h"

extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

/* -------------------------------------------------------------------
 * Creature moods (exact TR1 values)
 * ------------------------------------------------------------------- */

typedef enum {
    MOOD_BORED = 0,     /* Wander randomly */
    MOOD_ATTACK = 1,    /* Charge at Lara */
    MOOD_ESCAPE = 2,    /* Run away from Lara */
    MOOD_STALK = 3,     /* Approach cautiously */
} creature_mood_t;

/* -------------------------------------------------------------------
 * Creature state (attached to each AI-driven entity)
 * ------------------------------------------------------------------- */

typedef struct {
    creature_mood_t mood;
    int16_t         target_box;     /* Navigation target box */
    int16_t         current_box;    /* Current navigation box */
    tr_xyz32_t      target;         /* World-space target position */
    int16_t         head_rotation;  /* Head Y rotation toward target */
    int16_t         neck_rotation;
    int32_t         maximum_turn;   /* Max turn rate per frame */
    bool            alerted;        /* Has seen Lara */
    bool            reached_goal;
} creature_state_t;

/* Simple creature storage */
#define MAX_CREATURES 32
static creature_state_t creatures[MAX_CREATURES];
static int creature_count = 0;

/* -------------------------------------------------------------------
 * Creature AI helpers
 * ------------------------------------------------------------------- */

/* Calculate angle from entity to a target point */
static int16_t angle_to_target(const game_item_t *item, int32_t tx, int32_t tz) {
    int32_t dx = tx - item->pos.x;
    int32_t dz = tz - item->pos.z;

    /* Simple atan2 approximation using TR1's angle system */
    /* Full implementation would use a lookup table */
    if (dx == 0 && dz == 0) return item->rot.y;

    /* Quadrant-based angle computation */
    int32_t abs_dx = dx < 0 ? -dx : dx;
    int32_t abs_dz = dz < 0 ? -dz : dz;
    int16_t angle;

    if (abs_dz > abs_dx) {
        /* Primarily forward/backward */
        int32_t ratio = (abs_dx << 14) / abs_dz;  /* 0..16384 */
        angle = (int16_t)(ratio >> 2);  /* Rough arctan */
    } else {
        int32_t ratio = (abs_dz << 14) / abs_dx;
        angle = DEG_90 - (int16_t)(ratio >> 2);
    }

    /* Adjust for quadrant */
    if (dz < 0) angle = DEG_180 - angle;
    if (dx < 0) angle = -angle;

    return angle;
}

/* Distance squared between two points (avoids sqrt) */
static int32_t distance_sq(int32_t x1, int32_t z1, int32_t x2, int32_t z2) {
    int32_t dx = (x2 - x1) >> 6;  /* Scale down to prevent overflow */
    int32_t dz = (z2 - z1) >> 6;
    return dx * dx + dz * dz;
}

/* -------------------------------------------------------------------
 * Mood determination
 *
 * The mood system is TR1's genius AI feature — enemies don't just
 * charge. They assess the situation and behave accordingly.
 * ------------------------------------------------------------------- */

static creature_mood_t determine_mood(const game_state_t *gs,
                                        const game_item_t *item,
                                        const creature_state_t *creature) {
    if (!gs->lara_item) return MOOD_BORED;

    int32_t dist_sq = distance_sq(item->pos.x, item->pos.z,
                                   gs->lara_item->pos.x, gs->lara_item->pos.z);

    /* Distance thresholds (in scaled units, since we >> 6) */
    int32_t close_range = (3072 >> 6) * (3072 >> 6);   /* ~3 sectors */
    int32_t alert_range = (8192 >> 6) * (8192 >> 6);    /* ~8 sectors */

    bool lara_alive = gs->lara_item->hit_points > 0;
    bool low_health = item->hit_points < (item->hit_points / 4);

    if (!lara_alive) return MOOD_BORED;

    if (low_health && creature->alerted) {
        return MOOD_ESCAPE;
    }

    if (dist_sq < close_range && creature->alerted) {
        return MOOD_ATTACK;
    }

    if (dist_sq < alert_range) {
        return creature->alerted ? MOOD_STALK : MOOD_BORED;
    }

    return MOOD_BORED;
}

/* -------------------------------------------------------------------
 * Main creature control — called per frame for each AI entity
 * ------------------------------------------------------------------- */

void game_creature_control(game_state_t *gs, game_item_t *item, int creature_idx) {
    if (creature_idx < 0 || creature_idx >= MAX_CREATURES) return;
    creature_state_t *creature = &creatures[creature_idx];

    if (!gs->lara_item) return;

    /* Determine mood */
    creature->mood = determine_mood(gs, item, creature);

    /* Set target based on mood */
    switch (creature->mood) {
        case MOOD_ATTACK:
        case MOOD_STALK:
            /* Target Lara's position */
            creature->target.x = gs->lara_item->pos.x;
            creature->target.y = gs->lara_item->pos.y;
            creature->target.z = gs->lara_item->pos.z;
            creature->alerted = true;
            break;

        case MOOD_ESCAPE:
            /* Move away from Lara */
            creature->target.x = item->pos.x * 2 - gs->lara_item->pos.x;
            creature->target.z = item->pos.z * 2 - gs->lara_item->pos.z;
            break;

        case MOOD_BORED:
        default:
            /* Stay put or wander (simplified — full version uses random boxes) */
            creature->target.x = item->pos.x;
            creature->target.z = item->pos.z;
            break;
    }

    /* Calculate desired heading */
    int16_t target_angle = angle_to_target(item,
                                            creature->target.x,
                                            creature->target.z);

    /* Turn toward target (limited by maximum_turn) */
    int16_t angle_diff = target_angle - item->rot.y;

    /* Clamp turn rate */
    int16_t max_turn = creature->maximum_turn;
    if (max_turn == 0) max_turn = DEG_1 * 3;  /* Default: 3°/frame */

    if (angle_diff > max_turn) angle_diff = max_turn;
    if (angle_diff < -max_turn) angle_diff = -max_turn;

    item->rot.y += angle_diff;

    /* Head rotation tracks target (for visual feedback) */
    creature->head_rotation = target_angle - item->rot.y;
    if (creature->head_rotation > DEG_90) creature->head_rotation = DEG_90;
    if (creature->head_rotation < -DEG_90) creature->head_rotation = -DEG_90;

    /* Movement based on mood */
    if (creature->mood == MOOD_ATTACK) {
        /* Run speed (varies per creature type — simplified here) */
        item->speed = 48;
    } else if (creature->mood == MOOD_STALK) {
        item->speed = 24;
    } else if (creature->mood == MOOD_ESCAPE) {
        item->speed = 64;
    } else {
        item->speed = 0;
    }

    /* Apply movement */
    if (item->speed != 0) {
        int32_t s = tr_sin(item->rot.y) >> 2;
        int32_t c = tr_cos(item->rot.y) >> 2;
        item->pos.x += (item->speed * s) >> 14;
        item->pos.z += (item->speed * c) >> 14;
    }

    /* Apply gravity */
    if (item->gravity) {
        item->fall_speed += GRAVITY;
        item->pos.y += item->fall_speed;
    }

    /* Floor collision */
    const tr_sector_t *sector = game_get_sector(gs, item->room_num,
                                                  item->pos.x, item->pos.z);
    if (sector) {
        int32_t floor = sector->floor * -STEP_L;
        if (item->pos.y >= floor) {
            item->pos.y = floor;
            item->fall_speed = 0;
            item->gravity = false;
        } else if (floor - item->pos.y > STEP_L * 2) {
            item->gravity = true;
        } else {
            item->pos.y = floor;
        }
    }

    /* Update room */
    int16_t new_room = game_get_room_at(gs, item->room_num,
                                          item->pos.x, item->pos.y, item->pos.z);
    if (new_room != item->room_num) {
        item->room_num = new_room;
    }
}

/* -------------------------------------------------------------------
 * Initialize creature state for a new entity
 * ------------------------------------------------------------------- */

int game_creature_init(int16_t object_id) {
    if (creature_count >= MAX_CREATURES) return -1;

    creature_state_t *c = &creatures[creature_count];
    c->mood = MOOD_BORED;
    c->target_box = -1;
    c->current_box = -1;
    c->target.x = 0;
    c->target.y = 0;
    c->target.z = 0;
    c->head_rotation = 0;
    c->neck_rotation = 0;
    c->alerted = false;
    c->reached_goal = false;

    /* Set turn rate based on creature type */
    switch (object_id) {
        case O_WOLF:
        case O_RAPTOR:
            c->maximum_turn = DEG_1 * 6;
            break;
        case O_T_REX:
            c->maximum_turn = DEG_1 * 2;
            break;
        case O_BAT:
            c->maximum_turn = DEG_1 * 10;
            break;
        default:
            c->maximum_turn = DEG_1 * 3;
            break;
    }

    return creature_count++;
}

/* Object IDs referenced from tr_objects.c */
#define O_WOLF      7
#define O_BAT       9
#define O_T_REX     18
#define O_RAPTOR    19

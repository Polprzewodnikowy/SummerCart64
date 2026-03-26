/*
 * MW-N64: NPC AI packages
 *
 * INTEGRITY RULE: AI package types and parameters from OpenMW
 * (apps/openmw/mwmechanics/aiwander.cpp, aitravel.cpp, aifollow.cpp,
 * aiescort.cpp, aicombat.cpp). Package structure from loadnpc.hpp.
 *
 * Morrowind NPCs have AI packages that control their behavior:
 *   Wander:  Patrol within range of home position
 *   Travel:  Walk to a destination point
 *   Follow:  Follow a target NPC
 *   Escort:  Escort target to destination
 *   Combat:  Attack a target
 *   Activate: Use an object
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_character.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* AI package types (from OpenMW AiPackageTypeId) */
typedef enum {
    AI_WANDER = 0,
    AI_TRAVEL = 1,
    AI_ESCORT = 2,
    AI_FOLLOW = 3,
    AI_COMBAT = 4,
    AI_ACTIVATE = 5,
    AI_PURSUE = 6,
} ai_package_type_t;

/* AI package instance */
typedef struct {
    ai_package_type_t type;
    float dest_x, dest_y, dest_z;   /* Destination (travel/escort) */
    float wander_range;              /* Wander radius */
    int32_t wander_duration;         /* Duration in game hours */
    int32_t wander_time_of_day;      /* Time restriction */
    uint8_t wander_idle[8];          /* Idle chances (8 animations) */
    char target_id[64];              /* Target NPC (follow/escort/combat) */
    bool done;                       /* Package completed */
} ai_package_t;

/* NPC AI state */
typedef struct {
    ai_package_t packages[4];   /* Stack of AI packages (max 4) */
    int package_count;
    int current_package;

    /* Combat state */
    int16_t fight;     /* 0-100: aggressiveness (from NPC AI data) */
    int16_t flee;      /* 0-100: flee threshold */
    int16_t alarm;     /* 0-100: alert threshold */
    int16_t hello;     /* 0-100: greeting distance */
    bool in_combat;
    char combat_target[64];

    /* Wander state */
    float home_x, home_y, home_z;  /* Home position */
    float current_dest_x, current_dest_y, current_dest_z;
    int32_t idle_timer;
    int32_t idle_anim;

    /* Movement */
    float speed;        /* Current movement speed */
    float facing;       /* Current facing direction (radians) */
} npc_ai_state_t;

#define MAX_NPC_AI  256
static npc_ai_state_t s_npc_ai[MAX_NPC_AI];
static int s_npc_ai_count = 0;

/* -------------------------------------------------------------------
 * AI update (called per frame for each active NPC)
 *
 * From OpenMW mwmechanics/actors.cpp:
 *   1. Check if NPC should enter combat (fight threshold + detection)
 *   2. Execute current AI package
 *   3. If package done, pop and start next
 * ------------------------------------------------------------------- */

void ai_update_npc(int npc_index, float npc_x, float npc_y, float npc_z,
                    float player_x, float player_y, float player_z,
                    float dt) {
    if (npc_index < 0 || npc_index >= s_npc_ai_count) return;
    npc_ai_state_t *ai = &s_npc_ai[npc_index];

    if (ai->package_count == 0) return;
    ai_package_t *pkg = &ai->packages[ai->current_package];

    /* Distance to player */
    float dx = player_x - npc_x;
    float dy = player_y - npc_y;
    float dist_sq = dx * dx + dy * dy;

    /* Combat check: if fight > threshold and player in range */
    if (!ai->in_combat && ai->fight > 80 && dist_sq < 4096.0f * 4096.0f) {
        ai->in_combat = true;
        strncpy(ai->combat_target, "player", 63);
    }

    if (ai->in_combat) {
        /* Combat: move toward target and attack */
        if (dist_sq > 128.0f * 128.0f) {
            /* Move toward player */
            float inv_dist = 1.0f / (1.0f + (dx * dx + dy * dy));
            ai->current_dest_x = player_x;
            ai->current_dest_y = player_y;
            ai->speed = 200.0f;  /* Run speed */
            (void)inv_dist;
        } else {
            ai->speed = 0;
            /* In melee range — attack logic handled by combat system */
        }
        return;
    }

    switch (pkg->type) {
        case AI_WANDER: {
            /* Wander: pick random point within range, walk there, idle */
            if (ai->idle_timer > 0) {
                ai->idle_timer -= (int32_t)(dt * 30.0f);
                ai->speed = 0;
            } else {
                /* Walk toward current destination */
                float ddx = ai->current_dest_x - npc_x;
                float ddy = ai->current_dest_y - npc_y;
                float dd = ddx * ddx + ddy * ddy;
                if (dd < 64.0f * 64.0f) {
                    /* Reached destination — idle for a while */
                    ai->idle_timer = 30 * 3;  /* 3 seconds */
                    ai->speed = 0;
                    /* Pick new random destination within range */
                    /* Simplified: just stay near home */
                    ai->current_dest_x = ai->home_x + ((npc_index * 37) % 200) - 100;
                    ai->current_dest_y = ai->home_y + ((npc_index * 53) % 200) - 100;
                } else {
                    ai->speed = 100.0f;  /* Walk speed */
                }
            }
            break;
        }

        case AI_TRAVEL: {
            float ddx = pkg->dest_x - npc_x;
            float ddy = pkg->dest_y - npc_y;
            if (ddx * ddx + ddy * ddy < 128.0f * 128.0f) {
                pkg->done = true;
            } else {
                ai->current_dest_x = pkg->dest_x;
                ai->current_dest_y = pkg->dest_y;
                ai->speed = 150.0f;
            }
            break;
        }

        case AI_FOLLOW: {
            /* Follow player or target NPC */
            ai->current_dest_x = player_x;
            ai->current_dest_y = player_y;
            if (dist_sq > 256.0f * 256.0f) {
                ai->speed = 200.0f;  /* Run to catch up */
            } else if (dist_sq > 128.0f * 128.0f) {
                ai->speed = 100.0f;  /* Walk alongside */
            } else {
                ai->speed = 0;  /* Close enough */
            }
            break;
        }

        default:
            break;
    }
}

/* Initialize AI for an NPC from ESM data */
int ai_init_npc(float home_x, float home_y, float home_z,
                 int16_t fight, int16_t flee, int16_t alarm, int16_t hello) {
    if (s_npc_ai_count >= MAX_NPC_AI) return -1;
    npc_ai_state_t *ai = &s_npc_ai[s_npc_ai_count];
    memset(ai, 0, sizeof(*ai));
    ai->home_x = home_x;
    ai->home_y = home_y;
    ai->home_z = home_z;
    ai->current_dest_x = home_x;
    ai->current_dest_y = home_y;
    ai->fight = fight;
    ai->flee = flee;
    ai->alarm = alarm;
    ai->hello = hello;
    return s_npc_ai_count++;
}

/* Add a wander package (most common NPC behavior) */
void ai_add_wander(int npc_index, float range, int duration, uint8_t idle[8]) {
    if (npc_index < 0 || npc_index >= s_npc_ai_count) return;
    npc_ai_state_t *ai = &s_npc_ai[npc_index];
    if (ai->package_count >= 4) return;

    ai_package_t *pkg = &ai->packages[ai->package_count++];
    pkg->type = AI_WANDER;
    pkg->wander_range = range;
    pkg->wander_duration = duration;
    if (idle) memcpy(pkg->wander_idle, idle, 8);
}

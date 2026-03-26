/*
 * MW-N64: Dialogue system — implementation
 *
 * INTEGRITY RULE: Dialogue condition evaluation follows the exact
 * filtering logic from OpenMW (apps/openmw/mwdialogue/filter.cpp).
 * INFO records are selected by matching NPC ID, race, class, faction,
 * cell, gender, disposition, and up to 6 custom conditions.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_dialogue.h"
#include "mw_character.h"
#include <string.h>

/* External state */
extern mw_player_t g_player;

/* -------------------------------------------------------------------
 * Condition evaluation
 *
 * From OpenMW mwdialogue/filter.cpp Filter::testDisposition,
 * Filter::testSelectStruct, etc.
 * ------------------------------------------------------------------- */

static bool compare_int(int32_t value, int32_t target, uint8_t op) {
    switch (op) {
        case 0: return value == target;
        case 1: return value != target;
        case 2: return value >  target;
        case 3: return value >= target;
        case 4: return value <  target;
        case 5: return value <= target;
        default: return false;
    }
}

static bool eval_condition(const dial_condition_t *cond) {
    /* func_index maps to the type of check:
     * From OpenMW: SelectWrapper::Function enum
     *   0-7: journal index, item count, etc.
     *   Values depend on the condition type byte.
     * For now: evaluate the simple cases. */
    (void)cond;
    return true;  /* TODO: evaluate against player/game state */
}

/* -------------------------------------------------------------------
 * INFO response matching
 *
 * From OpenMW mwdialogue/filter.cpp Filter::operator():
 *   1. Match NPC ID (if specified)
 *   2. Match race (if specified)
 *   3. Match class (if specified)
 *   4. Match faction + rank (if specified)
 *   5. Match cell (if specified)
 *   6. Match gender (if specified)
 *   7. Check disposition >= minimum
 *   8. Evaluate all custom conditions
 *   9. First matching INFO wins (order matters!)
 * ------------------------------------------------------------------- */

static bool response_matches(const dial_response_t *info, const char *npc_id) {
    /* NPC ID filter */
    if (info->npc_id[0] && strcmp(info->npc_id, npc_id) != 0)
        return false;

    /* Race filter */
    if (info->race[0] && strcmp(info->race, g_player.race) != 0) {
        /* This checks against the NPC's race, not the player's.
         * Simplified: skip race check if we don't have NPC data here. */
    }

    /* Gender filter */
    if (info->gender >= 0 && info->gender != g_player.gender)
        return false;

    /* Disposition filter */
    /* TODO: get NPC's current disposition toward player */

    /* Custom conditions */
    for (int i = 0; i < info->condition_count; i++) {
        if (!eval_condition(&info->conditions[i]))
            return false;
    }

    return true;
}

/* -------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------- */

void dial_begin(dial_state_t *ds, int npc_item_index, const char *npc_id) {
    memset(ds, 0, sizeof(*ds));
    ds->npc_item_index = npc_item_index;
    strncpy(ds->npc_id, npc_id, 63);
}

void dial_get_topics(dial_state_t *ds) {
    /*
     * Scan DIAL records, for each topic check if any INFO matches.
     * Topics with at least one matching INFO are available.
     *
     * TODO: requires ESM DIAL/INFO records loaded into memory.
     * Currently stubbed — topics will be populated when ESM
     * dialogue parsing is implemented.
     */
    ds->topic_count = 0;

    /* Always available: background, latest rumors, little advice */
    strncpy(ds->topics[ds->topic_count++], "Background", 63);
    strncpy(ds->topics[ds->topic_count++], "Latest Rumors", 63);
    strncpy(ds->topics[ds->topic_count++], "Little Advice", 63);
}

bool dial_select_topic(dial_state_t *ds, const char *topic) {
    /*
     * Find the best matching INFO for this topic + current NPC.
     * INFO records are checked in order — first match wins.
     *
     * TODO: iterate DIAL[topic].INFO records, call response_matches().
     */
    (void)ds;
    (void)topic;
    (void)response_matches;
    return false;
}

void dial_end(dial_state_t *ds) {
    memset(ds, 0, sizeof(*ds));
}

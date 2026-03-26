#pragma once

#include <trx/game/items.h>

typedef enum {
    PROJECTILE_HIT_NONE,
    PROJECTILE_HIT_SHATTER, // some objects were shattered
    PROJECTILE_HIT_STOP, // a hard object (eg a bell) has been hit
} PROJECTILE_HIT;

typedef enum {
    GUN_SMASH_POLICY_NONE,
    GUN_SMASH_POLICY_CONTINUE,
    GUN_SMASH_POLICY_STOP,
    GUN_SMASH_POLICY_HEAVY,
} GUN_SMASH_POLICY;

GUN_SMASH_POLICY Gun_GetSmashPolicy(const ITEM *item);

void Gun_SmashItem(int16_t item_num);

PROJECTILE_HIT Gun_SmashItems(
    GAME_VECTOR start, GAME_VECTOR target, XYZ_32 *out_hit_pos,
    OBJECT_ID missile_obj_id);

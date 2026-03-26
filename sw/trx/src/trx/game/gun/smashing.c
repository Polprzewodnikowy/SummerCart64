#include <trx/game/gun/smashing.h>

#include <trx/game/collision/los.h>
#include <trx/game/gun/misc.h>
#include <trx/game/objects/vars.h>

// TODO: meh
extern void Smashable_Smash(int16_t item_num);

GUN_SMASH_POLICY Gun_GetSmashPolicy(const ITEM *const item)
{
    if (Object_IsType(item->object_id, g_ShatterableObjects)) {
        return GUN_SMASH_POLICY_CONTINUE;
    }
    if (Object_IsType(item->object_id, g_SmashableObjects)) {
        return GUN_SMASH_POLICY_STOP;
    }
    if (Object_IsType(item->object_id, g_HeavyShatterableObjects)) {
        return GUN_SMASH_POLICY_HEAVY;
    }
    return GUN_SMASH_POLICY_NONE;
}

void Gun_SmashItem(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    switch (item->object_id) {
    case O_SMASH_OBJECT_1:
    case O_SMASH_OBJECT_4:
        Smashable_Smash(item_num);
        break;

    case O_BELL:
    case O_CARCASS:
    case O_FUSE_BOX:
    case O_SMASH_OBJECT_2:
    case O_SMASH_OBJECT_3:
        if (item->status != IS_ACTIVE) {
            item->status = IS_ACTIVE;
            Item_AddActive(item_num);
        }
        break;

    case O_SCION_ITEM_3:
        Gun_HitTarget(item, nullptr, nullptr, item->hit_points);
        break;

    default:
        break;
    }
}

PROJECTILE_HIT Gun_SmashItems(
    const GAME_VECTOR start, const GAME_VECTOR target,
    XYZ_32 *const out_hit_pos, const OBJECT_ID missile_obj_id)
{
    int32_t hits = 0;
    int16_t last_item_num = NO_ITEM;
    const bool is_heavy_missile =
        Object_IsType(missile_obj_id, g_HeavyMissileObjects);
    while (true) {
        const int16_t item_num = LOS_CheckSmashable(start, target, out_hit_pos);
        if (item_num == NO_ITEM || item_num == last_item_num) {
            break;
        }
        last_item_num = item_num;

        const ITEM *const item = Item_Get(item_num);
        const GUN_SMASH_POLICY policy = Gun_GetSmashPolicy(item);
        switch (policy) {
        case GUN_SMASH_POLICY_HEAVY:
            if (is_heavy_missile) {
                Gun_SmashItem(item_num);
            }
            return PROJECTILE_HIT_STOP;
        case GUN_SMASH_POLICY_STOP:
            Gun_SmashItem(item_num);
            return PROJECTILE_HIT_STOP;
        case GUN_SMASH_POLICY_CONTINUE:
            Gun_SmashItem(item_num);
            hits++;
            break;
        default:
            break;
        }
    }
    return hits > 0 ? PROJECTILE_HIT_SHATTER : PROJECTILE_HIT_NONE;
}

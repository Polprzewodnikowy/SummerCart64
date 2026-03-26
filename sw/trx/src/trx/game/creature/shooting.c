#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/fx/gun_flash.h>
#include <trx/game/gun.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks/spawners.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

#define M_SHOOT_TARGETING_SPEED 300
#define M_SHOOT_HIT_CHANCE 0x2000

static void M_CalcShootVectors(
    const ITEM *const item, const ITEM *const target_item, XYZ_32 *const start,
    XYZ_32 *const target)
{
    start->x = item->pos.x;
    start->y = item->pos.y - STEP_L * 3;
    start->z = item->pos.z;

    target->x = target_item->pos.x;
    target->y = target_item->pos.y - STEP_L * 3;
    target->z = target_item->pos.z;

    const int16_t angle = XYZ_32_GetYaw((XYZ_32) {
        .x = target->x - start->x,
        .y = target->y - start->y,
        .z = target->z - start->z,
    });

    const int32_t dist = WALL_L * 2;
    target->x += (dist * Math_Sin(angle)) >> W2V_SHIFT;
    target->z += (dist * Math_Cos(angle)) >> W2V_SHIFT;
}

static void M_TriggerTR3GunShell(
    const ITEM *const item, const CREATURE_GUN *const gun)
{
    const int16_t effect_num = Effect_Create(item->room_num);
    if (effect_num == NO_EFFECT) {
        return;
    }

    XYZ_32 pos = {
        .x = gun->muzzle.pos.x >> 2,
        .y = gun->muzzle.pos.y >> 2,
        .z = gun->muzzle.pos.z >> 2,
    };
    Collide_GetJointAbsPosition(item, &pos, gun->muzzle.mesh_num);

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos = pos;
    effect->room_num = item->room_num;
    effect->rot.x = 0;
    effect->rot.y = 0;
    effect->rot.z = (int16_t)Random_GetControl();
    effect->speed = (int16_t)((Random_GetControl() & 0x1F) + 16);
    effect->object_id = O_GUN_SHELL;
    effect->fall_speed = (int16_t)(-48 - (Random_GetControl() & 7));
    effect->frame_num = Object_Get(O_GUN_SHELL)->mesh_idx;
    effect->shade = 0x4210;
    effect->counter = 1;
    effect->flag1 = item->rot.y + (Random_GetControl() & 0xFFF) - 0x4800;
}

static void M_TriggerTR3GunSmoke(
    const ITEM *const item, const CREATURE_GUN *const gun)
{
    XYZ_32 pos = {
        .x = gun->muzzle.pos.x - (gun->muzzle.pos.x >> 2),
        .y = gun->muzzle.pos.y - (gun->muzzle.pos.y >> 2),
        .z = gun->muzzle.pos.z - (gun->muzzle.pos.z >> 2),
    };
    Collide_GetJointAbsPosition(item, &pos, gun->muzzle.mesh_num);

    GAME_VECTOR smoke_pos = {
        .pos = pos,
        .room_num = item->room_num,
    };
    Room_GetSector(smoke_pos.pos, &smoke_pos.room_num);
    Sparks_TriggerGunSmoke(smoke_pos, true, LGT_PISTOLS, 32);
}

bool Creature_Shoot(
    ITEM *const item, const AI_INFO *const info, const CREATURE_GUN *const gun,
    const int16_t extra_rotation, const int32_t damage)
{
    const ITEM *const lara_item = Lara_GetItem();
    const CREATURE *const creature = item->creature_data;
    ITEM *const target_item = creature->enemy;

    if (g_TRVersion == 3) {
        M_TriggerTR3GunShell(item, gun);
        M_TriggerTR3GunSmoke(item, gun);
    }

    bool is_targetable;
    bool is_hit;
    if (g_TRVersion == 1) {
        // TR1 targeting is a bit dumb - eg with some minimal effort, Pierre
        // can't reach Lara in Folly. This branch preserves this behavior.
        if (info->distance > CREATURE_SHOOT_RANGE) {
            is_targetable = false;
            is_hit = false;
        } else {
            is_hit = Random_GetControl()
                < ((CREATURE_SHOOT_RANGE - info->distance)
                       / (CREATURE_SHOOT_RANGE / 0x7FFF)
                   - CREATURE_MISS_CHANCE);
            is_targetable = true;
        }
    } else {
        if (info->distance > CREATURE_SHOOT_RANGE
            || !Creature_CanTargetEnemy(item, info)) {
            is_targetable = false;
            is_hit = false;
        } else {
            int32_t distance =
                (((target_item->speed * Math_Sin(info->enemy_facing))
                  >> W2V_SHIFT)
                 * CREATURE_SHOOT_RANGE)
                / M_SHOOT_TARGETING_SPEED;
            distance = info->distance + SQUARE(distance);
            if (distance > CREATURE_SHOOT_RANGE) {
                is_hit = false;
            } else {
                const int32_t chance = M_SHOOT_HIT_CHANCE
                    + (CREATURE_SHOOT_RANGE - info->distance)
                        / (CREATURE_SHOOT_RANGE / 0x5000);
                is_hit = Random_GetControl() < chance;
            }
            is_targetable = true;
        }
    }

    int16_t effect_num = NO_EFFECT;
    if (target_item == lara_item) {
        if (is_hit) {
            effect_num = Creature_Effect(item, &gun->muzzle, Spawn_GunHit);
            Item_TakeDamage(target_item, damage, true);
        } else if (is_targetable) {
            effect_num = Creature_Effect(item, &gun->muzzle, Spawn_GunMiss);
        }
    } else {
        effect_num = Creature_Effect(item, &gun->muzzle, Spawn_GunShot);
        if (is_hit) {
            Item_TakeDamage(target_item, damage / 10, true);

            const OBJECT *const target_obj = Object_Get(target_item->object_id);
            int32_t joint = Random_GetControl() & 0xF;
            if (joint >= target_obj->mesh_count) {
                joint = 0;
            }

            XYZ_32 pos = {};
            Collide_GetJointAbsPosition(target_item, &pos, joint);
            Spawn_Blood(
                pos.x, pos.y, pos.z, target_item->speed, target_item->rot.y,
                target_item->room_num);
        }
    }

    if (FX_GunFlash_Spawn(item, gun) && effect_num != NO_EFFECT) {
        // Kill the old-style flash effect just spawned from previous chunk
        Effect_Kill(effect_num);
        effect_num = NO_EFFECT;
    }

    if (effect_num != NO_EFFECT) {
        Effect_Get(effect_num)->rot.y += extra_rotation;
    }

    XYZ_32 start, target;
    M_CalcShootVectors(item, target_item, &start, &target);
    Gun_SmashItems(
        (GAME_VECTOR) {
            .pos = start,
            .room_num = item->room_num,
        },
        (GAME_VECTOR) {
            .pos = target,
            .room_num = target_item->room_num,
        },
        nullptr, NO_OBJECT);

    return is_targetable;
}

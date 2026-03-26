#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/fx/water.h>
#include <trx/game/gun/misc.h>
#include <trx/game/gun/smashing.h>
#include <trx/game/gun/vars.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects/general/smashable.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_SPEED 200
#define M_FALL_SPEED (M_SPEED - 10) // = 190

static int32_t M_GetBlastRadius(void)
{
    return g_Config.gameplay.enable_bouncy_grenades ? WALL_L : WALL_L / 2;
}

static void M_SetTR3ProjectileShade(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    // OG TR3 uses `item->shade = -0x3DF0` on projectiles; in TRX any negative
    // shade forces the dynamic/smoothed lighting path.
    item->shade.value_1 = -1;
    item->shade.value_2 = -1;
}

static XYZ_32 M_GetLocalZOffset(const ITEM *const item, const int32_t dist)
{
    const int32_t cx = Math_Cos(item->rot.x);
    const int32_t sx = Math_Sin(item->rot.x);
    const int32_t cy = Math_Cos(item->rot.y);
    const int32_t sy = Math_Sin(item->rot.y);

    const int32_t horz = (dist * cx) >> W2V_SHIFT;
    return (XYZ_32) {
        .x = (horz * sy) >> W2V_SHIFT,
        .y = -(dist * sx) >> W2V_SHIFT,
        .z = (horz * cy) >> W2V_SHIFT,
    };
}

static void M_Explode(int16_t grenade_item_num, const XYZ_32 pos)
{
    const ITEM *const grenade_item = Item_Get(grenade_item_num);
    const ROOM *const room = Room_Get(grenade_item->room_num);
    const bool is_underwater = room != nullptr && room->flags.underwater;

    if (g_TRVersion == 3) {
        if (is_underwater) {
            Sparks_TriggerUnderwaterExplosion(grenade_item);
        } else {
            Sparks_TriggerExplosionSparks(
                pos, 3, -2, 0, grenade_item->room_num);
            for (int32_t i = 0; i < 2; i++) {
                Sparks_TriggerExplosionSparks(
                    pos, 3, -1, 0, grenade_item->room_num);
            }
        }

        Sound_Effect(
            SFX_EXPLOSION_1, &grenade_item->pos, 0x1800000 | SPM_PITCH);
        Sound_Effect(SFX_EXPLOSION_2, &grenade_item->pos, SPM_NORMAL);
    } else {
        const int16_t effect_num = Effect_Create(grenade_item->room_num);
        if (effect_num != NO_EFFECT) {
            EFFECT *const effect = Effect_Get(effect_num);
            effect->pos = pos;
            effect->speed = 0;
            effect->frame_num = 0;
            effect->counter = 0;
            effect->object_id = O_EXPLOSION_1;
        }

        Sound_Effect(SFX_EXPLOSION_3, nullptr, SPM_NORMAL);
    }

    Creature_AlertNearbyGuards(grenade_item);
    Item_Kill(grenade_item_num);
}

static bool M_CanExplodeTarget(const ITEM *const item)
{
    const OBJECT *const object = Object_Get(item->object_id);
    if (object->can_be_exploded_func != nullptr) {
        return object->can_be_exploded_func(item);
    }

    // TODO: as some creatures have more than one death animation, have a
    // way to expose those specific ones for checking, or delegate
    // responsibility directly to the objects.
    const ITEM_ACTION action = ItemAction_ToGameID(ITEM_ACTION_FINISH_LEVEL);
    for (int32_t i = 0; i < object->anim_count; i++) {
        const ANIM *const anim = Object_GetAnim(object, i);
        if (Anim_HasFXCommand(anim, action)) {
            return false;
        }
    }

    return true;
}

static bool M_TryExplodeItem(
    const ITEM *const projectile_item, const GAME_VECTOR old_pos,
    const int16_t target_item_num, const int32_t radius)
{
    ITEM *const target_item = Item_Get(target_item_num);

    const OBJECT *const target_obj = Object_Get(target_item->object_id);
    if (target_item == Lara_GetItem()) {
        return false;
    }
    if (!target_item->collidable) {
        return false;
    }

    if (target_item->status == IS_INVISIBLE
        || target_obj->collision_func == nullptr) {
        return false;
    }

    if (!Item_CanBeProjectileTarget(target_item)) {
        return false;
    }

    const ANIM_FRAME *const frame = Item_GetBestFrame(target_item);
    const BOUNDS_16 *const bounds = &frame->bounds;

    const int32_t cdy = projectile_item->pos.y - target_item->pos.y;
    if (cdy + radius < bounds->min.y || cdy - radius > bounds->max.y) {
        return false;
    }

    const int32_t cy = Math_Cos(target_item->rot.y);
    const int32_t sy = Math_Sin(target_item->rot.y);
    const int32_t cdx = projectile_item->pos.x - target_item->pos.x;
    const int32_t cdz = projectile_item->pos.z - target_item->pos.z;
    const int32_t odx = old_pos.x - target_item->pos.x;
    const int32_t odz = old_pos.z - target_item->pos.z;

    const int32_t rx = (cy * cdx - sy * cdz) >> W2V_SHIFT;
    const int32_t sx = (cy * odx - sy * odz) >> W2V_SHIFT;
    if ((rx + radius < bounds->min.x && sx + radius < bounds->min.x)
        || (rx - radius > bounds->max.x && sx - radius > bounds->max.x)) {
        return false;
    }

    const int32_t rz = (sy * cdx + cy * cdz) >> W2V_SHIFT;
    const int32_t sz = (sy * odx + cy * odz) >> W2V_SHIFT;
    if ((rz + radius < bounds->min.z && sz + radius < bounds->min.z)
        || (rz - radius > bounds->max.z && sz - radius > bounds->max.z)) {
        return false;
    }

    if (!Item_CanTakeDamage(target_item)) {
        return false;
    }

    const GAME_VECTOR hit_pos = {
        .pos = projectile_item->pos,
        .room_num = projectile_item->room_num,
    };
    Gun_HitTarget(
        target_item, &old_pos, &hit_pos, g_Weapons[LGT_GRENADE].damage);
    Stats_AddAmmoHits();

    if (Gun_GetSmashPolicy(target_item) != GUN_SMASH_POLICY_NONE) {
        Gun_SmashItem(target_item_num);
    } else if (
        target_item->hit_points <= 0 && M_CanExplodeTarget(target_item)) {
        Creature_Die(target_item_num, true);
    }
    return true;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const GAME_VECTOR old_pos = {
        .pos = item->pos,
        .room_num = item->room_num,
    };

    const ROOM *const room = Room_Get(item->room_num);
    const bool was_underwater = room != nullptr && room->flags.underwater;

    if (g_Config.gameplay.enable_bouncy_grenades) {
        if (was_underwater) {
            item->fall_speed += (5 - item->fall_speed) >> 1;
            item->speed -= item->speed >> 2;
            if (item->speed != 0) {
                item->rot.z += DEG_1 * ((item->speed >> 4) + 3);
                if (item->required_anim_state != 0) {
                    item->rot.y += DEG_1 * ((item->speed >> 2) + 3);
                } else {
                    item->rot.x += DEG_1 * ((item->speed >> 2) + 3);
                }
            }
        } else {
            item->fall_speed += 3;
            if (item->speed != 0) {
                item->rot.z += DEG_1 * ((item->speed >> 2) + 7);
                if (item->required_anim_state != 0) {
                    item->rot.y += DEG_1 * ((item->speed >> 1) + 7);
                } else {
                    item->rot.x += DEG_1 * ((item->speed >> 1) + 7);
                }
            }
        }
    }

    if (g_TRVersion == 3) {
        M_SetTR3ProjectileShade(item);
        if (!was_underwater && item->speed != 0) {
            const XYZ_32 back_64 = M_GetLocalZOffset(item, -64);
            Sparks_TriggerRocketSmoke(
                (XYZ_32) {
                    .x = item->pos.x + back_64.x,
                    .y = item->pos.y + back_64.y,
                    .z = item->pos.z + back_64.z,
                },
                -1, item->room_num);
        }
    }

    bool explode = false;
    int32_t radius = 0;

    if (g_Config.gameplay.enable_bouncy_grenades) {
        const XYZ_32 vel = {
            .x = (item->speed * Math_Sin(item->goal_anim_state)) >> W2V_SHIFT,
            .y = item->fall_speed,
            .z = (item->speed * Math_Cos(item->goal_anim_state)) >> W2V_SHIFT,
        };
        item->pos.x += vel.x;
        item->pos.y += vel.y;
        item->pos.z += vel.z;

        const int16_t y_rot = item->rot.y;
        item->rot.y = item->goal_anim_state;
        Collide_DoProperDetection(item, old_pos.pos);
        item->goal_anim_state = item->rot.y;
        item->rot.y = y_rot;

        if (item->hit_points > 0) {
            item->hit_points--;

            if (item->hit_points == 0) {
                radius = M_GetBlastRadius();
                explode = true;
            }
        }
    } else {
        item->speed--;
        if (item->speed < M_FALL_SPEED) {
            item->fall_speed++;
        }
        item->pos.y += item->fall_speed
            - ((item->speed * Math_Sin(item->rot.x)) >> W2V_SHIFT);

        const int16_t speed =
            (item->speed * Math_Cos(item->rot.x)) >> W2V_SHIFT;
        item->pos.z += (speed * Math_Cos(item->rot.y)) >> W2V_SHIFT;
        item->pos.x += (speed * Math_Sin(item->rot.y)) >> W2V_SHIFT;

        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
        item->floor = Room_GetHeight(sector, item->pos);
        Item_UpdateRoom(item_num, room_num);

        if (item->pos.y >= item->floor
            || item->pos.y <= Room_GetCeiling(sector, item->pos)) {
            radius = M_GetBlastRadius();
            explode = true;
        }
    }

    if (g_TRVersion == 3) {
        const ROOM *const new_room = Room_Get(item->room_num);
        const bool is_underwater =
            new_room != nullptr && new_room->flags.underwater;
        if (is_underwater && !was_underwater) {
            const int32_t inner_y_vel =
                -2048 - ((int32_t)item->fall_speed << 5);
            const int32_t middle_y_vel =
                -1024 - ((int32_t)item->fall_speed << 4);
            FX_Water_SetupSplash(&(FX_WATER_SPLASH_SETUP) {
                .x = item->pos.x,
                .y = new_room->max_ceiling,
                .z = item->pos.z,
                .inner_xz_off = 16,
                .inner_xz_size = 12,
                .inner_y_size = -96,
                .inner_xz_vel = 160,
                .inner_gravity = 128,
                .inner_y_vel = inner_y_vel,
                .inner_friction = 7,
                .middle_xz_off = 24,
                .middle_xz_size = 24,
                .middle_y_size = -64,
                .middle_xz_vel = 224,
                .middle_gravity = 72,
                .middle_y_vel = middle_y_vel,
                .middle_friction = 8,
                .outer_xz_off = 32,
                .outer_xz_size = 32,
                .outer_xz_vel = 272,
                .outer_friction = 9,
            });
        }
    }

    const GAME_VECTOR new_pos = {
        .pos = item->pos,
        .room_num = item->room_num,
    };
    if (Gun_SmashItems(old_pos, new_pos, nullptr, item->object_id)
        == PROJECTILE_HIT_STOP) {
        explode = true;
        radius = M_GetBlastRadius();
    }

    if (g_Config.gameplay.projectile_area_damage
        == PROJECTILE_AREA_DAMAGE_MULTI_SWEEP) {
        Room_GetNearbyRooms(item->pos, radius * 4, radius * 4, item->room_num);
        for (int32_t i = 0; i < Room_DrawGetCount(); i++) {
            const ROOM *const nearby_room = Room_Get(Room_DrawGetRoom(i));
            for (int16_t target_item_num = nearby_room->item_num;
                 target_item_num != NO_ITEM;
                 target_item_num = Item_Get(target_item_num)->next_item) {
                if (!M_TryExplodeItem(item, old_pos, target_item_num, radius)) {
                    continue;
                }

                if (!explode) {
                    explode = true;
                    radius = M_GetBlastRadius();
                    i = -1;
                    break;
                }
            }
        }
    } else {
        for (int16_t target_item_num = room->item_num;
             target_item_num != NO_ITEM;
             target_item_num = Item_Get(target_item_num)->next_item) {
            if (M_TryExplodeItem(item, old_pos, target_item_num, radius)) {
                explode = true;
            }
        }
    }

    if (explode) {
        M_Explode(item_num, old_pos.pos);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_position = true;
}

REGISTER_OBJECT(O_GRENADE, M_Setup)

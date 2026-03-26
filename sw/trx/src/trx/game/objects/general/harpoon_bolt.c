#include <trx/core/math.h>
#include <trx/game/game_buf.h>
#include <trx/game/gun/misc.h>
#include <trx/game/gun/smashing.h>
#include <trx/game/gun/vars.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects/vars.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>
#include <trx/game/stats.h>
#include <trx/version.h>

typedef struct {
    int16_t base_x_rot;
    bool base_x_rot_valid;
} M_PRIV;

#define M_TR3_HIT_POINTS 256
#define M_TR3_WOBBLE_START 192
#define M_TR3_SPEED_UW 128
#define M_TR3_SPEED_AIR 256

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

static void M_Initialise_TR3(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->priv == nullptr) {
        item->priv = GameBuf_Alloc(sizeof(M_PRIV), GBUF_ITEM_DATA);
    }
    M_PRIV *const p = item->priv;
    p->base_x_rot = 0;
    p->base_x_rot_valid = false;
}

static void M_Control_TR3(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    const GAME_VECTOR old_pos = {
        .pos = item->pos,
        .room_num = item->room_num,
    };

    M_SetTR3ProjectileShade(item);

    item->pos.x += (item->speed * Math_Sin(item->rot.y)) >> W2V_SHIFT;
    item->pos.y += item->fall_speed;
    item->pos.z += (item->speed * Math_Cos(item->rot.y)) >> W2V_SHIFT;

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    item->floor = Room_GetHeight(sector, item->pos);
    Item_UpdateRoom(item_num, room_num);

    const GAME_VECTOR new_pos = {
        .pos = item->pos,
        .room_num = item->room_num,
    };
    if (Gun_SmashItems(old_pos, new_pos, nullptr, item->object_id)
        == PROJECTILE_HIT_STOP) {
        Item_Kill(item_num);
        return;
    }

    for (int16_t target_num = Room_Get(item->room_num)->item_num;
         target_num != NO_ITEM; target_num = Item_Get(target_num)->next_item) {
        ITEM *const target_item = Item_Get(target_num);

        if (target_item == Lara_GetItem() || item_num == target_num) {
            continue;
        }

        if (!target_item->collidable) {
            continue;
        }

        if (!Item_CanBeProjectileTarget(target_item)) {
            continue;
        }

        const ANIM_FRAME *const frame = Item_GetBestFrame(target_item);
        if (frame == nullptr) {
            continue;
        }
        const BOUNDS_16 *const bounds = &frame->bounds;

        const int32_t cdy = item->pos.y - target_item->pos.y;
        if (cdy < bounds->min.y || cdy > bounds->max.y) {
            continue;
        }

        const int32_t cy = Math_Cos(target_item->rot.y);
        const int32_t sy = Math_Sin(target_item->rot.y);
        const int32_t cdx = item->pos.x - target_item->pos.x;
        const int32_t cdz = item->pos.z - target_item->pos.z;
        const int32_t odx = old_pos.x - target_item->pos.x;
        const int32_t odz = old_pos.z - target_item->pos.z;

        const int32_t rx = (cy * cdx - sy * cdz) >> W2V_SHIFT;
        const int32_t sx = (cy * odx - sy * odz) >> W2V_SHIFT;
        if ((rx < bounds->min.x && sx < bounds->min.x)
            || (rx > bounds->max.x && sx > bounds->max.x)) {
            continue;
        }

        const int32_t rz = (sy * cdx + cy * cdz) >> W2V_SHIFT;
        const int32_t sz = (sy * odx + cy * odz) >> W2V_SHIFT;
        if ((rz < bounds->min.z && sz < bounds->min.z)
            || (rz > bounds->max.z && sz > bounds->max.z)) {
            continue;
        }

        if (Item_CanTakeDamage(target_item)) {
            if (Item_ShouldSpawnBlood(target_item)) {
                Spawn_BloodBath(
                    item->pos.x, item->pos.y, item->pos.z, 0, 0, item->room_num,
                    3);
            }
            const GAME_VECTOR hit_pos = { .pos = item->pos,
                                          .room_num = item->room_num };
            Gun_HitTarget(
                target_item, &old_pos, &hit_pos, g_Weapons[LGT_HARPOON].damage);
            Stats_AddAmmoHits();
        }

        Item_Kill(item_num);
        return;
    }

    const int32_t ceiling = Room_GetCeiling(sector, item->pos);
    if (item->pos.y >= item->floor || item->pos.y <= ceiling) {
        if (item->hit_points <= 0) {
            item->hit_points = M_TR3_HIT_POINTS;
        }

        if (item->hit_points == M_TR3_HIT_POINTS) {
            if (p != nullptr) {
                p->base_x_rot = item->rot.x;
                p->base_x_rot_valid = true;
            }
        }

        if (item->hit_points >= M_TR3_WOBBLE_START) {
            const int32_t base_x_rot = (p != nullptr && p->base_x_rot_valid)
                ? p->base_x_rot
                : item->rot.x;
            const int32_t wobble_angle =
                (item->hit_points & 7) * (DEG_360 / 16);
            const int32_t wobble = (Math_Sin(wobble_angle) >> 3) - 1024;
            item->rot.x =
                (int16_t)(base_x_rot
                          + (((item->hit_points - M_TR3_WOBBLE_START) * wobble)
                             >> 6));
            item->hit_points--;
        }

        item->hit_points--;
        if (item->hit_points <= 0) {
            Item_Kill(item_num);
            return;
        }

        item->fall_speed = 0;
        item->speed = 0;
        return;
    }

    item->rot.z += 35 * DEG_1;

    const ROOM *const room = Room_Get(item->room_num);
    if (room != nullptr && room->flags.underwater) {
        const int32_t time4 = Output_GetTimeInGame() * 4;
        if ((time4 & 0xF) == 0) {
            Spawn_BubbleEx(&item->pos, item->room_num, 2, 8);
        }
        Sparks_TriggerRocketSmoke(item->pos, 64, item->room_num);

        item->fall_speed =
            (int16_t)((-M_TR3_SPEED_UW * Math_Sin(item->rot.x)) >> W2V_SHIFT);
        item->speed =
            (int16_t)((M_TR3_SPEED_UW * Math_Cos(item->rot.x)) >> W2V_SHIFT);
    } else {
        item->rot.x -= DEG_1;
        if (item->rot.x < -DEG_90) {
            item->rot.x = -DEG_90;
        }

        item->fall_speed =
            (int16_t)((-M_TR3_SPEED_AIR * Math_Sin(item->rot.x)) >> W2V_SHIFT);
        item->speed =
            (int16_t)((M_TR3_SPEED_AIR * Math_Cos(item->rot.x)) >> W2V_SHIFT);
    }
}

static void M_Control_TR12(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const GAME_VECTOR old_pos = {
        .pos = item->pos,
        .room_num = item->room_num,
    };

    if (!Room_Get(item->room_num)->flags.underwater) {
        item->fall_speed += GRAVITY / 2;
    }

    item->pos.x += (item->speed * Math_Sin(item->rot.y)) >> W2V_SHIFT;
    item->pos.z += (item->speed * Math_Cos(item->rot.y)) >> W2V_SHIFT;
    item->pos.y += item->fall_speed;

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    item->floor = Room_GetHeight(sector, item->pos);
    Item_UpdateRoom(item_num, room_num);

    const GAME_VECTOR new_pos = {
        .pos = item->pos,
        .room_num = item->room_num,
    };

    bool hit = false;
    if (Gun_SmashItems(old_pos, new_pos, nullptr, item->object_id)
        == PROJECTILE_HIT_STOP) {
        hit = true;
    }

    for (int16_t target_num = Room_Get(item->room_num)->item_num;
         target_num != NO_ITEM; target_num = Item_Get(target_num)->next_item) {
        ITEM *const target_item = Item_Get(target_num);
        const OBJECT *const target_obj = Object_Get(target_item->object_id);

        if (target_item == Lara_GetItem() || item_num == target_num) {
            continue;
        }

        if (!target_item->collidable) {
            continue;
        }

        if (!Item_CanBeProjectileTarget(target_item)) {
            continue;
        }

        const ANIM_FRAME *const frame = Item_GetBestFrame(target_item);
        if (frame == nullptr) {
            continue;
        }
        const BOUNDS_16 *const bounds = &frame->bounds;

        const int32_t cdy = item->pos.y - target_item->pos.y;
        if (cdy < bounds->min.y || cdy > bounds->max.y) {
            continue;
        }

        const int32_t cy = Math_Cos(target_item->rot.y);
        const int32_t sy = Math_Sin(target_item->rot.y);
        const int32_t cdx = item->pos.x - target_item->pos.x;
        const int32_t cdz = item->pos.z - target_item->pos.z;
        const int32_t odx = old_pos.x - target_item->pos.x;
        const int32_t odz = old_pos.z - target_item->pos.z;

        const int32_t rx = (cy * cdx - sy * cdz) >> W2V_SHIFT;
        const int32_t sx = (cy * odx - sy * odz) >> W2V_SHIFT;
        if ((rx < bounds->min.x && sx < bounds->min.x)
            || (rx > bounds->max.x && sx > bounds->max.x)) {
            continue;
        }

        const int32_t rz = (sy * cdx + cy * cdz) >> W2V_SHIFT;
        const int32_t sz = (sy * odx + cy * odz) >> W2V_SHIFT;
        if ((rz < bounds->min.z && sz < bounds->min.z)
            || (rz > bounds->max.z && sz > bounds->max.z)) {
            continue;
        }

        if (Item_CanTakeDamage(target_item)) {
            if (Item_ShouldSpawnBlood(target_item)) {
                Spawn_BloodBath(
                    item->pos.x, item->pos.y, item->pos.z, 0, 0, item->room_num,
                    5);
            }
            const GAME_VECTOR hit_pos = {
                .pos = item->pos,
                .room_num = item->room_num,
            };
            Gun_HitTarget(
                target_item, &old_pos, &hit_pos, g_Weapons[LGT_HARPOON].damage);
            Stats_AddAmmoHits();
        }
        hit = true;
        break;
    }

    if (!hit) {
        const int32_t ceiling = Room_GetCeiling(sector, item->pos);
        if (item->pos.y >= item->floor || item->pos.y <= ceiling) {
            hit = true;
        }
    }

    if (hit) {
        Item_Kill(item_num);
    } else if (Room_Get(item->room_num)->flags.underwater) {
        Spawn_Bubble(&item->pos, item->room_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = g_TRVersion == 3 ? M_Initialise_TR3 : nullptr;
    obj->control_func = g_TRVersion == 3 ? M_Control_TR3 : M_Control_TR12;
    obj->save_position = true;
}

REGISTER_OBJECT(O_HARPOON_BOLT, M_Setup)

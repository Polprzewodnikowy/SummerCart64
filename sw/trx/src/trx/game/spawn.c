#include <trx/game/spawn.h>

#include <trx/core/math.h>
#include <trx/game/collision.h>
#include <trx/game/collision/los.h>
#include <trx/game/effects.h>
#include <trx/game/fx/water.h>
#include <trx/game/gun/vars.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/sparks/spawners.h>
#include <trx/version.h>

static void M_ShootAtLara(EFFECT *const effect)
{
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - effect->pos.x;
    const int32_t dy = lara_item->pos.y - effect->pos.y;
    const int32_t dz = lara_item->pos.z - effect->pos.z;

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(lara_item);
    const int32_t dist_vert =
        dy + bounds->max.y + 3 * (bounds->min.y - bounds->max.y) / 4;
    const int32_t dist_horz = Math_Sqrt(SQUARE(dz) + SQUARE(dx));
    effect->rot.x = -Math_Atan(dist_horz, dist_vert);
    effect->rot.y = Math_Atan(dz, dx);
    effect->rot.x += (Random_GetControl() - 0x4000) / 64;
    effect->rot.y += (Random_GetControl() - 0x4000) / 64;
}

XYZ_32 Spawn_GetRayPos(
    const GAME_VECTOR start, GAME_VECTOR hit_pos, const int32_t dist)
{
    // Get the position at wall
    LOS_Check(&start, &hit_pos, true);

    // Retract a bit
    const int16_t angle = XYZ_32_GetYaw((XYZ_32) {
        .x = hit_pos.x - start.x,
        .y = hit_pos.y - start.y,
        .z = hit_pos.z - start.z,
    });
    hit_pos.pos.x -= (dist * Math_Sin(angle)) >> W2V_SHIFT;
    hit_pos.pos.z -= (dist * Math_Cos(angle)) >> W2V_SHIFT;

    return hit_pos.pos;
}

void Spawn_Splash(const ITEM *const item)
{
    if (g_TRVersion == 3) {
        FX_Water_Splash(item);
        return;
    }

    const int32_t water_height = Room_GetWaterHeight(item->pos, item->room_num);
    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);

    for (int32_t i = 0; i < 10; i++) {
        const int16_t effect_num = Effect_Create(room_num);
        if (effect_num == NO_EFFECT) {
            continue;
        }

        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_SPLASH_1;
        effect->pos.x = item->pos.x;
        effect->pos.y = water_height;
        effect->pos.z = item->pos.z;
        effect->rot.y = 2 * Random_GetDraw() + DEG_180;
        effect->speed = Random_GetDraw() / 256;
        effect->frame_num = 0;
    }
}

void Spawn_Ricochet(const GAME_VECTOR pos)
{
    if (g_TRVersion == 3) {
        const ITEM *const lara_item = Lara_GetItem();
        const int32_t angle16 = Math_Atan(
            lara_item->pos.z - pos.pos.z, lara_item->pos.x - pos.pos.x);
        Sparks_TriggerRicochet(pos, ((uint16_t)angle16 >> 4) & 0x0FFF, 16);
        Sound_Effect(SFX_LARA_RICOCHET, &pos.pos, SPM_NORMAL);
        return;
    }

    const int16_t effect_num = Effect_Create(pos.room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_RICOCHET;
        effect->pos = pos.pos;
        effect->counter = 4;
        effect->frame_num = -3 * Random_GetDraw() / 0x8000;
        Sound_Effect(SFX_LARA_RICOCHET, &effect->pos, SPM_NORMAL);
    }
}

void Spawn_RicochetRay(const GAME_VECTOR start, GAME_VECTOR hit_pos)
{
    hit_pos.pos = Spawn_GetRayPos(start, hit_pos, STEP_L / 12);
    Spawn_Ricochet(hit_pos);
}

void Spawn_Bubble(const XYZ_32 *const pos, const int16_t room_num)
{
    if (g_TRVersion == 3) {
        Spawn_BubbleEx(pos, room_num, 8, 8);
        return;
    }

    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num == NO_EFFECT) {
        return;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos = *pos;
    effect->object_id = O_BUBBLE_1;
    effect->frame_num = -((Random_GetDraw() * 3) / 0x8000);
    effect->speed = 10 + ((Random_GetDraw() * 6) / 0x8000);
}

void Spawn_BubbleEx(
    const XYZ_32 *const pos, const int16_t room_num, const int32_t size,
    const int32_t size_range)
{
    if (g_TRVersion != 3) {
        Spawn_Bubble(pos, room_num);
        return;
    }

    int16_t water_room = room_num;
    Room_GetSector(*pos, &water_room);
    if (!Room_Get(water_room)->flags.underwater) {
        return;
    }

    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num == NO_EFFECT) {
        return;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos = *pos;
    effect->object_id = O_BUBBLE_1;
    effect->frame_num = 0;

    effect->speed = (Random_GetControl() & 0xFF) + 64;
    effect->fall_speed = (Random_GetControl() & 0x1F) + 32;

    Sparks_TriggerBubble(pos->x, pos->y, pos->z, size, size_range, effect_num);
}

int16_t Spawn_Blood(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    if (g_TRVersion == 3) {
        if (Room_Get(room_num)->flags.underwater) {
            FX_Water_TriggerUnderwaterBlood(
                (XYZ_32) { x, y, z }, Random_GetControl() & 7);
        } else {
            Sparks_TriggerBlood(
                (XYZ_32) { x, y, z }, y_rot >> 4,
                (Random_GetControl() & 7) + 6);
        }
        return NO_EFFECT;
    }

    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->pos.x = x;
        effect->pos.y = y;
        effect->pos.z = z;
        effect->rot.y = y_rot;
        effect->speed = speed;
        effect->frame_num = 0;
        effect->object_id = O_BLOOD_1;
        effect->counter = 0;
    }
    return effect_num;
}

int16_t Spawn_BloodD(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    if (g_TRVersion == 3) {
        if (Room_Get(room_num)->flags.underwater) {
            FX_Water_TriggerUnderwaterBloodD(
                (XYZ_32) { x, y + 64, z }, Random_GetDraw() & 7);
        } else {
            Sparks_TriggerBloodD(
                (XYZ_32) { x, y, z }, y_rot >> 4, (Random_GetDraw() & 7) + 6);
        }
        return NO_EFFECT;
    }
    return Spawn_Blood(x, y, z, speed, y_rot, room_num);
}

void Spawn_BloodBath(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (g_TRVersion == 3) {
            Spawn_Blood(
                x - (Random_GetControl() << 9) / 0x8000 + 256,
                y - (Random_GetControl() << 9) / 0x8000 + 256,
                z - (Random_GetControl() << 9) / 0x8000 + 256, speed, y_rot,
                room_num);
        } else {
            Spawn_Blood(
                x - (Random_GetDraw() << 9) / 0x8000 + 256,
                y - (Random_GetDraw() << 9) / 0x8000 + 256,
                z - (Random_GetDraw() << 9) / 0x8000 + 256, speed, y_rot,
                room_num);
        }
    }
}

void Spawn_BloodBathD(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        Spawn_BloodD(
            x - (Random_GetDraw() << 9) / 0x8000 + 256, y,
            z - (Random_GetDraw() << 9) / 0x8000 + 256, speed, y_rot, room_num);
    }
}

int16_t Spawn_GunShot(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num == NO_EFFECT) {
        return effect_num;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos.x = x;
    effect->pos.y = y;
    effect->pos.z = z;
    effect->room_num = room_num;
    effect->rot.z = 0;
    effect->rot.x = 0;
    effect->rot.y = y_rot;
    effect->counter = 3;
    effect->frame_num = 0;
    effect->object_id = O_GUN_FLASH;
    effect->shade = SHADE_NEUTRAL;
    return effect_num;
}

int16_t Spawn_GunHit(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    const ITEM *const lara_item = Lara_GetItem();
    XYZ_32 vec = {
        .x = -((Random_GetDraw() - 0x4000) << 7) / 0x7FFF,
        .y = -((Random_GetDraw() - 0x4000) << 7) / 0x7FFF,
        .z = -((Random_GetDraw() - 0x4000) << 7) / 0x7FFF,
    };
    Collide_GetJointAbsPosition(
        lara_item, &vec, Random_GetControl() * LM_NUMBER_OF / 0x7FFF);
    Spawn_Blood(
        vec.x, vec.y, vec.z, lara_item->speed, lara_item->rot.y,
        lara_item->room_num);
    Sound_Effect(SFX_LARA_BULLETHIT, &lara_item->pos, SPM_NORMAL);
    return Spawn_GunShot(x, y, z, speed, y_rot, room_num);
}

int16_t Spawn_GunMiss(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    const ITEM *const lara_item = Lara_GetItem();
    const GAME_VECTOR pos = {
        .x = lara_item->pos.x + ((Random_GetDraw() - 0x4000) << 9) / 0x7FFF,
        .y = lara_item->floor,
        .z = lara_item->pos.z + ((Random_GetDraw() - 0x4000) << 9) / 0x7FFF,
        .room_num = lara_item->room_num,
    };
    Spawn_Ricochet(pos);
    return Spawn_GunShot(x, y, z, speed, y_rot, room_num);
}

void Spawn_GunShell(const LARA_GUN_TYPE weapon_type, const bool right)
{
    if (g_TRVersion < 3) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    XYZ_32 offset = right ? g_Weapons[weapon_type].shell_pos
                          : g_Weapons[weapon_type].shell_pos_alt;
    if (offset.x == 0 && offset.y == 0 && offset.z == 0) {
        return;
    }

    Lara_GetMeshPos(right ? LM_HAND_R : LM_HAND_L, &offset);

    const int16_t effect_num = Effect_Create(lara_item->room_num);
    if (effect_num == NO_EFFECT) {
        return;
    }

    const bool shotgun = weapon_type == LGT_SHOTGUN;
    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos = offset;
    effect->room_num = lara_item->room_num;
    effect->rot.x = 0;
    effect->rot.y = 0;
    effect->rot.z = (int16_t)Random_GetControl();
    effect->speed = (int16_t)((Random_GetControl() & 0x1F) + 16);
    effect->object_id = shotgun ? O_SHOTGUN_SHELL : O_GUN_SHELL;
    effect->frame_num = Object_Get(effect->object_id)->mesh_idx;
    effect->fall_speed = (int16_t)(-48 - (Random_GetControl() & 7));
    effect->shade = 0x4210;
    effect->counter = (int16_t)((Random_GetControl() & 1) + 1);

    if (shotgun || weapon_type == LGT_M16 || weapon_type == LGT_MP5) {
        const int32_t spread = (Random_GetControl() & 0xFFF);
        effect->flag1 = lara->left_arm.rot.y + lara_item->rot.y - spread
            + lara->torso_rot.y + (shotgun ? 0x2800 : 0x4800);
        if (!shotgun && effect->speed < 24) {
            effect->speed += 24;
        }
    } else if (right) {
        effect->flag1 = lara_item->rot.y - (Random_GetControl() & 0xFFF)
            + lara->left_arm.rot.y + 0x4800;
    } else {
        effect->flag1 = lara_item->rot.y + (Random_GetControl() & 0xFFF)
            + lara->left_arm.rot.y - 0x4800;
    }
}

int16_t Spawn_AtlanteanShard(
    int32_t x, int32_t y, int32_t z, int16_t speed, int16_t y_rot,
    int16_t room_num)
{
    int16_t effect_num = Effect_Create(room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *effect = Effect_Get(effect_num);
        effect->room_num = room_num;
        effect->pos.x = x;
        effect->pos.y = y;
        effect->pos.z = z;
        effect->rot.x = 0;
        effect->rot.y = y_rot;
        effect->rot.z = 0;
        effect->object_id = O_MISSILE_ATLANTEAN_SHARD;
        effect->frame_num = 0;
        effect->speed = 250;
        effect->shade = 3584;
        M_ShootAtLara(effect);
    }
    return effect_num;
}

int16_t Spawn_AtlanteanBomb(
    int32_t x, int32_t y, int32_t z, int16_t speed, int16_t y_rot,
    int16_t room_num)
{
    int16_t effect_num = Effect_Create(room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *effect = Effect_Get(effect_num);
        effect->room_num = room_num;
        effect->pos.x = x;
        effect->pos.y = y;
        effect->pos.z = z;
        effect->rot.x = 0;
        effect->rot.y = y_rot;
        effect->rot.z = 0;
        effect->object_id = O_MISSILE_ATLANTEAN_BOMB;
        effect->frame_num = 0;
        effect->speed = 220;
        effect->shade = SHADE_NEUTRAL;
        M_ShootAtLara(effect);
    }
    return effect_num;
}

int16_t Spawn_FireStream(
    const int32_t x, const int32_t y, const int32_t z, int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num == NO_EFFECT) {
        return effect_num;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos.x = x;
    effect->pos.y = y;
    effect->pos.z = z;
    effect->rot.x = 0;
    effect->rot.y = y_rot;
    effect->rot.z = 0;
    effect->room_num = room_num;
    effect->speed = 200;
    effect->frame_num =
        ((Object_Get(O_MISSILE_FLAME)->mesh_count + 1) * Random_GetDraw())
        >> 15;
    effect->object_id = O_MISSILE_FLAME;
    effect->shade = 14 * 256;

    M_ShootAtLara(effect);

    if (Object_Get(O_DRAGON_FRONT)->loaded) {
        effect->counter = 0x4000;
    } else {
        effect->counter = 20;
    }

    return effect_num;
}

void Spawn_MysticLight(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);

    const int16_t effect_num = Effect_Create(item->room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_TWINKLE;

        effect->rot.y = 2 * Random_GetDraw();
        effect->pos = XYZ_32_OffsetYaw(item->pos, effect->rot.y, 5 * WALL_L);
        effect->pos.y += (Random_GetDraw() >> 2) - WALL_L;
        effect->room_num = item->room_num;
        effect->counter = item_num;
        effect->frame_num = 0;
    }

    // clang-format off
    Output_AddDynamicLight(
        item->pos,
        ((4 * Random_GetDraw()) >> 15) + 12,
        ((4 * Random_GetDraw()) >> 15) + 10);
    // clang-format on
}

int16_t Spawn_Knife(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num == NO_EFFECT) {
        return effect_num;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos.x = x;
    effect->pos.y = y;
    effect->pos.z = z;
    effect->room_num = room_num;
    effect->rot.x = 0;
    effect->rot.y = y_rot;
    effect->rot.z = 0;
    effect->speed = 150;
    effect->frame_num = 0;
    effect->object_id = O_MISSILE_KNIFE;
    effect->shade = 3584;
    M_ShootAtLara(effect);
    return effect_num;
}

int16_t Spawn_Harpoon(
    const int32_t x, const int32_t y, const int32_t z, const int16_t speed,
    const int16_t y_rot, const int16_t room_num)
{
    const int16_t effect_num = Effect_Create(room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->pos.x = x;
        effect->pos.y = y;
        effect->pos.z = z;
        effect->room_num = room_num;
        effect->rot.x = 0;
        effect->rot.y = y_rot;
        effect->rot.z = 0;
        effect->speed = 150;
        effect->fall_speed = 0;
        effect->frame_num = 0;
        effect->object_id = O_MISSILE_HARPOON;
        effect->shade = 3584;
        M_ShootAtLara(effect);
    }
    return effect_num;
}

#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

#define M_SHARD_DAMAGE 30
#define M_EXPLOSION_DAMAGE 100
#define M_EXPLOSION_RANGE_BASE WALL_L
#define M_EXPLOSION_RANGE SQUARE(M_EXPLOSION_RANGE_BASE) // = 1048576

static void M_Move(EFFECT *const effect)
{
    effect->pos.y += (effect->speed * Math_Sin(-effect->rot.x)) >> W2V_SHIFT;
    const int32_t speed =
        (effect->speed * Math_Cos(effect->rot.x)) >> W2V_SHIFT;
    effect->pos.z += (speed * Math_Cos(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.x += (speed * Math_Sin(effect->rot.y)) >> W2V_SHIFT;
}

static bool M_HitFloorOrCeiling(EFFECT *const effect)
{
    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(effect->pos, &room_num);
    const int32_t height = Room_GetHeight(sector, effect->pos);
    const int32_t ceiling = Room_GetCeiling(sector, effect->pos);
    if (effect->pos.y >= height || effect->pos.y <= ceiling) {
        return true;
    }
    if (room_num != effect->room_num) {
        Effect_UpdateRoom(Effect_GetIndex(effect), room_num);
    }
    return false;
}

static void M_ConvertToRicochet(
    EFFECT *const effect, const SAMPLE_TRX_ID sample_id)
{
    effect->object_id = O_RICOCHET;
    effect->frame_num = -Random_GetControl() / 11000;
    effect->speed = 0;
    effect->counter = 6;
    Sound_Effect(sample_id, &effect->pos, SPM_NORMAL);
}

static void M_ConvertToBlood(
    EFFECT *const effect, const SAMPLE_TRX_ID sample_id)
{
    ITEM *const lara_item = Lara_GetItem();
    if (g_TRVersion >= 3) {
        Spawn_Blood(
            effect->pos.x, effect->pos.y, effect->pos.z, lara_item->speed,
            lara_item->rot.y, lara_item->room_num);
        Effect_Kill(Effect_GetIndex(effect));
    } else {
        effect->object_id = O_BLOOD_1;
        effect->speed = lara_item->speed;
        effect->rot.y = lara_item->rot.y;
        effect->frame_num = 0;
        effect->counter = 0;
    }
    Sound_Effect(sample_id, &effect->pos, SPM_NORMAL);
}

static void M_ConvertToExplosion(EFFECT *const effect)
{
    effect->object_id = O_EXPLOSION_1;
    effect->frame_num = 0;
    effect->speed = 0;
    effect->counter = 0;
    Sound_Effect(SFX_EXPLOSION_1, &effect->pos, SPM_NORMAL);
}

static void M_BlastDamage(EFFECT *const effect)
{
    ITEM *const lara_item = Lara_GetItem();
    const int32_t x = effect->pos.x - lara_item->pos.x;
    const int32_t y = effect->pos.y - lara_item->pos.y;
    const int32_t z = effect->pos.z - lara_item->pos.z;
    if (Item_Test3DRange(x, y, z, M_EXPLOSION_RANGE_BASE)) {
        const int32_t range = SQUARE(x) + SQUARE(y) + SQUARE(z);
        Lara_TakeDamage(
            M_EXPLOSION_DAMAGE * (M_EXPLOSION_RANGE - range)
                / M_EXPLOSION_RANGE,
            true);
    }
}

static void M_ControlAtlanteanShard(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);

    M_Move(effect);

    if (M_HitFloorOrCeiling(effect)) {
        M_ConvertToRicochet(effect, SFX_LARA_RICOCHET);
        return;
    }

    if (!Lara_IsNearItem(&effect->pos, 200)) {
        return;
    }

    Lara_TakeDamage(M_SHARD_DAMAGE, true);
    M_ConvertToBlood(effect, SFX_LARA_BULLETHIT);
}

static void M_ControlAtlanteanBomb(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);

    M_Move(effect);

    if (M_HitFloorOrCeiling(effect)) {
        M_ConvertToExplosion(effect);
        M_BlastDamage(effect);
        return;
    }

    if (!Lara_IsNearItem(&effect->pos, 200)) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    M_ConvertToExplosion(effect);
    effect->rot.y = lara_item->rot.y;
    effect->speed = lara_item->speed;
    Lara_TakeDamage(M_EXPLOSION_DAMAGE, true);
    if (lara_item->hit_points > 0) {
        Sound_Effect(SFX_LARA_INJURY, &lara_item->pos, SPM_NORMAL);
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->hit_effect = effect;
        lara->hit_effect_count = 5;
    }
}

static void M_ControlPoison(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);

    M_Move(effect);
    if (M_HitFloorOrCeiling(effect)) {
        Effect_Kill(effect_num);
        return;
    }

    if (Lara_IsNearItem(&effect->pos, 350)) {
        LARA_INFO *const lara_info = Lara_GetLaraInfo();
        lara_info->poison_timer += 4;
    }

    if (effect->counter == 0) {
        Effect_Kill(effect_num);
    }
    effect->counter--;
}

static void M_ControlFlame(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);

    M_Move(effect);
    if (M_HitFloorOrCeiling(effect)) {
        if (g_TRVersion == 3) {
            Sparks_TriggerFlamethrowerHitFlame(effect->pos);
            const RGB_888 color = { 255, 192, Random_GetControl() & 0x3F };
            Output_AddDynamicLightRGB(effect->pos, 24, color);
        } else {
            Output_AddDynamicLight(effect->pos, 14, 11);
        }
        Effect_Kill(effect_num);
        return;
    }

    if (g_TRVersion == 3 && Room_Get(effect->room_num)->flags.underwater) {
        if (Random_GetControl() & 1) {
            Sparks_TriggerFlamethrowerSmoke(effect->pos, true);
        }
        Effect_Kill(effect_num);
        return;
    }

    if (Lara_IsNearItem(&effect->pos, 350)) {
        Lara_TakeDamage(3, true);
        Lara_CatchFire();
        return;
    }

    if (effect->counter == 0) {
        if (g_TRVersion < 3) {
            Output_AddDynamicLight(effect->pos, 14, 11);
            Sound_Effect(SFX_DRAGON_FIRE, &effect->pos, SPM_NORMAL);
        }
        Effect_Kill(effect_num);
    }
    effect->counter--;
}

static void M_ControlHarpoon(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    M_Move(effect);
    if (M_HitFloorOrCeiling(effect)) {
        M_ConvertToRicochet(effect, SFX_PROJECTILE_HIT);
        return;
    }

    if (!Room_Get(effect->room_num)->flags.underwater) {
        if (effect->rot.x > -0x3000) {
            effect->rot.x -= DEG_1;
        }
    }

    if (Lara_IsNearItem(&effect->pos, 200)) {
        Lara_TakeDamage(50, true);
        M_ConvertToBlood(effect, SFX_CRUNCH_1);
    }

    if (Room_Get(effect->room_num)->flags.underwater) {
        if (g_TRVersion == 3) {
            const int32_t time4 = Output_GetTimeInGame() * 4;
            if ((time4 & 0xF) == 0) {
                Spawn_BubbleEx(&effect->pos, effect->room_num, 8, 8);
            }
            Sparks_TriggerRocketSmoke(effect->pos, 64, effect->room_num);
        } else {
            Spawn_Bubble(&effect->pos, effect->room_num);
        }
    }
}

static void M_ControlKnife(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    M_Move(effect);
    if (M_HitFloorOrCeiling(effect)) {
        M_ConvertToRicochet(effect, SFX_PROJECTILE_HIT);
        return;
    }

    if (Lara_IsNearItem(&effect->pos, 200)) {
        Lara_TakeDamage(50, true);
        M_ConvertToBlood(effect, SFX_CRUNCH_1);
    }

    effect->rot.z += 30 * DEG_1;
}

static void M_SetupAtlanteanBomb(OBJECT *const obj)
{
    obj->control_func = M_ControlAtlanteanBomb;
    obj->save_position = true;
}

static void M_SetupAtlanteanShard(OBJECT *const obj)
{
    obj->control_func = M_ControlAtlanteanShard;
    obj->save_position = true;
}

static void M_SetupFlame(OBJECT *const obj)
{
    obj->control_func = M_ControlFlame;
    obj->save_position = true;
}

static void M_SetupPoison(OBJECT *const obj)
{
    obj->control_func = M_ControlPoison;
    obj->save_position = true;
}

static void M_SetupHarpoon(OBJECT *const obj)
{
    obj->control_func = M_ControlHarpoon;
    obj->save_position = true;
}

static void M_SetupKnife(OBJECT *const obj)
{
    obj->control_func = M_ControlKnife;
    obj->save_position = true;
}

REGISTER_OBJECT(O_MISSILE_ATLANTEAN_SHARD, M_SetupAtlanteanShard)
REGISTER_OBJECT(O_MISSILE_ATLANTEAN_BOMB, M_SetupAtlanteanBomb)
REGISTER_OBJECT(O_MISSILE_FLAME, M_SetupFlame)
REGISTER_OBJECT(O_MISSILE_POISON, M_SetupPoison)
REGISTER_OBJECT(O_MISSILE_HARPOON, M_SetupHarpoon)
REGISTER_OBJECT(O_MISSILE_KNIFE, M_SetupKnife)

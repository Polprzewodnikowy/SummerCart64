#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks/spawners.h>
#include <trx/version.h>

static void M_SpawnSplash(const GAME_VECTOR pos)
{
    const int16_t effect_num = Effect_Create(pos.room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->pos = pos.pos;
        effect->rot.y = 0;
        effect->speed = 0;
        effect->frame_num = 0;
        effect->object_id = O_SPLASH_1;
    }
}

static void M_Control_TR12(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->rot.x += 5 * DEG_1;
    effect->rot.z += 10 * DEG_1;
    effect->pos.x += (effect->speed * Math_Sin(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.z += (effect->speed * Math_Cos(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.y += effect->fall_speed;
    effect->fall_speed += GRAVITY;

    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(effect->pos, &room_num);

    const ROOM *const current_room = Room_Get(effect->room_num);
    const ROOM *const next_room = Room_Get(room_num);
    if (!current_room->flags.underwater && next_room->flags.underwater) {
        M_SpawnSplash(
            (GAME_VECTOR) { .pos = effect->pos, .room_num = effect->room_num });
    }

    const int32_t ceiling = Room_GetCeiling(sector, effect->pos);
    if (effect->pos.y < ceiling) {
        effect->pos.y = ceiling;
        effect->fall_speed = -effect->fall_speed;
    }

    const int32_t height = Room_GetHeight(sector, effect->pos);
    if (effect->pos.y >= height) {
        if (effect->counter > 0) {
            effect->speed = 0;
            effect->frame_num = 0;
            effect->counter = 0;
            effect->object_id = O_EXPLOSION_1;
            effect->shade = SHADE_NEUTRAL;
            Sound_Effect(SFX_EXPLOSION_1, &effect->pos, SPM_NORMAL);
        } else {
            Effect_Kill(effect_num);
        }
        return;
    }

    const int16_t counter_value =
        (g_TRVersion == 1) ? ABS(effect->counter) : effect->counter;
    const bool trigger_explosion =
        (g_TRVersion == 1) ? (effect->counter > 0) : (effect->counter == 0);

    if (Lara_IsNearItem(&effect->pos, counter_value * 2)) {
        Lara_TakeDamage(counter_value, true);

        if (trigger_explosion) {
            effect->speed = 0;
            effect->frame_num = 0;
            effect->counter = 0;
            effect->object_id = O_EXPLOSION_1;
            effect->shade = SHADE_NEUTRAL;
            Sound_Effect(SFX_EXPLOSION_1, &effect->pos, SPM_NORMAL);

            LARA_INFO *const lara = Lara_GetLaraInfo();
            lara->hit_effect_count = 5;
            lara->hit_effect = effect;
        } else {
            Effect_Kill(effect_num);
        }
    }

    if (room_num != effect->room_num) {
        Effect_UpdateRoom(effect_num, room_num);
    }
}

static void M_Control_TR3(const int16_t effect_num)
{
    int32_t lp;

    EFFECT *const effect = Effect_Get(effect_num);
    effect->rot.x += 5 * DEG_1;
    effect->rot.z += 10 * DEG_1;
    effect->fall_speed += 3;
    effect->pos.x +=
        (effect->speed * Math_Sin(effect->rot.y)) >> (W2V_SHIFT + 2);
    effect->pos.y += effect->fall_speed;
    effect->pos.z +=
        (effect->speed * Math_Cos(effect->rot.y)) >> (W2V_SHIFT + 2);

    const int32_t time4 = (int32_t)Output_GetTimeInGame() * 4;
    if (!(time4 & 0xC)) {
        if (effect->counter & 1) {
            Sparks_TriggerFireFlame(effect->pos, effect_num, 0);
        }

        if (effect->counter & 2) {
            Sparks_TriggerFireSmoke(effect->pos, -1, 0);
        }
    }

    int16_t room_num = effect->room_num;
    SECTOR *const sector = Room_GetSector(effect->pos, &room_num);
    int32_t c = Room_GetCeiling(sector, effect->pos);

    if (effect->pos.y < c) {
        effect->pos.y = c;
        effect->fall_speed = -effect->fall_speed;
    }

    int32_t h = Room_GetHeight(sector, effect->pos);

    if (effect->pos.y >= h) {
        if (effect->counter & 3) {
            for (int32_t i = 0; i < 3; i++) {
                if (effect->counter & 1) {
                    Sparks_TriggerFireFlame(
                        (XYZ_32) { effect->pos.x, h, effect->pos.z }, -1, 0);
                }
                if (effect->counter & 2) {
                    Sparks_TriggerFireSmoke(
                        (XYZ_32) { effect->pos.x, h, effect->pos.z }, -1, 0);
                }
            }
            Sound_Effect(SFX_EXPLOSION_1, &effect->pos, SPM_NORMAL);
        }

        Effect_Kill(effect_num);
        return;
    }

    if (Lara_IsNearItem(&effect->pos, effect->counter & ~3)) {
        Lara_TakeDamage(effect->counter >> 2, true);

        if (effect->counter & 3) {
            for (int32_t i = 0; i < 3; i++) {
                if (effect->counter & 1) {
                    Sparks_TriggerFireFlame(
                        (XYZ_32) { effect->pos.x, h, effect->pos.z }, -1, 0);
                }
                if (effect->counter & 2) {
                    Sparks_TriggerFireSmoke(
                        (XYZ_32) { effect->pos.x, h, effect->pos.z }, -1, 0);
                }
            }

            Sound_Effect(SFX_EXPLOSION_1, &effect->pos, SPM_NORMAL);
        }

        Effect_Kill(effect_num);
    }

    if (effect->room_num != room_num) {
        Effect_UpdateRoom(effect_num, room_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = g_TRVersion == 3 ? M_Control_TR3 : M_Control_TR12;
    obj->loaded = true;
    obj->mesh_count = 0;
}

REGISTER_OBJECT(O_BODY_PART, M_Setup)

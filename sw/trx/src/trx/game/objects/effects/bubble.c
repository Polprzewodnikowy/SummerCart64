#include <trx/core/utils.h>
#include <trx/game/effects.h>
#include <trx/game/fx/water.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/version.h>

static void M_Control_TR1TR2(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->rot.y += 9 * DEG_1;
    effect->rot.x += 13 * DEG_1;

    const XYZ_32 pos = {
        .x = effect->pos.x + ((Math_Sin(effect->rot.y) * 11) >> W2V_SHIFT),
        .y = effect->pos.y - effect->speed,
        .z = effect->pos.z + ((Math_Cos(effect->rot.x) * 8) >> W2V_SHIFT),
    };

    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    if (sector == nullptr || !Room_Get(room_num)->flags.underwater) {
        Effect_Kill(effect_num);
        return;
    }

    const int32_t ceiling = Room_GetCeiling(sector, pos);
    if (ceiling == NO_HEIGHT || pos.y <= ceiling) {
        Effect_Kill(effect_num);
        return;
    }

    if (effect->room_num != room_num) {
        Effect_UpdateRoom(effect_num, room_num);
    }
    effect->pos = pos;
}

static void M_Control_TR3(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->rot.y += 9 * DEG_1;
    effect->rot.x += 13 * DEG_1;
    effect->speed += effect->fall_speed;

    const XYZ_32 pos = {
        .x = effect->pos.x + ((3 * Math_Sin(effect->rot.y)) >> W2V_SHIFT),
        .y = effect->pos.y - ((int32_t)effect->speed >> 8),
        .z = effect->pos.z + (Math_Cos(effect->rot.x) >> W2V_SHIFT),
    };

    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    if (sector == nullptr) {
        Effect_Kill(effect_num);
        return;
    }

    const int32_t floor = Room_GetHeight(sector, pos);
    if (pos.y > floor) {
        Effect_Kill(effect_num);
        return;
    }

    if (!Room_Get(room_num)->flags.underwater) {
        const ROOM *const old_room = Room_Get(effect->room_num);
        FX_Water_SetupRipple(
            effect->pos.x, old_room->max_ceiling, effect->pos.z,
            -2 - (Random_GetControl() & 1), true);
        Effect_Kill(effect_num);
        return;
    }

    const int32_t ceiling = Room_GetCeiling(sector, pos);
    if (ceiling == NO_HEIGHT || pos.y <= ceiling) {
        Effect_Kill(effect_num);
        return;
    }

    if (effect->room_num != room_num) {
        Effect_UpdateRoom(effect_num, room_num);
    }

    effect->pos = pos;
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = g_TRVersion == 3 ? M_Control_TR3 : M_Control_TR1TR2;
    if (obj->loaded) {
        for (int32_t i = 0; i < -obj->mesh_count; i++) {
            Output_GetSpriteTexture(obj->mesh_idx + i)->flags = VERT_ABS_SPRITE;
        }
    }
}

REGISTER_OBJECT(O_BUBBLE_1, M_Setup)

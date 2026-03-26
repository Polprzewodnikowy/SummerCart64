#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

#define M_RANGE 200
#define M_DAMAGE 10

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->fall_speed += GRAVITY;
    effect->pos.z += (effect->speed * Math_Cos(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.x += (effect->speed * Math_Sin(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.y += effect->fall_speed;

    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(effect->pos, &room_num);
    const int32_t ceiling = Room_GetCeiling(sector, effect->pos);
    const int32_t height = Room_GetHeight(sector, effect->pos);

    if (effect->pos.y >= height || effect->pos.y < ceiling) {
        Effect_Kill(effect_num);
    } else if (Lara_IsNearItem(&effect->pos, M_RANGE)) {
        Lara_TakeDamage(M_DAMAGE, true);
        Effect_Kill(effect_num);
    } else if (room_num != effect->room_num) {
        Effect_UpdateRoom(effect_num, room_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->semi_transparent = g_TRVersion >= 2;
}

REGISTER_OBJECT(O_EMBER, M_Setup)

#include <trx/core/math.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

static void M_Control(const int16_t effect_num)
{
    EFFECT *effect = Effect_Get(effect_num);
    const OBJECT *const obj = Object_Get(effect->object_id);

    effect->frame_num--;
    if (effect->frame_num <= obj->mesh_count) {
        Effect_Kill(effect_num);
    }

    if (effect->frame_num == -1) {
        return;
    }

    const XYZ_32 pos =
        XYZ_32_OffsetYaw(effect->pos, effect->rot.y, effect->speed);
    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);

    if (pos.y >= Room_GetHeight(sector, pos)
        || pos.y <= Room_GetCeiling(sector, pos)) {
        return;
    }

    const int16_t new_effect_num = Effect_Create(room_num);
    if (new_effect_num != NO_EFFECT) {
        EFFECT *const new_effect = Effect_Get(new_effect_num);
        new_effect->pos = pos;
        new_effect->rot.y = effect->rot.y;
        new_effect->room_num = room_num;
        new_effect->speed = effect->speed;
        new_effect->frame_num = 0;
        new_effect->object_id = O_NATLA_GUN;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
}

REGISTER_OBJECT(O_NATLA_GUN, M_Setup)

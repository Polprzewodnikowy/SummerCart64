#include <trx/core/math.h>
#include <trx/game/const.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    const OBJECT *const obj = Object_Get(effect->object_id);

    effect->frame_num--;
    if (effect->frame_num <= obj->mesh_count) {
        Effect_Kill(effect_num);
        return;
    }

    effect->pos.z += (effect->speed * Math_Cos(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.x += (effect->speed * Math_Sin(effect->rot.y)) >> W2V_SHIFT;
    if (effect->fall_speed != 0) {
        effect->pos.y += effect->fall_speed;
        effect->fall_speed += GRAVITY;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
}

REGISTER_OBJECT(O_SNOW_SPRITE, M_Setup)

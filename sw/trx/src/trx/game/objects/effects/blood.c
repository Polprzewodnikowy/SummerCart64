#include <trx/core/math.h>
#include <trx/game/const.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/version.h>

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    const OBJECT *const obj = Object_Get(effect->object_id);
    effect->pos.x += (effect->speed * Math_Sin(effect->rot.y)) >> W2V_SHIFT;
    effect->pos.z += (effect->speed * Math_Cos(effect->rot.y)) >> W2V_SHIFT;
    effect->counter++;
    if (effect->counter == 4) {
        effect->frame_num--;
        effect->counter = 0;
        if (effect->frame_num <= obj->mesh_count) {
            Effect_Kill(effect_num);
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->semi_transparent = g_TRVersion >= 2;
}

REGISTER_OBJECT(O_BLOOD_1, M_Setup)

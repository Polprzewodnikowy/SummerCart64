#include <trx/game/effects.h>
#include <trx/game/objects/common.h>

static void M_Control(int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->counter++;
    if (effect->counter == 1) {
        effect->counter = 0;
        effect->frame_num--;
        if (effect->frame_num <= Object_Get(effect->object_id)->mesh_count) {
            Effect_Kill(effect_num);
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
}

REGISTER_OBJECT(O_PICKUP_AID, M_Setup)

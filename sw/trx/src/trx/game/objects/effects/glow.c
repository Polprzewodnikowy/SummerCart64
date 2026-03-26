#include <trx/game/effects.h>
#include <trx/game/objects.h>

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);

    effect->counter--;
    if (effect->counter == 0) {
        Effect_Kill(effect_num);
        return;
    }

    effect->shade += effect->speed;
    effect->frame_num += effect->fall_speed;
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
}

REGISTER_OBJECT(O_GLOW, M_Setup)

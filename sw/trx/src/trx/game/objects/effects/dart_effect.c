#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/version.h>

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    const OBJECT *const obj = Object_Get(effect->object_id);

    effect->counter++;
    if (effect->counter >= 3) {
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
    obj->draw_func = Object_DrawSpriteItem;
    obj->semi_transparent = g_TRVersion >= 2;
}

REGISTER_OBJECT(O_DART_EFFECT, M_Setup)

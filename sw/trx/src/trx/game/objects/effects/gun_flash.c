#include <trx/config.h>
#include <trx/core/colors.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/version.h>

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);

    effect->counter--;
    if (effect->counter == 0) {
        Effect_Kill(effect_num);
        return;
    }

    effect->rot.z = Random_GetControl();
    if (g_Config.visuals.enable_gun_lighting) {
        if (g_TRVersion >= 3) {
            Output_AddDynamicLightRGB(
                effect->pos, 12, (RGB_888) { 192, 144, 0 });
        } else {
            Output_AddDynamicLight(effect->pos, 12, 11);
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
}

REGISTER_OBJECT(O_GUN_FLASH, M_Setup)

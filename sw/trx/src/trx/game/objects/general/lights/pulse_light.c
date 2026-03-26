#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>

typedef struct {
    int32_t cycle;
} M_PRIV;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    p->cycle += 728;

    int32_t falloff = ABS((32 * Math_Sin(p->cycle)) >> W2V_SHIFT);
    if (falloff > 31) {
        falloff = 31;
    } else if (falloff < 8) {
        falloff = 8;
        p->cycle += 2048;
    }

    Output_AddDynamicLightRGB(item->pos, falloff, (RGB_888) { 255, 96, 0 });
}

static void M_Setup(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_PULSE_LIGHT, M_Setup)

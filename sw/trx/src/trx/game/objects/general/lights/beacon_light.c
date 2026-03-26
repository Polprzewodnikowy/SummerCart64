#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>

typedef struct {
    int32_t timer;
} M_PRIV;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (!Item_IsTriggerActive(item)) {
        return;
    }

    p->timer = (p->timer + 1) & 0x3F;
    if (p->timer < 3) {
        const uint8_t rg = 255 - (Random_GetControl() & 3);
        const uint8_t b = 255 - (Random_GetControl() & 0x1F);
        Output_AddDynamicLightRGB(item->pos, 16, (RGB_888) { rg, rg, b });
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_BEACON_LIGHT, M_Setup)

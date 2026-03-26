#include <trx/game/objects.h>
#include <trx/game/output.h>

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        return;
    }
    Output_AddDynamicLightRGB(item->pos, 16, COLOR_RGB_888_WHITE);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_ON_OFF_LIGHT, M_Setup)

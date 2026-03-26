#include <trx/game/objects.h>

static void M_Control(const int16_t item_num)
{
    Item_Animate(Item_Get(item_num));
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_anim = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_SCION_HOLDER, M_Setup)

#include <trx/game/effects.h>
#include <trx/game/objects.h>

static void M_Control(const int16_t item_num)
{
    Item_KillAllActive();
    Effect_KillAllActive();
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_KILL_ALL_TRIGGERED, M_Setup)

#include <trx/game/objects.h>

static void M_Setup(OBJECT *const obj)
{
    obj->draw_func = nullptr;
}

REGISTER_OBJECT(O_DUMMY, M_Setup)

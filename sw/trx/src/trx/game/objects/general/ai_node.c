#include <trx/game/objects.h>

static void M_Setup(OBJECT *const obj)
{
    obj->draw_func = nullptr;
    obj->hit_points = 0;
}

REGISTER_OBJECT(O_AI_AMBUSH, M_Setup)
REGISTER_OBJECT(O_AI_GUARD, M_Setup)
REGISTER_OBJECT(O_AI_FOLLOW, M_Setup)
REGISTER_OBJECT(O_AI_PATROL_1, M_Setup)
REGISTER_OBJECT(O_AI_PATROL_2, M_Setup)
REGISTER_OBJECT(O_AI_MODIFY, M_Setup)
REGISTER_OBJECT(O_AI_X1, M_Setup)
REGISTER_OBJECT(O_AI_X2, M_Setup)
REGISTER_OBJECT(O_AI_X3, M_Setup)

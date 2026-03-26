#include <trx/game/objects.h>
#include <trx/game/objects/traps/propeller.h>

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = Propeller_Control;
    obj->collision_func = Object_Collision;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_POWER_SAW, M_Setup)

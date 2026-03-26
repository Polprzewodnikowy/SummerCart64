#include <trx/game/objects.h>
#include <trx/game/rooms.h>

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (!Item_IsTriggerActive(item)) {
        return;
    }

    Item_Animate(item);

    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_ANIMATING_1, M_Setup)
REGISTER_OBJECT(O_ANIMATING_2, M_Setup)
REGISTER_OBJECT(O_ANIMATING_3, M_Setup)
REGISTER_OBJECT(O_ANIMATING_4, M_Setup)
REGISTER_OBJECT(O_ANIMATING_5, M_Setup)
REGISTER_OBJECT(O_ANIMATING_6, M_Setup)
REGISTER_OBJECT(O_ANIMATING_7, M_Setup)
REGISTER_OBJECT(O_ANIMATING_8, M_Setup)
REGISTER_OBJECT(O_ANIMATING_9, M_Setup)
REGISTER_OBJECT(O_ANIMATING_10, M_Setup)

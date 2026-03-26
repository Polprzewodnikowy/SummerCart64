#include <trx/game/objects.h>
#include <trx/game/rooms.h>

typedef enum {
    MOVING_BAR_STATE_INACTIVE = 0,
    MOVING_BAR_STATE_ACTIVE = 1,
} MOVING_BAR_STATE;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (Item_IsTriggerActive(item)) {
        item->goal_anim_state = MOVING_BAR_STATE_ACTIVE;
    } else {
        item->goal_anim_state = MOVING_BAR_STATE_INACTIVE;
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
    obj->save_position = true;
}

REGISTER_OBJECT(O_MOVING_BAR, M_Setup)

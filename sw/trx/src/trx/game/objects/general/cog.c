#include <trx/game/objects.h>
#include <trx/game/rooms.h>

typedef enum {
    COG_STATE_INACTIVE = 0,
    COG_STATE_ACTIVE = 1,
} COG_STATE;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (Item_IsTriggerActive(item)) {
        item->goal_anim_state = COG_STATE_ACTIVE;
    } else {
        item->goal_anim_state = COG_STATE_INACTIVE;
    }

    Item_Animate(item);

    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_COG_1, M_Setup)
REGISTER_OBJECT(O_COG_2, M_Setup)
REGISTER_OBJECT(O_COG_3, M_Setup)

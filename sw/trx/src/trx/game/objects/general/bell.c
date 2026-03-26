#include <trx/game/objects.h>
#include <trx/game/rooms.h>

typedef enum {
    BELL_STATE_STOP = 0,
    BELL_STATE_SWING = 1,
} BELL_STATE;

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    return false;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    item->goal_anim_state = BELL_STATE_SWING;

    const SECTOR *const sector = Room_GetSector(item->pos, &item->room_num);
    item->floor = Room_GetHeight(sector, item->pos);
    Room_TestTriggers(item);

    Item_Animate(item);

    if (item->current_anim_state == BELL_STATE_STOP) {
        item->status = IS_INACTIVE;
        Item_RemoveActive(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;

    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BELL, M_Setup)

#include <trx/game/objects.h>
#include <trx/game/rooms.h>

typedef enum {
    CABIN_STATE_START = 0,
    CABIN_STATE_DROP_1 = 1,
    CABIN_STATE_DROP_2 = 2,
    CABIN_STATE_DROP_3 = 3,
    CABIN_STATE_FINISH = 4,
} CABIN_STATE;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if ((item->flags & IF_CODE_BITS) == IF_CODE_BITS) {
        switch (item->current_anim_state) {
        case CABIN_STATE_START:
            item->goal_anim_state = CABIN_STATE_DROP_1;
            break;
        case CABIN_STATE_DROP_1:
            item->goal_anim_state = CABIN_STATE_DROP_2;
            break;
        case CABIN_STATE_DROP_2:
            item->goal_anim_state = CABIN_STATE_DROP_3;
            break;
        }
        item->flags = 0;
    }

    if (item->current_anim_state == CABIN_STATE_FINISH) {
        Room_SetFlipSlotFlags(3, IF_CODE_BITS);
        Room_FlipMap();
        Item_Kill(item_num);
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = Object_DrawUnclippedItem;
    obj->collision_func = Object_Collision;
    obj->save_anim = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_PORTACABIN, M_Setup)

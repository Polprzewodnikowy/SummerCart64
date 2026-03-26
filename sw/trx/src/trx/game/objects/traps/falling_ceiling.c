#include <trx/game/lara.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/rooms.h>

#define M_DAMAGE 300

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->current_anim_state == TRAP_SET) {
        item->goal_anim_state = TRAP_ACTIVATE;
        item->gravity = true;
    } else if (
        item->current_anim_state == TRAP_ACTIVATE && item->touch_bits != 0) {
        Lara_TakeDamage(M_DAMAGE, true);
    }

    Item_Animate(item);
    if (item->status == IS_DEACTIVATED) {
        if (!Item_IsTriggerActive(item)) {
            Trap_Reset(item);
        }
        return;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);

    item->floor = Room_GetHeight(sector, item->pos);
    Item_UpdateRoom(item_num, room_num);

    if (item->current_anim_state == TRAP_ACTIVATE
        && item->pos.y >= item->floor) {
        item->pos.y = item->floor;
        item->goal_anim_state = TRAP_WORKING;
        item->fall_speed = 0;
        item->gravity = false;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = Trap_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision_Trap;
    obj->save_position = true;
    obj->save_anim = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_FALLING_CEILING_1, M_Setup)
REGISTER_OBJECT(O_FALLING_CEILING_2, M_Setup)

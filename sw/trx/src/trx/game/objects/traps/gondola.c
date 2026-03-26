#include <trx/game/objects/traps/gondola.h>

#include <trx/game/objects.h>
#include <trx/game/rooms.h>

#define M_SINK_SPEED 50
#define M_SINK_ROOM_SHIFT (STEP_L * 3 / 2)

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    switch (item->current_anim_state) {
    case GONDOLA_STATE_FLOATING:
        if (item->goal_anim_state == GONDOLA_STATE_CRASH) {
            item->mesh_bits = 0xFF;
            Item_Explode(item_num, 240, 0);
        }
        break;

    case GONDOLA_STATE_SINK: {
        item->pos.y = item->pos.y + M_SINK_SPEED;
        const ANIM_FRAME *const frame = Item_GetBestFrame(item);
        const int16_t room_shift = frame->bounds.min.y + M_SINK_ROOM_SHIFT;
        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(
            (XYZ_32) { item->pos.x, item->pos.y + room_shift, item->pos.z },
            &room_num);
        item->floor = Room_GetHeight(sector, item->pos);
        Item_UpdateRoom(item_num, room_num);

        if (item->pos.y >= item->floor) {
            item->goal_anim_state = GONDOLA_STATE_LAND;
            item->pos.y = item->floor;
        }
        break;
    }
    }

    Item_Animate(item);

    if (item->status == IS_DEACTIVATED) {
        Item_RemoveActive(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_flags = true;
    obj->save_anim = true;
    obj->save_position = true;
}

REGISTER_OBJECT(O_GONDOLA, M_Setup)

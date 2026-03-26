#include <trx/core/utils.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const ITEM *const lara_item = Lara_GetItem();

    item->pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, 100);

    XYZ_32 pos = lara_item->pos;
    pos.x += ((item->pos.x - lara_item->pos.x) >> 2);
    pos.y += ((item->pos.y - lara_item->pos.y) >> 2);
    pos.z += ((item->pos.z - lara_item->pos.z) >> 2);
    Sound_Effect(SFX_HELICOPTER_LOOP, &pos, SPM_NORMAL);

    if (ABS(item->pos.z - lara_item->pos.z) > WALL_L * 30) {
        Item_Kill(item_num);
    }

    Item_Animate(item);

    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_position = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_MINI_COPTER, M_Setup)

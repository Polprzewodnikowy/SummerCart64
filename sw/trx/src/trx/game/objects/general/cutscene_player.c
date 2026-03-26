#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/creature.h>
#include <trx/game/cutscene.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

static void M_Initialise(const int16_t item_num)
{
    Item_AddActive(item_num);
    ITEM *const item = Item_Get(item_num);
    item->rot.y = 0;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->status = IS_ACTIVE;
    CAMERA_INFO *const camera = Cutscene_GetCamera();
    item->rot.y = camera->target_angle;
    item->pos = camera->pos.pos;

    XYZ_32 pos = {};
    Collide_GetJointAbsPosition(item, &pos, 0);

    const int16_t room_num = Room_GetIndexFromPos(pos);
    if (room_num != NO_ROOM) {
        Item_UpdateRoom(item_num, room_num);
    }

    int16_t floor_room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &floor_room_num);
    const int32_t height = Room_GetHeight(sector, pos);
    item->floor = height == NO_HEIGHT ? pos.y : height;

    if (item->dynamic_light && item->status != IS_INVISIBLE) {
        pos.x = 0;
        pos.y = 0;
        pos.z = 0;
        Collide_GetJointAbsPosition(item, &pos, 0);
        Output_AddDynamicLight(pos, 12, 11);
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->shadow_size = (UNIT_SHADOW * 10) / 16;
    obj->control_func = M_Control;
    obj->hit_points = 1;
}

REGISTER_OBJECT(O_PLAYER_1, M_Setup)
REGISTER_OBJECT(O_PLAYER_2, M_Setup)
REGISTER_OBJECT(O_PLAYER_3, M_Setup)
REGISTER_OBJECT(O_PLAYER_4, M_Setup)
REGISTER_OBJECT(O_PLAYER_5, M_Setup)
REGISTER_OBJECT(O_PLAYER_6, M_Setup)
REGISTER_OBJECT(O_PLAYER_7, M_Setup)
REGISTER_OBJECT(O_PLAYER_8, M_Setup)
REGISTER_OBJECT(O_PLAYER_9, M_Setup)
REGISTER_OBJECT(O_PLAYER_10, M_Setup)

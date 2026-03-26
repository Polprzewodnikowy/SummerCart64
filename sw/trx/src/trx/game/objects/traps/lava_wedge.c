#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/rooms.h>

#define M_SPEED 25

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (!Item_IsTriggerActive(item)) {
        Trap_Reset(item);
        return;
    }

    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    if (item->status != IS_DEACTIVATED) {
        XYZ_32 pos = item->pos;

        switch (item->rot.y) {
        case 0:
            item->pos.z += M_SPEED;
            pos.z += 2 * WALL_L;
            break;
        case -DEG_180:
            item->pos.z -= M_SPEED;
            pos.z -= 2 * WALL_L;
            break;
        case DEG_90:
            item->pos.x += M_SPEED;
            pos.x += 2 * WALL_L;
            break;
        default:
            item->pos.x -= M_SPEED;
            pos.x -= 2 * WALL_L;
            break;
        }

        const SECTOR *const sector = Room_GetSector(pos, &room_num);
        if (Room_GetHeight(sector, pos) != item->pos.y) {
            item->status = IS_DEACTIVATED;
        }
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->water_status == LWS_CHEAT) {
        item->touch_bits = 0;
    }

    if (item->touch_bits) {
        const ITEM *const lara_item = Lara_GetItem();
        if (lara_item->hit_points > 0) {
            Lara_TouchLava();
        }

        if (g_Config.debug.enable_invulnerability) {
            return;
        }
        g_Camera.item = item;
        g_Camera.flags = CF_CHASE_OBJECT;
        g_Camera.type = CAM_FIXED;
        g_Camera.target_angle = -DEG_180;
        g_Camera.target_distance = WALL_L * 3;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = Trap_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_position = true;
    obj->save_anim = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_LAVA_WEDGE, M_Setup)

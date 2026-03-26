#include <trx/core/math.h>
#include <trx/game/game_buf.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define M_DAMAGE 20
#define M_SPEED 1

static void M_Move(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const XYZ_32 pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, M_SPEED << 4);

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);

    if (Room_GetHeight(sector, pos) != pos.y) {
        item->status = IS_DEACTIVATED;
        Sound_StopEffect(SFX_SPIKE_WALL);
    } else {
        item->pos = pos;
        Item_UpdateRoom(item_num, room_num);
    }

    Sound_Effect(SFX_SPIKE_WALL, &item->pos, SPM_NORMAL);
}

static void M_HitLara(ITEM *const item)
{
    Lara_TakeDamage(M_DAMAGE, true);

    const ITEM *const lara_item = Lara_GetItem();
    Spawn_BloodBath(
        lara_item->pos.x, lara_item->pos.y - WALL_L / 2, lara_item->pos.z,
        M_SPEED, item->rot.y, lara_item->room_num, 3);
    item->touch_bits = 0;

    Sound_Effect(SFX_LARA_FLESH_WOUND, &item->pos, SPM_NORMAL);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (!Item_IsTriggerActive(item)) {
        Trap_Reset(item);
    } else if (item->status != IS_DEACTIVATED) {
        M_Move(item_num);
    }

    if (item->touch_bits) {
        M_HitLara(item);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = Trap_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_position = true;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_SPIKE_WALL, M_Setup)

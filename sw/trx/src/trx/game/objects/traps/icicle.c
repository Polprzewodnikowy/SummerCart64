#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>

#define M_DAMAGE 200

typedef enum {
    // clang-format off
    ICICLE_EMPTY = 0,
    ICICLE_BREAK = 1,
    ICICLE_FALL  = 2,
    ICICLE_LAND  = 3,
    // clang-format on
} M_STATE;

static void M_Reset(ITEM *const item)
{
    item->mesh_bits = 0xFFFFFFFF;
    Trap_Reset(item);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    switch (item->current_anim_state) {
    case ICICLE_BREAK:
        item->goal_anim_state = ICICLE_FALL;
        break;

    case ICICLE_FALL:
        if (!item->gravity) {
            item->gravity = true;
            item->fall_speed = 50;
        }
        if (item->touch_bits != 0) {
            Lara_TakeDamage(M_DAMAGE, true);
        }
        break;

    case ICICLE_LAND:
        item->gravity = false;
        break;
    }

    Item_Animate(item);
    if (item->status == IS_DEACTIVATED) {
        if (!Item_IsTriggerActive(item)) {
            M_Reset(item);
        }
        return;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    item->floor = Room_GetHeight(sector, item->pos);
    if (item->current_anim_state == ICICLE_FALL && item->pos.y >= item->floor) {
        item->pos.y = item->floor;
        item->gravity = false;
        item->goal_anim_state = ICICLE_LAND;
        item->fall_speed = 0;
        item->mesh_bits = 0b00101011;
        Sound_Effect(SFX_ICICLE, &item->pos, SPM_NORMAL);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = Trap_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision_Trap;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_ICICLE, M_Setup)

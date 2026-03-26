#include <trx/core/math.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define M_DAMAGE 100

typedef enum {
    // clang-format off
    M_STATE_NULL = 0,
    M_STATE_STOP  = 1,
    M_STATE_SPIN  = 2,
    // clang-format on
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_SPIN_FAST = 0,
    M_ANIM_SPIN_SLOW = 1,
    M_ANIM_SPIN_END  = 2,
    M_ANIM_STOP      = 3,
    // clang-format on
} M_ANIM;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    Item_SwitchToAnim(item, M_ANIM_STOP, 0);
    item->current_anim_state = M_STATE_STOP;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    bool flip = false;

    if (item->current_anim_state == M_STATE_SPIN) {
        if (item->goal_anim_state != M_STATE_STOP) {
            const XYZ_32 pos =
                XYZ_32_OffsetYaw(item->pos, item->rot.y, WALL_L * 3 / 2);

            int16_t room_num = item->room_num;
            const SECTOR *const sector = Room_GetSector(pos, &room_num);
            if (Room_GetHeight(sector, pos) == NO_HEIGHT) {
                item->goal_anim_state = M_STATE_STOP;
            }
        }

        flip = true;
        if (item->touch_bits != 0) {
            Lara_TakeDamage(M_DAMAGE, true);

            const ITEM *const lara_item = Lara_GetItem();
            Spawn_BloodBath(
                lara_item->pos.x, lara_item->pos.y - WALL_L / 2,
                lara_item->pos.z, item->speed * 2, lara_item->rot.y,
                lara_item->room_num, 2);
        }

        Sound_Effect(SFX_ROLLING_BLADE, &item->pos, SPM_NORMAL);
    } else {
        if (Item_IsTriggerActive(item)) {
            item->goal_anim_state = M_STATE_SPIN;
        }
        flip = false;
    }

    Item_Animate(item);

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const int32_t height = Room_GetHeight(sector, item->pos);
    item->floor = height;
    item->pos.y = height;
    Item_UpdateRoom(item_num, room_num);

    if (flip && item->current_anim_state == M_STATE_STOP) {
        item->rot.y += DEG_180;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_SPINNING_BLADE, M_Setup)

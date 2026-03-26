#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

typedef enum {
    // clang-format off
    SPRINGBOARD_STATE_OFF = 0,
    SPRINGBOARD_STATE_ON = 1,
    // clang-format on
} SPRINGBOARD_STATE;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    ITEM *const lara_item = Lara_GetItem();

    if (item->current_anim_state == SPRINGBOARD_STATE_OFF
        && lara_item->pos.y == item->pos.y
        && ROUND_TO_SECTOR(lara_item->pos.x) == ROUND_TO_SECTOR(item->pos.x)
        && ROUND_TO_SECTOR(lara_item->pos.z) == ROUND_TO_SECTOR(item->pos.z)) {
        if (lara_item->hit_points <= 0) {
            return;
        }

        ITEM *const vehicle = Lara_Vehicle_GetItem();
        if (vehicle != nullptr) {
            if (vehicle->object_id != O_SKIDOO_FAST
                && vehicle->object_id != O_SKIDOO_ARMED) {
                return;
            }

            vehicle->fall_speed = -200;
            vehicle->pos.y -= STEP_L;
        } else {
            if (lara_item->current_anim_state == LS(LS_WALK_BACK)
                || lara_item->current_anim_state == LS(LS_FAST_BACK)) {
                lara_item->speed = -lara_item->speed;
            }

            lara_item->fall_speed = -240;
            lara_item->gravity = true;

            Item_SwitchToAnim(lara_item, LA(LA_FALL_START), 0);
            lara_item->current_anim_state = LS(LS_JUMP_FORWARD);
            lara_item->goal_anim_state = LS(LS_JUMP_FORWARD);
        }
        item->goal_anim_state = SPRINGBOARD_STATE_ON;
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_SPRINGBOARD, M_Setup)

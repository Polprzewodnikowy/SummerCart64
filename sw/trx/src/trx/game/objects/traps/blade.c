#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/spawn.h>

// clang-format off
#define BLADE_CUT_DAMAGE 100
#define BLADE_TOUCH_BITS 0b00000010 // = 2
// clang-format on

typedef enum {
    // clang-format off
    BLADE_STATE_EMPTY = 0,
    BLADE_STATE_STOP  = 1,
    BLADE_STATE_CUT   = 2,
    // clang-format on
} BLADE_STATE;

typedef enum {
    // clang-format off
    BLADE_ANIM_RETURN   = 0,
    BLADE_ANIM_FINISHED = 1,
    BLADE_ANIM_SET      = 2,
    BLADE_ANIM_CUT      = 3,
    // clang-format on
} BLADE_ANIM;

static void M_Initialise(const int16_t item_num)
{
    const OBJECT *const obj = Object_Get(O_BLADE);
    ITEM *const item = Item_Get(item_num);
    Item_SwitchToAnim(item, BLADE_ANIM_SET, 0);
    item->current_anim_state = BLADE_STATE_STOP;
}

static void M_Stop(ITEM *const item)
{
    const int16_t anim_idx = Item_GetRelativeAnim(item);
    if (anim_idx == BLADE_ANIM_CUT) {
        const ANIM *const anim = Item_GetAnim(item);
        if (!Item_IsTriggerActive(item) && anim->jump_anim_num == item->anim_num
            && Item_TestFrameEqual(item, -1)) {
            item->status = IS_INACTIVE;
            Item_RemoveActive(Item_GetIndex(item));
            return;
        }
    }

    item->goal_anim_state = BLADE_STATE_STOP;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (Item_IsTriggerActive(item)
        && item->current_anim_state == BLADE_STATE_STOP) {
        item->goal_anim_state = BLADE_STATE_CUT;
    } else {
        M_Stop(item);
    }

    if ((item->touch_bits & BLADE_TOUCH_BITS) != 0
        && item->current_anim_state == BLADE_STATE_CUT) {
        Lara_TakeDamage(BLADE_CUT_DAMAGE, true);

        const ITEM *const lara_item = Lara_GetItem();
        Spawn_BloodBath(
            lara_item->pos.x, item->pos.y - STEP_L, lara_item->pos.z,
            lara_item->speed, lara_item->rot.y, lara_item->room_num, 2);
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision_Trap;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BLADE, M_Setup)

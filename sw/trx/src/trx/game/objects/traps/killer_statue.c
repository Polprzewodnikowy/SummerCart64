#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

#define KILLER_STATUE_CUT_DAMAGE 20
#define KILLER_STATUE_TOUCH_BITS 0b10000000 // = 128
// clang-format on

typedef enum {
    // clang-format off
    KILLER_STATUE_STATE_EMPTY = 0,
    KILLER_STATUE_STATE_STOP  = 1,
    KILLER_STATUE_STATE_CUT   = 2,
    // clang-format on
} KILLER_STATUE_STATE;

typedef enum {
    // clang-format off
    KILLER_STATUE_ANIM_RETURN   = 0,
    KILLER_STATUE_ANIM_FINISHED = 1,
    KILLER_STATUE_ANIM_CUT      = 2,
    KILLER_STATUE_ANIM_SET      = 3,
    // clang-format on
} KILLER_STATUE_ANIM;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    Item_SwitchToAnim(item, KILLER_STATUE_ANIM_SET, 0);
    item->current_anim_state = KILLER_STATUE_STATE_STOP;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (Item_IsTriggerActive(item)
        && item->current_anim_state == KILLER_STATUE_STATE_STOP) {
        item->goal_anim_state = KILLER_STATUE_STATE_CUT;
    } else {
        item->goal_anim_state = KILLER_STATUE_STATE_STOP;
    }

    if ((item->touch_bits & KILLER_STATUE_TOUCH_BITS) != 0
        && item->current_anim_state == KILLER_STATUE_STATE_CUT) {
        Lara_TakeDamage(KILLER_STATUE_CUT_DAMAGE, true);

        const ITEM *const lara_item = Lara_GetItem();
        Spawn_Blood(
            lara_item->pos.x + (Random_GetControl() - 0x4000) / 256,
            lara_item->pos.y - Random_GetControl() / 44,
            lara_item->pos.z + (Random_GetControl() - 0x4000) / 256,
            lara_item->speed,
            lara_item->rot.y + (Random_GetControl() - 0x4000) / 8,
            lara_item->room_num);
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

REGISTER_OBJECT(O_KILLER_STATUE, M_Setup)

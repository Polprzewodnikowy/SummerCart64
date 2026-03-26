#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/game_buf.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>

#define MUMMY_HITPOINTS 18

typedef enum {
    MUMMY_STATE_EMPTY = 0,
    MUMMY_STATE_STOP = 1,
    MUMMY_STATE_DEATH = 2,
} MUMMY_STATE;

static bool M_CanDropItems(const ITEM *const item)
{
    return item->hit_points <= 0 || item->status == IS_DEACTIVATED;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->touch_bits = 0;
    item->mesh_bits = 0xFFFF87FF;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    int16_t head = 0;

    if (Item_IsTriggerActive(item)) {
        if (!LOT_EnableBaddieAI(item_num, true)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    if (item->current_anim_state == MUMMY_STATE_STOP) {
        const ITEM *const lara_item = Lara_GetItem();
        head =
            Math_Atan(
                lara_item->pos.z - item->pos.z, lara_item->pos.x - item->pos.x)
            - item->rot.y;
        CLAMP(head, -FRONT_ARC, FRONT_ARC);

        if (item->hit_points <= 0 || item->touch_bits) {
            item->goal_anim_state = MUMMY_STATE_DEATH;
        }
    }

    Creature_Head(item, head);
    Item_Animate(item);

    if (item->status == IS_DEACTIVATED) {
        // Count kill if Lara touches mummy and it falls.
        if (item->hit_points > 0) {
            Stats_AddKill();
        }
        Item_RemoveActive(item_num);
        Carrier_TestItemDrops(item_num);
        item->hit_points = 0;
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->can_drop_items_func = M_CanDropItems;

    obj->hit_points = MUMMY_HITPOINTS;

    obj->save_flags = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;

    Object_GetBone(obj, 2)->rot.y = true;
}

REGISTER_OBJECT(O_MUMMY, M_Setup)

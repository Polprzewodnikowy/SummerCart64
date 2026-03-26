#include <trx/game/game_flow.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/sound.h>

#define M_LF_USE_KEYHOLE 104

static XYZ_32 m_KeyholePosition = {
    .x = 0,
    .y = 0,
    .z = WALL_L / 2 - LARA_RADIUS - 50,
};

static const OBJECT_BOUNDS m_KeyholeBounds = {
    .shift = {
        .min = { .x = -200, .y = +0, .z = +WALL_L / 2 - 200, },
        .max = { .x = +200, .y = +0, .z = +WALL_L / 2, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = -30 * DEG_1, .z = -10 * DEG_1, },
        .max = { .x = +10 * DEG_1, .y = +30 * DEG_1, .z = +10 * DEG_1, },
    },
};

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return &m_KeyholeBounds;
}

static void M_Use(ITEM *const lara_item, ITEM *const receptacle_item)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    Lara_AlignPosition(receptacle_item, &m_KeyholePosition);
    Lara_AnimateUntil(lara_item, LS(LS_USE_KEY));
    lara_item->goal_anim_state = LS(LS_STOP);
    lara->gun_status = LGS_HANDS_BUSY;
    lara->interact_target.is_moving = false;
}

static void M_ConsumeKeyItem(ITEM *const receptacle_item)
{
    const OBJECT_ID key_object_id =
        Object_FindReceptacleKey(receptacle_item->object_id);
    if (key_object_id != NO_OBJECT) {
        Inv_RemoveItem(key_object_id);
    }
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->interact_target.item_num = NO_ITEM;
}

static void M_MarkDone(ITEM *const receptacle_item)
{
    receptacle_item->status = IS_ACTIVE;
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    if (lara_item->current_anim_state != LS(LS_STOP)) {
        if (lara_item->current_anim_state == LS(LS_USE_KEY)
            && Lara_TestPosition(item, obj->bounds_func())
            && Item_TestFrameEqual(lara_item, M_LF_USE_KEYHOLE)) {
            M_ConsumeKeyItem(item);
            M_MarkDone(item);
        }
        return;
    }

    if (lara->interact_target.is_moving
        && lara->interact_target.item_num == item_num) {
        M_Use(lara_item, item);
    }

    if (!g_Input.action || lara->gun_status != LGS_ARMLESS || lara_item->gravity
        || lara_item->current_anim_state != LS(LS_STOP)) {
        return;
    }

    if (!Lara_TestPosition(item, obj->bounds_func())) {
        return;
    }

    if (item->status != IS_INACTIVE) {
        Lara_RefuseInteraction();
    } else if (!GF_ShowInventoryKeys(item->object_id)) {
        Lara_RefuseInteraction();
    }
}

static bool M_IsUsable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    return item->status == IS_INACTIVE;
}

static void M_Setup(OBJECT *const obj)
{
    obj->collision_func = M_Collision;
    obj->bounds_func = M_Bounds;
    obj->save_flags = true;
    obj->is_usable_func = M_IsUsable;
}

bool Keyhole_Trigger(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->status != IS_ACTIVE || lara->gun_status == LGS_HANDS_BUSY) {
        return false;
    }
    item->status = IS_DEACTIVATED;
    return true;
}

REGISTER_OBJECT(O_KEY_HOLE_1, M_Setup)
REGISTER_OBJECT(O_KEY_HOLE_2, M_Setup)
REGISTER_OBJECT(O_KEY_HOLE_3, M_Setup)
REGISTER_OBJECT(O_KEY_HOLE_4, M_Setup)

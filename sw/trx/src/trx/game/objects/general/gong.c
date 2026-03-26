#include <trx/game/game_flow.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/general/pickup.h>

static XYZ_32 m_Position = { .x = 0, .y = 0, .z = 0 };

static const OBJECT_BOUNDS m_Bounds = {
    .shift = {
        .min = { .x = -WALL_L / 2, .y = -100, .z = -WALL_L / 2 - 300, },
        .max = { .x = +WALL_L, .y = +100, .z = -WALL_L / 2 + 100, },
    },
    .rot = {
        .min = { .x = -30 * DEG_1, .y = 0, .z = 0, },
        .max = { .x = +30 * DEG_1, .y = 0, .z = 0, },
    },
    .ignore_rot = true,
};

static void M_ConsumeKeyItem(ITEM *const receptacle_item)
{
    const OBJECT_ID key_object_id =
        Object_FindReceptacleKey(receptacle_item->object_id);
    if (key_object_id != NO_OBJECT) {
        Inv_RemoveItem(key_object_id);
    }
}

static void M_CreateGongBonger(ITEM *const lara_item)
{
    const int16_t item_gong_bonger_num = Item_Create();
    if (item_gong_bonger_num == NO_ITEM) {
        return;
    }

    ITEM *const item_gong_bonger = Item_Get(item_gong_bonger_num);
    item_gong_bonger->object_id = O_GONG_BONGER;
    item_gong_bonger->pos.x = lara_item->pos.x;
    item_gong_bonger->pos.y = lara_item->pos.y;
    item_gong_bonger->pos.z = lara_item->pos.z;
    item_gong_bonger->rot.x = 0;
    item_gong_bonger->rot.y = lara_item->rot.y;
    lara_item->rot.z = 0;
    item_gong_bonger->room_num = lara_item->room_num;

    Item_Initialise(item_gong_bonger_num);
    Item_AddActive(item_gong_bonger_num);
    item_gong_bonger->status = IS_ACTIVE;
    item_gong_bonger->shade.value_1 = -1;
}

static void M_Use(ITEM *const lara_item, ITEM *const receptacle_item)
{
    Lara_AlignPosition(receptacle_item, &m_Position);
    lara_item->rot.y += DEG_180;

    Lara_SwitchToExtraState(LS_EXTRA_GONG_BONG);
    if (Item_TestFrameEqual(lara_item, 0)) {
        M_ConsumeKeyItem(receptacle_item);
    }

    M_CreateGongBonger(lara_item);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->interact_target.is_moving = false;
    lara->interact_target.item_num = NO_ITEM;
}

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return &m_Bounds;
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->extra_anim) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);

    if (lara->interact_target.is_moving
        && lara->interact_target.item_num == item_num) {
        M_Use(lara_item, item);
        return;
    }

    if (item->status != IS_INACTIVE || !g_Input.action
        || lara->gun_status != LGS_ARMLESS || lara_item->gravity
        || lara_item->current_anim_state != LS(LS_STOP)) {
        goto normal_collision;
    }

    const XYZ_16 old_rot = item->rot;
    item->rot.x = 0;
    item->rot.y = lara_item->rot.y;
    item->rot.z = 0;

    if (!Lara_TestPosition(item, obj->bounds_func())) {
        item->rot = old_rot;
        goto normal_collision;
    }

    item->rot = old_rot;

    if (!GF_ShowInventoryKeys(item->object_id)) {
        Lara_RefuseInteraction();
    }

    return;

normal_collision:
    Object_Collision(item_num, lara_item, coll);
}

static void M_Setup(OBJECT *const obj)
{
    obj->collision_func = M_Collision;
    obj->bounds_func = M_Bounds;
}

REGISTER_OBJECT(O_GONG, M_Setup)

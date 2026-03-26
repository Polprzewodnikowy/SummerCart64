#include <trx/game/camera.h>
#include <trx/game/game_flow.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/overlay.h>
#include <trx/game/sound.h>

#define M_RANGE_H (STEP_L * 2)
#define M_RANGE_V (STEP_L * 3)

static const OBJECT_BOUNDS m_MidasTouch_Bounds = {
    .shift = {
        .min = { .x = -700, .y = +384 - 100, .z = -700, },
        .max = { .x = +700, .y = +384 + 100 + 512, .z = +700, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = -30 * DEG_1, .z = -10 * DEG_1, },
        .max = { .x = +10 * DEG_1, .y = +30 * DEG_1, .z = +10 * DEG_1, },
    },
};

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return &m_MidasTouch_Bounds;
}

static void M_KillLara(const ITEM *const item)
{
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();

    Lara_SwitchToExtraState(LS_EXTRA_MIDAS_KILL);
    lara_item->hit_points = -1;
    lara_item->gravity = false;
    lara->gun_type = LGT_UNARMED;
    lara->air = -1;

    Camera_InvokeCinematic(lara_item, 0, 0);
}

static bool M_IsUsable(const int16_t item_num)
{
    const ITEM *const lara_item = Lara_GetItem();
    return lara_item->current_anim_state != LS(LS_USE_MIDAS);
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);

    const DIRECTION quadrant = Math_GetDirection(lara_item->rot.y);
    switch (quadrant) {
    case DIR_NORTH:
        item->rot.y = 0;
        break;
    case DIR_EAST:
        item->rot.y = DEG_90;
        break;
    case DIR_SOUTH:
        item->rot.y = -DEG_180;
        break;
    case DIR_WEST:
        item->rot.y = -DEG_90;
        break;
    default:
        break;
    }

    if (!lara_item->gravity && lara_item->current_anim_state == LS(LS_STOP)
        && lara_item->pos.x > item->pos.x - M_RANGE_H
        && lara_item->pos.x < item->pos.x + M_RANGE_H
        && lara_item->pos.y > item->pos.y - M_RANGE_V
        && lara_item->pos.y < item->pos.y + M_RANGE_V
        && lara_item->pos.z > item->pos.z - M_RANGE_H
        && lara_item->pos.z < item->pos.z + M_RANGE_H) {
        M_KillLara(item);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->interact_target.is_moving
        && lara->interact_target.item_num == item_num) {
        Lara_SwitchToExtraState(LS_EXTRA_USE_MIDAS);
        lara->interact_target.is_moving = false;
    }

    if (!g_Input.action || lara->gun_status != LGS_ARMLESS || lara_item->gravity
        || lara_item->current_anim_state != LS(LS_STOP)) {
        return;
    }

    if (!Lara_TestPosition(item, obj->bounds_func())) {
        return;
    }

    if (!GF_ShowInventoryKeys(item->object_id)) {
        Lara_RefuseInteraction();
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->collision_func = M_Collision;
    obj->draw_func = nullptr;
    obj->bounds_func = M_Bounds;
    obj->is_usable_func = M_IsUsable;
}

REGISTER_OBJECT(O_MIDAS_TOUCH, M_Setup)

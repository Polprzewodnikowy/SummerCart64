// Tomb of Qualopec and Sanctuary Scion pickup.
// Triggers O_LARA_EXTRA pedestal pickup animation.

#include <trx/game/camera.h>
#include <trx/game/game.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/level.h>
#include <trx/game/objects/common.h>
#include <trx/game/overlay.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>

static XYZ_32 m_Scion1_Position = { 0, 640, -310 };

static const OBJECT_BOUNDS m_Scion1_Bounds = {
    .shift = {
        .min = { .x = -256, .y = +640 - 100, .z = -350, },
        .max = { .x = +256, .y = +640 + 100, .z = -200, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = 0, .z = 0, },
        .max = { .x = +10 * DEG_1, .y = 0, .z = 0, },
    },
};

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return &m_Scion1_Bounds;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->status == IS_DEACTIVATED) {
            const int16_t item_num = Item_GetIndex(item);
            Item_RemoveDrawn(item_num);
        }
    }
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const OBJECT *const obj = Object_Get(item->object_id);

    const XYZ_16 old_rot = item->rot;
    item->rot.y = lara_item->rot.y;
    item->rot.x = 0;
    item->rot.z = 0;

    if (!Lara_TestPosition(item, obj->bounds_func())) {
        goto cleanup;
    }

    if (g_Input.action && lara->gun_status == LGS_ARMLESS && !lara_item->gravity
        && lara_item->current_anim_state == LS(LS_STOP)) {
        lara->interact_target.item_num = item_num;
        Lara_AlignPosition(item, &m_Scion1_Position);
        Lara_SwitchToExtraState(LS_EXTRA_SCION_PICKUP_1);
        Camera_InvokeCinematic(lara_item, 0, 0);
    }
cleanup:
    item->rot = old_rot;
}

static void M_Setup(OBJECT *const obj)
{
    obj->handle_save_func = M_HandleSave;
    obj->draw_func = Object_DrawPickupItem;
    obj->collision_func = M_Collision;
    obj->save_flags = true;
    obj->bounds_func = M_Bounds;
}

REGISTER_OBJECT(O_SCION_ITEM_1, M_Setup)

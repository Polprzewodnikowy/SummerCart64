// Atlantis Scion - triggers O_LARA_EXTRA reach anim.

#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>

#define EXTRA_ANIM_HOLDER_SCION 0

static XYZ_32 m_Scion4_Position = { 0, 280, -512 + 105 };

static const OBJECT_BOUNDS m_Scion4_Bounds = {
    .shift = {
        .min = { .x = -256, .y = +256 - 50, .z = -512 - 350, },
        .max = { .x = +256, .y = +256 + 50, .z = -200, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = 0, .z = 0, },
        .max = { .x = +10 * DEG_1, .y = 0, .z = 0, },
    },
};

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return &m_Scion4_Bounds;
}

static void M_Control(const int16_t item_num)
{
    Item_Animate(Item_Get(item_num));
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const OBJECT *const obj = Object_Get(item->object_id);

    int16_t rotx = item->rot.x;
    int16_t roty = item->rot.y;
    int16_t rotz = item->rot.z;
    item->rot.y = lara_item->rot.y;
    item->rot.x = 0;
    item->rot.z = 0;

    if (!Lara_TestPosition(item, obj->bounds_func())) {
        goto cleanup;
    }

    if (g_Input.action && lara->gun_status == LGS_ARMLESS && !lara_item->gravity
        && lara_item->current_anim_state == LS(LS_STOP)) {
        Lara_AlignPosition(item, &m_Scion4_Position);
        Lara_SwitchToExtraState(LS_EXTRA_SCION_PICKUP_2);
        Camera_InvokeCinematic(lara_item, 0, -DEG_90);
    }
cleanup:
    item->rot.x = rotx;
    item->rot.y = roty;
    item->rot.z = rotz;
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = M_Collision;
    obj->save_flags = true;
    obj->bounds_func = M_Bounds;
}

REGISTER_OBJECT(O_SCION_ITEM_4, M_Setup)

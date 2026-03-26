#include <trx/game/lara.h>

static void M_Turn180(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    item->rot.x = -item->rot.x;
    item->rot.y += DEG_180;
    if (item == Lara_GetItem()
        && item->current_anim_state != LS(LS_ROLL_CONT)) {
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->move_angle += DEG_180;
    }
}

static void M_Turn90(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    item->rot.y += DEG_90;
}

static void M_InvisibilityOn(ITEM *const item)
{
    if (item != nullptr) {
        item->status = IS_INVISIBLE;
    }
}

static void M_InvisibilityOff(ITEM *const item)
{
    if (item != nullptr) {
        item->status = IS_ACTIVE;
    }
}

static void M_ShadowOn(ITEM *const item)
{
    if (item != nullptr) {
        item->enable_shadow = true;
    }
}

static void M_ShadowOff(ITEM *const item)
{
    if (item != nullptr) {
        item->enable_shadow = false;
    }
}

static void M_DynamicLightOn(ITEM *const item)
{
    if (item != nullptr) {
        item->dynamic_light = true;
    }
}

static void M_DynamicLightOff(ITEM *const item)
{
    if (item != nullptr) {
        item->dynamic_light = false;
    }
}

static void M_SwapMeshes(ITEM *const item, const OBJECT_ID swap_id)
{
    if (item == nullptr) {
        return;
    }

    const OBJECT *const obj_1 = Object_Get(item->object_id);
    for (int32_t mesh_idx = 0; mesh_idx < obj_1->mesh_count; mesh_idx++) {
        Object_SwapMesh(item->object_id, swap_id, mesh_idx);
    }
}

static void M_SwapMeshesWithMeshSwap1(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    M_SwapMeshes(item, O_MESH_SWAP_1);
}

static void M_SwapMeshesWithMeshSwap2(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    M_SwapMeshes(item, O_MESH_SWAP_2);
}

static void M_SwapMeshesWithMeshSwap3(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    M_SwapMeshes(item, O_MESH_SWAP_3);
}

REGISTER_ITEM_ACTION(ITEM_ACTION_TURN_180, M_Turn180)
REGISTER_ITEM_ACTION(ITEM_ACTION_TURN_90, M_Turn90)
REGISTER_ITEM_ACTION(ITEM_ACTION_INVISIBILITY_ON, M_InvisibilityOn)
REGISTER_ITEM_ACTION(ITEM_ACTION_INVISIBILITY_OFF, M_InvisibilityOff)
REGISTER_ITEM_ACTION(ITEM_ACTION_SHADOW_ON, M_ShadowOn)
REGISTER_ITEM_ACTION(ITEM_ACTION_SHADOW_OFF, M_ShadowOff)
REGISTER_ITEM_ACTION(ITEM_ACTION_DYNAMIC_LIGHT_ON, M_DynamicLightOn)
REGISTER_ITEM_ACTION(ITEM_ACTION_DYNAMIC_LIGHT_OFF, M_DynamicLightOff)
REGISTER_ITEM_ACTION(
    ITEM_ACTION_SWAP_MESHES_WITH_MESH_SWAP_1, M_SwapMeshesWithMeshSwap1)
REGISTER_ITEM_ACTION(
    ITEM_ACTION_SWAP_MESHES_WITH_MESH_SWAP_2, M_SwapMeshesWithMeshSwap2)
REGISTER_ITEM_ACTION(
    ITEM_ACTION_SWAP_MESHES_WITH_MESH_SWAP_3, M_SwapMeshesWithMeshSwap3)

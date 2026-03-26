#include <trx/config.h>
#include <trx/game/const.h>
#include <trx/game/objects.h>
#include <trx/game/objects/general/bridge_common.h>

static int16_t M_GetFloorHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (g_Config.gameplay.fix_bridge_collision
        && !Bridge_IsSameSector(x, z, item)) {
        return height;
    }

    const int32_t offset_height =
        item->pos.y + (Bridge_GetOffset(item, x, y, z) / 4);
    if (y > offset_height || item->pos.y >= height) {
        return height;
    }

    if (g_Config.gameplay.fix_bridge_collision && item->pos.y >= height) {
        return height;
    }

    return offset_height;
}

static int16_t M_GetCeilingHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (g_Config.gameplay.fix_bridge_collision
        && !Bridge_IsSameSector(x, z, item)) {
        return height;
    }

    const int32_t offset_height =
        item->pos.y + (Bridge_GetOffset(item, x, y, z) / 4);
    if (y <= offset_height) {
        return height;
    }

    if (g_Config.gameplay.fix_bridge_collision && item->pos.y <= height) {
        return height;
    }

    return offset_height + STEP_L;
}

static void M_Initialise(const int16_t item_num)
{
    Bridge_FixEmbeddedPosition(item_num);
    Walkable_AllocateNodes(Item_Get(item_num), 1);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->floor_height_func = M_GetFloorHeight;
    obj->ceiling_height_func = M_GetCeilingHeight;
    obj->add_walkable_func = Bridge_AddWalkable;
}

REGISTER_OBJECT(O_BRIDGE_TILT_1, M_Setup)

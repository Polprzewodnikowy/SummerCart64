#include <trx/config.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/items.h>
#include <trx/game/objects.h>
#include <trx/game/objects/general/door.h>
#include <trx/game/objects/traps/movable_block.h>
#include <trx/game/rooms.h>

typedef enum {
    DRAWBRIDGE_STATE_CLOSED = DOOR_STATE_CLOSED,
    DRAWBRIDGE_STATE_OPEN = DOOR_STATE_OPEN,
} DRAWBRIDGE_STATE;

typedef enum {
    DRAWBRIDGE_ANIM_CLOSED = 3,
} DRAWBRIDGE_ANIM;

static bool M_IsItemOnTop(const ITEM *item, int32_t x, int32_t z)
{
    int32_t ix = item->pos.x >> WALL_SHIFT;
    int32_t iz = item->pos.z >> WALL_SHIFT;
    x >>= WALL_SHIFT;
    z >>= WALL_SHIFT;

    if (item->rot.y == 0 && x == ix && (z == iz - 1 || z == iz - 2)) {
        return true;
    } else if (
        item->rot.y == -DEG_180 && x == ix && (z == iz + 1 || z == iz + 2)) {
        return true;
    } else if (
        item->rot.y == DEG_90 && z == iz && (x == ix - 1 || x == ix - 2)) {
        return true;
    } else if (
        item->rot.y == -DEG_90 && z == iz && (x == ix + 1 || x == ix + 2)) {
        return true;
    }

    return false;
}

static int16_t M_GetFloorHeight(
    const ITEM *item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (item->current_anim_state != DRAWBRIDGE_STATE_OPEN) {
        return height;
    } else if (!M_IsItemOnTop(item, x, z)) {
        return height;
    } else if (y > item->pos.y) {
        return height;
    } else if (
        g_Config.gameplay.fix_bridge_collision && item->pos.y >= height) {
        return height;
    }
    return item->pos.y;
}

static int16_t M_GetCeilingHeight(
    const ITEM *item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (item->current_anim_state != DRAWBRIDGE_STATE_OPEN) {
        return height;
    } else if (!M_IsItemOnTop(item, x, z)) {
        return height;
    } else if (y <= item->pos.y) {
        return height;
    } else if (
        g_Config.gameplay.fix_bridge_collision && item->pos.y <= height) {
        return height;
    }
    return item->pos.y + STEP_L;
}

static BOUNDS_16 M_RotateBounds(const BOUNDS_16 bounds, int16_t rot_y)
{
    BOUNDS_16 rot_bounds = {};

    switch (rot_y) {
    case 0:
    default:
        rot_bounds = bounds;
        break;
    case DEG_90:
        rot_bounds.min.x = bounds.min.z;
        rot_bounds.max.x = bounds.max.z;
        rot_bounds.min.z = -bounds.max.x;
        rot_bounds.max.z = -bounds.min.x;
        break;
    case -DEG_180:
        rot_bounds.min.x = -bounds.max.x;
        rot_bounds.max.x = -bounds.min.x;
        rot_bounds.min.z = -bounds.max.z;
        rot_bounds.max.z = -bounds.min.z;
        break;
    case -DEG_90:
        rot_bounds.min.x = -bounds.max.z;
        rot_bounds.max.x = -bounds.min.z;
        rot_bounds.min.z = bounds.min.x;
        rot_bounds.max.z = bounds.max.x;
        break;
    }
    return rot_bounds;
}

static void M_GetSectorPositions(const ITEM *const item, VECTOR *sector_pos)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    const ANIM_FRAME *const frame =
        Object_GetAnim(obj, DRAWBRIDGE_ANIM_CLOSED)->frame_ptr;
    const BOUNDS_16 rot_bounds = M_RotateBounds(frame->bounds, item->rot.y);

    const int32_t x0 = item->pos.x + rot_bounds.min.x;
    const int32_t x1 = item->pos.x + rot_bounds.max.x - 1; // inclusive
    const int32_t z0 = item->pos.z + rot_bounds.min.z;
    const int32_t z1 = item->pos.z + rot_bounds.max.z - 1;

    const int32_t sx0 = Math_FloorDiv(x0, WALL_L);
    const int32_t sx1 = Math_FloorDiv(x1, WALL_L);
    const int32_t sz0 = Math_FloorDiv(z0, WALL_L);
    const int32_t sz1 = Math_FloorDiv(z1, WALL_L);

    // Sector of the drawbridge original position.
    const int32_t sx_orig = Math_FloorDiv(item->pos.x, WALL_L);
    const int32_t sz_orig = Math_FloorDiv(item->pos.z, WALL_L);

    for (int32_t sx = sx0; sx <= sx1; ++sx) {
        for (int32_t sz = sz0; sz <= sz1; ++sz) {
            if (sx == sx_orig && sz == sz_orig) {
                // Skip the original sector since it's WALL_L away.
                continue;
            }
            const XYZ_32 pos = {
                .x = sx * WALL_L + WALL_L / 2,
                .y = item->pos.y,
                .z = sz * WALL_L + WALL_L / 2,
            };
            Vector_Add(sector_pos, &pos);
        }
    }
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    VECTOR *const positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    Walkable_AllocateNodes(item, positions->count);
    Vector_Free(positions);
}

static void M_DropStack(const ITEM *const item)
{
    VECTOR *positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    for (int32_t i = 0; i < positions->count; i++) {
        MovableBlock_DropStack(
            *(const XYZ_32 *)Vector_Get(positions, i), item->room_num);
    }
    Vector_Free(positions);
}

static void M_Collision(int16_t item_num, ITEM *lara_item, COLL_INFO *coll)
{
    const ITEM *const item = Item_Get(item_num);
    if (item->current_anim_state == DRAWBRIDGE_STATE_CLOSED) {
        Door_Collision(item_num, lara_item, coll);
    }
}

static void M_Control(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (Item_IsTriggerActive(item)) {
        item->goal_anim_state = DRAWBRIDGE_STATE_OPEN;
    } else {
        item->goal_anim_state = DRAWBRIDGE_STATE_CLOSED;
        if (item->current_anim_state == DRAWBRIDGE_STATE_OPEN) {
            M_DropStack(item);
        }
    }

    Item_Animate(item);

    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);
}

static void M_AddWalkable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    VECTOR *positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    for (int32_t i = 0; i < positions->count; i++) {
        Walkable_Add(item_num, *(const XYZ_32 *)Vector_Get(positions, i));
    }
    Vector_Free(positions);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->ceiling_height_func = M_GetCeilingHeight;
    obj->collision_func = M_Collision;
    obj->control_func = M_Control;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->floor_height_func = M_GetFloorHeight;
    obj->add_walkable_func = M_AddWalkable;
}

REGISTER_OBJECT(O_DRAWBRIDGE, M_Setup)

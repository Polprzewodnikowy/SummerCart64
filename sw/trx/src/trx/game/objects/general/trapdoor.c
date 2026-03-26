#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/objects.h>
#include <trx/game/objects/traps/movable_block.h>

typedef enum {
    TRAPDOOR_STATE_CLOSED,
    TRAPDOOR_STATE_OPEN,
} TRAPDOOR_STATE;

typedef enum {
    TRAPDOOR_ANIM_CLOSED = 0,
} TRAPDOOR_ANIM;

static bool M_IsItemOnTop(
    const ITEM *const item, const int32_t x, const int32_t z)
{
    const BOUNDS_16 *const orig_bounds = &Item_GetBestFrame(item)->bounds;
    if (orig_bounds == nullptr) {
        return false;
    }

    BOUNDS_16 fixed_bounds = {};

    // Bounds need to change in order to account for 2 sector trapdoors
    // and the trapdoor angle.
    if (item->rot.y == 0) {
        fixed_bounds.min.x = orig_bounds->min.x;
        fixed_bounds.max.x = orig_bounds->max.x;
        fixed_bounds.min.z = orig_bounds->min.z;
        fixed_bounds.max.z = orig_bounds->max.z;
    } else if (item->rot.y == DEG_90) {
        fixed_bounds.min.x = orig_bounds->min.z;
        fixed_bounds.max.x = orig_bounds->max.z;
        fixed_bounds.min.z = -orig_bounds->max.x;
        fixed_bounds.max.z = -orig_bounds->min.x;
    } else if (item->rot.y == -DEG_180) {
        fixed_bounds.min.x = -orig_bounds->max.x;
        fixed_bounds.max.x = -orig_bounds->min.x;
        fixed_bounds.min.z = -orig_bounds->max.z;
        fixed_bounds.max.z = -orig_bounds->min.z;
    } else if (item->rot.y == -DEG_90) {
        fixed_bounds.min.x = -orig_bounds->max.z;
        fixed_bounds.max.x = -orig_bounds->min.z;
        fixed_bounds.min.z = orig_bounds->min.x;
        fixed_bounds.max.z = orig_bounds->max.x;
    }

    if (x <= item->pos.x + fixed_bounds.max.x
        && x >= item->pos.x + fixed_bounds.min.x
        && z <= item->pos.z + fixed_bounds.max.z
        && z >= item->pos.z + fixed_bounds.min.z) {
        return true;
    }

    return false;
}

static int16_t M_GetFloorHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (!M_IsItemOnTop(item, x, z)) {
        return height;
    } else if (item->current_anim_state != TRAPDOOR_STATE_CLOSED) {
        return height;
    } else if (y > item->pos.y || item->pos.y > height) {
        return height;
    } else {
        return item->pos.y;
    }
}

static int16_t M_GetCeilingHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (!M_IsItemOnTop(item, x, z)) {
        return height;
    } else if (item->current_anim_state != TRAPDOOR_STATE_CLOSED) {
        return height;
    } else if (y <= item->pos.y || item->pos.y <= height) {
        return height;
    } else {
        return item->pos.y + STEP_L;
    }
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
        Object_GetAnim(obj, TRAPDOOR_ANIM_CLOSED)->frame_ptr;
    const BOUNDS_16 rot_bounds = M_RotateBounds(frame->bounds, item->rot.y);

    const int32_t x0 = item->pos.x + rot_bounds.min.x;
    const int32_t x1 = item->pos.x + rot_bounds.max.x - 1;
    const int32_t z0 = item->pos.z + rot_bounds.min.z;
    const int32_t z1 = item->pos.z + rot_bounds.max.z - 1;

    const int32_t sx0 = Math_FloorDiv(x0, WALL_L);
    const int32_t sx1 = Math_FloorDiv(x1, WALL_L);
    const int32_t sz0 = Math_FloorDiv(z0, WALL_L);
    const int32_t sz1 = Math_FloorDiv(z1, WALL_L);

    for (int32_t sx = sx0; sx <= sx1; ++sx) {
        for (int32_t sz = sz0; sz <= sz1; ++sz) {
            XYZ_32 pos = {
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
    VECTOR *const positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    for (int32_t i = 0; i < positions->count; i++) {
        MovableBlock_DropStack(
            *(const XYZ_32 *)Vector_Get(positions, i), item->room_num);
    }
    Vector_Free(positions);
}

static void M_AddWalkable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    VECTOR *const positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    for (int32_t i = 0; i < positions->count; i++) {
        Walkable_Add(item_num, *(const XYZ_32 *)Vector_Get(positions, i));
    }
    Vector_Free(positions);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (Item_IsTriggerActive(item)) {
        if (item->current_anim_state == TRAPDOOR_STATE_CLOSED) {
            item->goal_anim_state = TRAPDOOR_STATE_OPEN;
            M_DropStack(item);
        }
    } else {
        if (item->current_anim_state == TRAPDOOR_STATE_OPEN) {
            item->goal_anim_state = TRAPDOOR_STATE_CLOSED;
        }
    }
    Item_Animate(item);
    Item_UpdateRoom(item_num, item->room_num);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->floor_height_func = M_GetFloorHeight;
    obj->ceiling_height_func = M_GetCeilingHeight;
    obj->save_flags = true;
    obj->save_anim = true;
    obj->add_walkable_func = M_AddWalkable;
}

REGISTER_OBJECT(O_TRAPDOOR_TYPE_1, M_Setup)
REGISTER_OBJECT(O_TRAPDOOR_TYPE_2, M_Setup)
REGISTER_OBJECT(O_TRAPDOOR_TYPE_3, M_Setup)

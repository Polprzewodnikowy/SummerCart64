#include <trx/game/items/col.h>

#include <trx/core/utils.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

static BOUNDS_16 m_NullBounds = {};
static BOUNDS_16 m_InterpolatedBounds = {};

int16_t Item_GetHeight(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const int32_t height = Room_GetHeight(sector, item->pos);

    return height;
}

int32_t Item_GetDistance(const ITEM *const item, const XYZ_32 target)
{
    return XYZ_32_GetDistance(item->pos, target);
}

void Item_Translate(
    ITEM *const item, const int32_t x, const int32_t y, const int32_t z)
{
    const int32_t c = Math_Cos(item->rot.y);
    const int32_t s = Math_Sin(item->rot.y);
    item->pos.x += ((c * x + s * z) >> W2V_SHIFT);
    item->pos.y += y;
    item->pos.z += ((c * z - s * x) >> W2V_SHIFT);
}

bool Item_Test3DRange(
    const int32_t x, const int32_t y, const int32_t z, const int32_t range)
{
    return ABS(x) < range && ABS(y) < range && ABS(z) < range
        && (SQUARE(x) + SQUARE(y) + SQUARE(z) < SQUARE(range));
}

bool Item_IsNearby(
    const ITEM *const item_1, const ITEM *const item_2, const int32_t distance)
{
    return XYZ_32_IsNearby(item_1->pos, item_2->pos, distance);
}

bool Item_TestBoundsCollide(
    const ITEM *const src_item, const ITEM *const dst_item,
    const int32_t radius)
{
    const COLL_ITEM src = {
        .bounds = Item_GetBestFrame(src_item)->bounds,
        .pos = src_item->pos,
        .rot = src_item->rot,
    };
    const COLL_ITEM dst = {
        .bounds = Item_GetBestFrame(dst_item)->bounds,
        .pos = dst_item->pos,
        .rot = dst_item->rot,
    };
    return Collide_TestBoundsCollide(&src, &dst, radius);
}

bool Item_TestStatic3DBoundsCollide(
    const STATIC_MESH *const mesh, const ITEM *const dst_item,
    const int32_t radius)
{
    const COLL_ITEM src = {
        .bounds = Object_Get3DStatic(mesh->static_num)->collision_bounds,
        .pos = mesh->pos,
        .rot = { .y = mesh->rot.y },
    };
    const COLL_ITEM dst = {
        .bounds = Item_GetBestFrame(dst_item)->bounds,
        .pos = dst_item->pos,
        .rot = dst_item->rot,
    };
    return Collide_TestBoundsCollide(&src, &dst, radius);
}

const BOUNDS_16 *Item_GetBoundsAccurate(const ITEM *const item)
{
    int32_t rate;
    ANIM_FRAME *frames[2];
    const int32_t frac = Item_GetFrames(item, frames, &rate);
    if (frames[0] == nullptr) {
        return &m_NullBounds;
    }

    if (frac == 0) {
        return &frames[0]->bounds;
    }

#define CALC(target, b1, b2, prop)                                             \
    target->prop = (b1)->prop + ((((b2)->prop - (b1)->prop) * frac) / rate);
    BOUNDS_16 *const result = &m_InterpolatedBounds;
    CALC(result, &frames[0]->bounds, &frames[1]->bounds, min.x);
    CALC(result, &frames[0]->bounds, &frames[1]->bounds, max.x);
    CALC(result, &frames[0]->bounds, &frames[1]->bounds, min.y);
    CALC(result, &frames[0]->bounds, &frames[1]->bounds, max.y);
    CALC(result, &frames[0]->bounds, &frames[1]->bounds, min.z);
    CALC(result, &frames[0]->bounds, &frames[1]->bounds, max.z);
#undef CALC

    return result;
}

BOUNDS_16 Item_RotateBounds(const ITEM *const item, const int16_t rot_y)
{
    const BOUNDS_16 *bounds = &Item_GetBestFrame(item)->bounds;
    BOUNDS_16 rot_bounds = {};
    if (bounds == nullptr) {
        return rot_bounds;
    }

    switch (rot_y) {
    case 0:
    default:
        rot_bounds = *bounds;
        break;
    case DEG_90:
        rot_bounds.min.x = bounds->min.z;
        rot_bounds.max.x = bounds->max.z;
        rot_bounds.min.z = -bounds->max.x;
        rot_bounds.max.z = -bounds->min.x;
        break;
    case -DEG_180:
        rot_bounds.min.x = -bounds->max.x;
        rot_bounds.max.x = -bounds->min.x;
        rot_bounds.min.z = -bounds->max.z;
        rot_bounds.max.z = -bounds->min.z;
        break;
    case -DEG_90:
        rot_bounds.min.x = -bounds->max.z;
        rot_bounds.max.x = -bounds->min.z;
        rot_bounds.min.z = bounds->min.x;
        rot_bounds.max.z = bounds->max.x;
        break;
    }
    return rot_bounds;
}

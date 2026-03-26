#include <trx/game/collision/los.h>

#include <trx/core/utils.h>
#include <trx/game/items.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>

#include <math.h>

#define M_CLIP_1 8
#define M_CLIP_2 8
#define M_MAX_LOS_ROOMS 200

static int32_t m_LOSRooms[M_MAX_LOS_ROOMS] = {};
static int32_t m_LOSNumRooms = 0;

static inline bool M_TryPushLOSRoom(const int16_t room_num)
{
    if (m_LOSNumRooms < M_MAX_LOS_ROOMS) {
        m_LOSRooms[m_LOSNumRooms++] = room_num;
        return true;
    }
    return false;
}

static inline bool M_ResetLOSRooms(const int16_t room_num)
{
    m_LOSNumRooms = 0;
    return M_TryPushLOSRoom(room_num);
}

static inline bool M_RoomInLOSRooms(const int16_t room_num)
{
    for (int32_t i = 0; i < m_LOSNumRooms; i++) {
        if (m_LOSRooms[i] == room_num) {
            return true;
        }
    }
    return false;
}

// This routine transforms the world-space LOS segment [start,target] into the
// object's local coordinates (undoing its translation and Y-rotation), then
// performs a slab intersection test against that local AABB. The first
// smashable item hit is returned, or NO_ITEM if none.
//
// (AABB = Axis-Aligned Bounding Box. It's the rectangular box defined by
// bounds->min/max along X,Y,Z in the object's local space (no rotation).)
//
// @param start  World-space ray origin
// @param target World-space ray end
// @param item   Item to check
// @return       Whether the item collides with the segment
static bool M_ItemIntersectSegment(
    const XYZ_32 start, const XYZ_32 target, const ITEM *const item,
    XYZ_32 *const out_hit_pos)
{
    const double dx = target.x - start.x;
    const double dy = target.y - start.y;
    const double dz = target.z - start.z;

    // Translate into object-local space
    const double ox = start.x - item->pos.x;
    const double oy = start.y - item->pos.y;
    const double oz = start.z - item->pos.z;

    // Unrotate by -rot.y around Y axis
    const float c = cosf(-item->rot.y * M_PI / DEG_180);
    const float s = sinf(-item->rot.y * M_PI / DEG_180);
    const double lx = ox * c + oz * s;
    const double ly = oy;
    const double lz = oz * c - ox * s;
    const double ldx = dx * c + dz * s;
    const double ldy = dy;
    const double ldz = dz * c - dx * s;

    // Local AABB extents from item's bounds
    const BOUNDS_16 *const orig_bounds = Item_GetBoundsAccurate(item);
    BOUNDS_16 patched_bounds = *orig_bounds;

    const BOUNDS_16 *const bounds = &patched_bounds;

    // Parametric interval [t0..t1] in Q14 fixed-point
    double t0 = 0.0;
    double t1 = 1.0;

    // X slab
    if (ldx != 0) {
        double t_near = (double)(bounds->min.x - lx) / ldx;
        double t_far = (double)(bounds->max.x - lx) / ldx;
        if (t_near > t_far) {
            SWAP(t_near, t_far);
        }
        if (t_near > t1 || t_far < t0) {
            return false;
        }
        CLAMPL(t0, t_near);
        CLAMPG(t1, t_far);
    } else if (lx < bounds->min.x || lx > bounds->max.x) {
        return false;
    }

    // Y slab
    if (ldy != 0) {
        double t_near = (double)(bounds->min.y - ly) / ldy;
        double t_far = (double)(bounds->max.y - ly) / ldy;
        if (t_near > t_far) {
            SWAP(t_near, t_far);
        }
        if (t_near > t1 || t_far < t0) {
            return false;
        }
        CLAMPL(t0, t_near);
        CLAMPG(t1, t_far);
    } else if (ly < bounds->min.y || ly > bounds->max.y) {
        return false;
    }

    // Z slab
    if (ldz != 0) {
        double t_near = (double)(bounds->min.z - lz) / ldz;
        double t_far = (double)(bounds->max.z - lz) / ldz;
        if (t_near > t_far) {
            SWAP(t_near, t_far);
        }
        if (t_near > t1 || t_far < t0) {
            return false;
        }
        CLAMPL(t0, t_near);
        CLAMPG(t1, t_far);
    } else if (lz < bounds->min.z || lz > bounds->max.z) {
        return false;
    }

    if (out_hit_pos != nullptr) {
        // world-space hit position = start + t0 * (target-start)
        out_hit_pos->x = start.x + t0 * dx;
        out_hit_pos->y = start.y + t0 * dy;
        out_hit_pos->z = start.z + t0 * dz;
    }

    return true;
}

// Smashes all objects along the ray path.
//
// @param start  World-space ray origin
// @param target World-space ray end
// @return       First smashable item's index, or NO_ITEM if none hit
int32_t LOS_CheckSmashable(
    const GAME_VECTOR start, const GAME_VECTOR target,
    XYZ_32 *const out_hit_pos)
{
    int32_t best_dist = INT32_MAX;
    int16_t best_item_num = NO_ITEM;

    for (int16_t item_num = 0; item_num < Item_GetTotalCount(); item_num++) {
        const ITEM *const item = Item_Get(item_num);
        if (item->status == IS_DEACTIVATED) {
            continue;
        }
        if (!Object_IsType(item->object_id, g_SmashableObjects)
            && !Object_IsType(item->object_id, g_ShatterableObjects)
            && !Object_IsType(item->object_id, g_HeavyShatterableObjects)) {
            continue;
        }

        XYZ_32 hit_pos;
        if (!M_ItemIntersectSegment(start.pos, target.pos, item, &hit_pos)) {
            continue;
        }

        // Confirm item is reachable via visible rooms
        {
            GAME_VECTOR start_tmp = start;
            GAME_VECTOR target_tmp = {
                .pos = item->pos,
            };
            if (!LOS_Check(&start_tmp, &target_tmp, false)) {
                continue;
            }
        }

        // Ray segment intersects the object's local AABB
        const int32_t dist = Item_GetDistance(item, start.pos);
        if (dist < best_dist) {
            best_dist = dist;
            best_item_num = item_num;
            if (out_hit_pos != nullptr) {
                *out_hit_pos = hit_pos;
            }
        }
    }

    return best_item_num;
}

static int32_t M_CheckX(
    const GAME_VECTOR *const start, GAME_VECTOR *const target)
{
    const int32_t dx = target->x - start->x;
    if (dx == 0) {
        return 1;
    }

    const int32_t dy = ((target->y - start->y) * WALL_L) / dx;
    const int32_t dz = ((target->z - start->z) * WALL_L) / dx;

    int16_t room_num = start->room_num;
    int16_t last_room_num = start->room_num;

    M_ResetLOSRooms(room_num);

    if (dx < 0) {
        XYZ_32 cur_pos;
        cur_pos.x = ROUND_TO_SECTOR(start->x);
        cur_pos.y = start->y + ((dy * (cur_pos.x - start->x)) >> WALL_SHIFT);
        cur_pos.z = start->z + ((dz * (cur_pos.x - start->x)) >> WALL_SHIFT);

        while (cur_pos.x > target->x) {
            XYZ_32 sample_pos = cur_pos;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = room_num;
                    return -1;
                }
            }

            if (room_num != last_room_num) {
                last_room_num = room_num;
                M_TryPushLOSRoom(room_num);
            }

            sample_pos.x -= 1;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = last_room_num;
                    return 0;
                }
            }

            cur_pos.x -= WALL_L;
            cur_pos.y -= dy;
            cur_pos.z -= dz;
        }
    } else {
        XYZ_32 cur_pos;
        cur_pos.x = ROUND_TO_SECTOR_END(start->x);
        cur_pos.y = start->y + (((cur_pos.x - start->x) * dy) >> WALL_SHIFT);
        cur_pos.z = start->z + (((cur_pos.x - start->x) * dz) >> WALL_SHIFT);

        while (cur_pos.x < target->x) {
            XYZ_32 sample_pos = cur_pos;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = room_num;
                    return -1;
                }
            }

            if (room_num != last_room_num) {
                last_room_num = room_num;
                M_TryPushLOSRoom(room_num);
            }

            sample_pos.x += 1;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = last_room_num;
                    return 0;
                }
            }

            cur_pos.x += WALL_L;
            cur_pos.y += dy;
            cur_pos.z += dz;
        }
    }

    target->room_num = room_num;
    return 1;
}

static int32_t M_CheckZ(
    const GAME_VECTOR *const start, GAME_VECTOR *const target)
{
    const int32_t dz = target->z - start->z;
    if (dz == 0) {
        return 1;
    }

    const int32_t dx = ((target->x - start->x) * WALL_L) / dz;
    const int32_t dy = ((target->y - start->y) * WALL_L) / dz;

    int16_t room_num = start->room_num;
    int16_t last_room_num = start->room_num;

    M_ResetLOSRooms(room_num);

    if (dz < 0) {
        XYZ_32 cur_pos;
        cur_pos.z = ROUND_TO_SECTOR(start->z);
        cur_pos.x = start->x + ((dx * (cur_pos.z - start->z)) >> WALL_SHIFT);
        cur_pos.y = start->y + ((dy * (cur_pos.z - start->z)) >> WALL_SHIFT);

        while (cur_pos.z > target->z) {
            XYZ_32 sample_pos = cur_pos;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = room_num;
                    return -1;
                }
            }

            if (room_num != last_room_num) {
                last_room_num = room_num;
                M_TryPushLOSRoom(room_num);
            }

            sample_pos.z -= 1;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = last_room_num;
                    return 0;
                }
            }

            cur_pos.z -= WALL_L;
            cur_pos.x -= dx;
            cur_pos.y -= dy;
        }
    } else {
        XYZ_32 cur_pos;
        cur_pos.z = ROUND_TO_SECTOR_END(start->z);
        cur_pos.x = start->x + ((dx * (cur_pos.z - start->z)) >> WALL_SHIFT);
        cur_pos.y = start->y + ((dy * (cur_pos.z - start->z)) >> WALL_SHIFT);

        while (cur_pos.z < target->z) {
            XYZ_32 sample_pos = cur_pos;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = room_num;
                    return -1;
                }
            }

            if (room_num != last_room_num) {
                last_room_num = room_num;
                M_TryPushLOSRoom(room_num);
            }

            sample_pos.z += 1;

            {
                const SECTOR *const sector =
                    Room_GetSector(sample_pos, &room_num);
                const int32_t height = Room_GetHeight(sector, sample_pos);
                const int32_t ceiling = Room_GetCeiling(sector, sample_pos);
                if (cur_pos.y > height || cur_pos.y < ceiling) {
                    target->pos = cur_pos;
                    target->room_num = last_room_num;
                    return 0;
                }
            }

            cur_pos.z += WALL_L;
            cur_pos.x += dx;
            cur_pos.y += dy;
        }
    }

    target->room_num = room_num;
    return 1;
}

static int32_t M_ClipTargetSimple(
    const GAME_VECTOR *const start, GAME_VECTOR *const target)
{
    const SECTOR *sector = Room_GetSector(target->pos, &target->room_num);

    // This function exists because of issue #4070.
    int32_t dx = target->x - start->x;
    int32_t dy = target->y - start->y;
    int32_t dz = target->z - start->z;

    const int32_t height = Room_GetHeight(sector, target->pos);
    if (target->y > height && start->y < height) {
        target->y = height;
        target->x = start->x + dx * (height - start->y) / dy;
        target->z = start->z + dz * (height - start->y) / dy;
        return false;
    }

    const int32_t ceiling = Room_GetCeiling(sector, target->pos);
    if (target->y < ceiling && start->y > ceiling) {
        target->y = ceiling;
        target->x = start->x + dx * (ceiling - start->y) / dy;
        target->z = start->z + dz * (ceiling - start->y) / dy;
        return false;
    }

    return true;
}

static int32_t M_ClipTargetWithSlopes(
    const GAME_VECTOR *const start, GAME_VECTOR *const target)
{
    int16_t room_num = target->room_num;
    const SECTOR *sector = Room_GetSector(target->pos, &room_num);

    if (target->y > Room_GetHeight(sector, target->pos)) {
        const XYZ_32 origin = {
            start->x + ((M_CLIP_1 - 1) * (target->x - start->x) / M_CLIP_1),
            start->y + ((M_CLIP_1 - 1) * (target->y - start->y) / M_CLIP_1),
            start->z + ((M_CLIP_1 - 1) * (target->z - start->z) / M_CLIP_1),
        };
        XYZ_32 delta;
        for (int32_t i = M_CLIP_2 - 1; i > 0; i--) {
            delta.x = origin.x + (i * (target->x - origin.x) / M_CLIP_2);
            delta.y = origin.y + (i * (target->y - origin.y) / M_CLIP_2);
            delta.z = origin.z + (i * (target->z - origin.z) / M_CLIP_2);
            sector = Room_GetSector(delta, &room_num);
            if (delta.y < Room_GetHeight(sector, delta)) {
                break;
            }
        }

        target->pos = delta;
        target->room_num = room_num;
        return 0;
    }

    if (target->y < Room_GetCeiling(sector, target->pos)) {
        const XYZ_32 origin = {
            start->x + ((M_CLIP_1 - 1) * (target->x - start->x) / M_CLIP_1),
            start->y + ((M_CLIP_1 - 1) * (target->y - start->y) / M_CLIP_1),
            start->z + ((M_CLIP_1 - 1) * (target->z - start->z) / M_CLIP_1),
        };
        XYZ_32 delta;
        for (int32_t i = M_CLIP_2 - 1; i > 0; i--) {
            delta.x = origin.x + (i * (target->x - origin.x) / M_CLIP_2);
            delta.y = origin.y + (i * (target->y - origin.y) / M_CLIP_2);
            delta.z = origin.z + (i * (target->z - origin.z) / M_CLIP_2);

            sector = Room_GetSector(delta, &room_num);
            if (delta.y > Room_GetCeiling(sector, delta)) {
                break;
            }
        }

        target->pos = delta;
        target->room_num = room_num;
        return 0;
    }

    return 1;
}

bool LOS_Check(
    const GAME_VECTOR *const start, GAME_VECTOR *const target,
    const bool use_slope_clipping)
{
    const int32_t dx = ABS(target->x - start->x);
    const int32_t dz = ABS(target->z - start->z);

    int32_t los1;
    int32_t los2;
    if (dz > dx) {
        los1 = M_CheckX(start, target);
        los2 = M_CheckZ(start, target);
    } else {
        los1 = M_CheckZ(start, target);
        los2 = M_CheckX(start, target);
    }

    if (!los2) {
        return false;
    }

    if (dx == 0 && dz == 0) {
        target->room_num = start->room_num;
    }

    const bool clip_result =
        (use_slope_clipping ? M_ClipTargetWithSlopes(start, target)
                            : M_ClipTargetSimple(start, target));
    return clip_result && los1 == 1 && los2 == 1;
}

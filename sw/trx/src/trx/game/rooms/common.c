#include <trx/game/rooms/common.h>

#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/game_buf.h>
#include <trx/game/level.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/vars.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/sound/common.h>
#include <trx/version.h>

#include <string.h>

static int32_t m_RoomCount = 0;
static ROOM *m_Rooms = nullptr;
static bool m_FlipStatus = false;
static int32_t m_FlipEffect = -1;
static int32_t m_FlipTimer = 0;
static int32_t m_FlipSlotFlags[MAX_FLIP_MAPS] = {};

#define M_OUTSIDE_TABLE_STEP_SHIFT 2
#define M_OUTSIDE_TABLE_STEP (1 << M_OUTSIDE_TABLE_STEP_SHIFT)
#define M_OUTSIDE_TABLE_BLOCK_SHIFT (WALL_SHIFT + M_OUTSIDE_TABLE_STEP_SHIFT)
#define M_OUTSIDE_TABLE_MAX_ROOMS_PER_CELL 64
#define M_OUTSIDE_TABLE_SENTINEL NO_ROOM
#define M_OUTSIDE_OFFSET_EMPTY 0xFFFF

static int16_t *m_OutsideRoomTable = nullptr;
static uint16_t *m_OutsideRoomOffsets = nullptr;
static int32_t m_OutsideGridX = 0;
static int32_t m_OutsideGridZ = 0;
static int32_t m_OutsideOriginCellX = 0;
static int32_t m_OutsideOriginCellZ = 0;

static void M_AddFlipItems(const ROOM *const room)
{
    int16_t item_num = room->item_num;
    while (item_num != NO_ITEM) {
        ITEM *const item = Item_Get(item_num);
        const OBJECT *const obj = Object_Get(item->object_id);

        if (obj->handle_flip_func != nullptr) {
            obj->handle_flip_func(item, RFS_FLIPPED);
        }

        item_num = item->next_item;
    }
}

static void M_RemoveFlipItems(const ROOM *const room)
{
    int16_t item_num = room->item_num;
    while (item_num != NO_ITEM) {
        ITEM *const item = Item_Get(item_num);
        const OBJECT *const obj = Object_Get(item->object_id);

        if (obj->handle_flip_func != nullptr) {
            obj->handle_flip_func(item, RFS_UNFLIPPED);
        }

        // TR2 does not have land/water objects like crocodile/alligator in TR1,
        // so avoid instances of floating water creatures in drained rooms.
        if (g_TRVersion >= 2 && (item->flags & IF_ONE_SHOT) && obj->intelligent
            && item->hit_points <= 0) {
            Item_RemoveDrawn(item_num);
            item->flags |= IF_KILLED;
        }

        item_num = item->next_item;
    }
}

static void M_GetNewRoom(
    const int32_t x, const int32_t y, const int32_t z, int16_t room_num)
{
    Room_GetSector((XYZ_32) { x, y, z }, &room_num);
    Room_MarkToBeDrawn(room_num);
}

void Room_InitialiseRooms(const int32_t num_rooms)
{
    m_RoomCount = num_rooms;
    m_Rooms = num_rooms == 0
        ? nullptr
        : GameBuf_Alloc(sizeof(ROOM) * num_rooms, GBUF_ROOMS);

    m_OutsideRoomTable = nullptr;
    m_OutsideRoomOffsets = nullptr;
    m_OutsideGridX = 0;
    m_OutsideGridZ = 0;
    m_OutsideOriginCellX = 0;
    m_OutsideOriginCellZ = 0;
}

void Room_Shutdown(void)
{
    m_RoomCount = 0;
    m_Rooms = nullptr;
    m_FlipStatus = false;
    m_FlipEffect = -1;
    m_FlipTimer = 0;
    memset(m_FlipSlotFlags, 0, sizeof(m_FlipSlotFlags));

    m_OutsideRoomTable = nullptr;
    m_OutsideRoomOffsets = nullptr;
    m_OutsideGridX = 0;
    m_OutsideGridZ = 0;
    m_OutsideOriginCellX = 0;
    m_OutsideOriginCellZ = 0;
}

int32_t Room_GetCount(void)
{
    return m_RoomCount;
}

ROOM *Room_Get(const int32_t room_num)
{
    if (m_Rooms == nullptr) {
        return nullptr;
    }
    if (room_num < 0 || room_num >= Room_GetCount()) {
        return nullptr;
    }
    return &m_Rooms[room_num];
}

void Room_BuildOutsideTable(void)
{
    m_OutsideRoomTable = nullptr;
    m_OutsideRoomOffsets = nullptr;
    m_OutsideGridX = 0;
    m_OutsideGridZ = 0;
    m_OutsideOriginCellX = 0;
    m_OutsideOriginCellZ = 0;

    const int32_t num_rooms = Room_GetCount();
    if (num_rooms <= 0) {
        return;
    }

    {
        int32_t min_x = INT32_MAX;
        int32_t min_z = INT32_MAX;
        int32_t max_x = INT32_MIN;
        int32_t max_z = INT32_MIN;
        for (int32_t i = 0; i < num_rooms; i++) {
            const ROOM *const room = Room_Get(i);
            if (room == nullptr) {
                continue;
            }
            min_x = MIN(min_x, room->pos.x);
            min_z = MIN(min_z, room->pos.z);
            max_x = MAX(max_x, room->pos.x + (room->size.x << WALL_SHIFT));
            max_z = MAX(max_z, room->pos.z + (room->size.z << WALL_SHIFT));
        }

        m_OutsideOriginCellX = (min_x >> M_OUTSIDE_TABLE_BLOCK_SHIFT) - 1;
        m_OutsideOriginCellZ = (min_z >> M_OUTSIDE_TABLE_BLOCK_SHIFT) - 1;
        const int32_t max_cell_x = (max_x >> M_OUTSIDE_TABLE_BLOCK_SHIFT) + 1;
        const int32_t max_cell_z = (max_z >> M_OUTSIDE_TABLE_BLOCK_SHIFT) + 1;

        m_OutsideGridX = max_cell_x - m_OutsideOriginCellX + 1;
        m_OutsideGridZ = max_cell_z - m_OutsideOriginCellZ + 1;
        if (m_OutsideGridX < 1) {
            m_OutsideGridX = 1;
        }
        if (m_OutsideGridZ < 1) {
            m_OutsideGridZ = 1;
        }
    }

    const int32_t full_table_size =
        m_OutsideGridX * m_OutsideGridZ * M_OUTSIDE_TABLE_MAX_ROOMS_PER_CELL;
    int16_t *full_table = Memory_Alloc(full_table_size * sizeof(int16_t));
    for (int32_t i = 0; i < full_table_size; i++) {
        full_table[i] = M_OUTSIDE_TABLE_SENTINEL;
    }

    const int32_t blocks_x = m_OutsideGridX * M_OUTSIDE_TABLE_STEP;
    const int32_t blocks_z = m_OutsideGridZ * M_OUTSIDE_TABLE_STEP;
    for (int32_t y = 0; y < blocks_x; y += M_OUTSIDE_TABLE_STEP) {
        for (int32_t x = 0; x < blocks_z; x += M_OUTSIDE_TABLE_STEP) {
            for (int32_t i = 0; i < num_rooms; i++) {
                const ROOM *const room = Room_Get(i);

                const int32_t room_x = (room->pos.z >> WALL_SHIFT)
                    - (m_OutsideOriginCellZ * M_OUTSIDE_TABLE_STEP);
                const int32_t room_y = (room->pos.x >> WALL_SHIFT)
                    - (m_OutsideOriginCellX * M_OUTSIDE_TABLE_STEP);

                bool cont = false;
                for (int32_t ry = 0; ry < M_OUTSIDE_TABLE_STEP && !cont; ry++) {
                    for (int32_t rx = 0; rx < M_OUTSIDE_TABLE_STEP; rx++) {
                        if (x + rx >= room_x && x + rx < room_x + room->size.z
                            && y + ry >= room_y
                            && y + ry < room_y + room->size.x) {
                            cont = true;
                            break;
                        }
                    }
                }

                if (!cont) {
                    continue;
                }

                int16_t *const cell =
                    &full_table
                        [M_OUTSIDE_TABLE_MAX_ROOMS_PER_CELL
                         * ((x >> M_OUTSIDE_TABLE_STEP_SHIFT)
                            + m_OutsideGridZ
                                * (y >> M_OUTSIDE_TABLE_STEP_SHIFT))];
                for (int32_t lp = 0; lp < M_OUTSIDE_TABLE_MAX_ROOMS_PER_CELL;
                     lp++) {
                    if (cell[lp] == M_OUTSIDE_TABLE_SENTINEL) {
                        cell[lp] = i;
                        break;
                    }
                }
            }
        }
    }

    const int32_t offset_count = m_OutsideGridX * m_OutsideGridZ;
    m_OutsideRoomOffsets = GameBuf_Alloc(
        sizeof(uint16_t) * (size_t)offset_count, GBUF_OUTSIDE_ROOM_TABLE);
    for (int32_t i = 0; i < offset_count; i++) {
        m_OutsideRoomOffsets[i] = M_OUTSIDE_OFFSET_EMPTY;
    }

    m_OutsideRoomTable = GameBuf_Alloc(
        full_table_size * sizeof(int16_t), GBUF_OUTSIDE_ROOM_TABLE);
    int16_t *const out_base = m_OutsideRoomTable;
    int16_t *out_ptr = out_base;

    for (int32_t y = 0; y < m_OutsideGridX; y++) {
        for (int32_t x = 0; x < m_OutsideGridZ; x++) {
            const int32_t cell_idx = x + y * m_OutsideGridZ;
            const int16_t *const cell =
                &full_table[M_OUTSIDE_TABLE_MAX_ROOMS_PER_CELL * cell_idx];

            int32_t count = 0;
            while (count < M_OUTSIDE_TABLE_MAX_ROOMS_PER_CELL
                   && cell[count] != M_OUTSIDE_TABLE_SENTINEL) {
                count++;
            }

            if (count == 0) {
                continue;
            }

            if (count == 1) {
                m_OutsideRoomOffsets[cell_idx] = 0x8000U | (uint16_t)cell[0];
                continue;
            }

            int16_t *scan = out_base;
            while (scan < out_ptr) {
                if (memcmp(scan, cell, count * sizeof(int16_t)) == 0) {
                    m_OutsideRoomOffsets[cell_idx] =
                        (uint16_t)(scan - out_base);
                    break;
                }

                int32_t scan_len = 0;
                while (scan[scan_len] != M_OUTSIDE_TABLE_SENTINEL) {
                    scan_len++;
                }
                scan += scan_len + 1;
            }

            if (scan < out_ptr) {
                continue;
            }

            const int32_t new_off = (int32_t)(out_ptr - out_base);
            ASSERT(new_off >= 0);
            ASSERT(new_off < 0x8000);
            m_OutsideRoomOffsets[cell_idx] = new_off;

            ASSERT(new_off + count + 1 <= full_table_size);
            memcpy(out_ptr, cell, count * sizeof(int16_t));
            out_ptr += count;
            *out_ptr++ = M_OUTSIDE_TABLE_SENTINEL;
        }
    }

    Memory_FreePointer(&full_table);
}

int32_t Room_GetOutsideStatus(const XYZ_32 pos, int16_t *const out_room_num)
{
    if (out_room_num != nullptr) {
        *out_room_num = NO_ROOM;
    }

    if (m_OutsideRoomTable == nullptr || m_OutsideRoomOffsets == nullptr) {
        return -2;
    }

    const int32_t cell_x =
        (pos.x >> M_OUTSIDE_TABLE_BLOCK_SHIFT) - m_OutsideOriginCellX;
    const int32_t cell_z =
        (pos.z >> M_OUTSIDE_TABLE_BLOCK_SHIFT) - m_OutsideOriginCellZ;
    if (cell_x < 0 || cell_x >= m_OutsideGridX || cell_z < 0
        || cell_z >= m_OutsideGridZ) {
        return -2;
    }

    const uint16_t entry =
        m_OutsideRoomOffsets[m_OutsideGridZ * cell_x + cell_z];
    if (entry == M_OUTSIDE_OFFSET_EMPTY) {
        return -2;
    }

    const int16_t *p = nullptr;
    int16_t single_room = M_OUTSIDE_TABLE_SENTINEL;
    if ((entry & 0x8000U) != 0U) {
        single_room = (int16_t)(entry & ~0x8000U);
    } else {
        p = &m_OutsideRoomTable[entry];
    }

    while (true) {
        int16_t candidate_room_num;
        if (p != nullptr) {
            if (*p == M_OUTSIDE_TABLE_SENTINEL) {
                break;
            }
            candidate_room_num = (int16_t)(*p);
            p++;
        } else {
            if (single_room == M_OUTSIDE_TABLE_SENTINEL) {
                break;
            }
            candidate_room_num = single_room;
            single_room = M_OUTSIDE_TABLE_SENTINEL;
        }

        const ROOM *const room = Room_Get(candidate_room_num);
        if (room == nullptr) {
            continue;
        }

        if (pos.y <= room->max_ceiling || pos.y >= room->min_floor) {
            continue;
        }

        if (pos.z <= room->pos.z + WALL_L
            || pos.z >= room->pos.z + (room->size.z << WALL_SHIFT) - WALL_L) {
            continue;
        }

        if (pos.x <= room->pos.x + WALL_L
            || pos.x >= room->pos.x + (room->size.x << WALL_SHIFT) - WALL_L) {
            continue;
        }

        int16_t rn = candidate_room_num;
        const SECTOR *const sector = Room_GetSector(pos, &rn);
        const int32_t floor = Room_GetHeight(sector, pos);
        if (floor == NO_HEIGHT || pos.y > floor) {
            return -2;
        }

        const int32_t ceiling = Room_GetCeiling(sector, pos);
        if (pos.y < ceiling) {
            return -2;
        }

        if (!room->flags.underwater && !room->flags.wind) {
            return -3;
        }

        if (out_room_num != nullptr) {
            *out_room_num = candidate_room_num;
        }
        return 1;
    }

    return -2;
}

int32_t Room_GetNumber(const ROOM *const room)
{
    if (room == nullptr) {
        return NO_ROOM;
    }
    return room - m_Rooms;
}

void Room_InitialiseFlipStatus(void)
{
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        ROOM *const room = Room_Get(i);
        // Some level data links only one side of a flip pair. In that case, a
        // previous iteration may already have marked this room as flipped.
        if (room->flip_status == RFS_FLIPPED) {
            continue;
        }
        if (room->flipped_room == NO_ROOM) {
            room->flip_status = RFS_NONE;
        } else {
            ROOM *const flipped_room = Room_Get(room->flipped_room);
            room->flip_status = RFS_UNFLIPPED;
            flipped_room->flip_status = RFS_FLIPPED;
        }
    }

    m_FlipStatus = false;
    m_FlipEffect = -1;
    m_FlipTimer = 0;
    for (int32_t i = 0; i < MAX_FLIP_MAPS; i++) {
        m_FlipSlotFlags[i] = 0;
    }
}

void Room_FlipMap(void)
{
    Walkable_Reset();

    for (int32_t i = 0; i < Room_GetCount(); i++) {
        ROOM *const room = Room_Get(i);
        if (room->flipped_room < 0) {
            continue;
        }

        M_RemoveFlipItems(room);

        ROOM *const flipped = Room_Get(room->flipped_room);
        const ROOM temp = *room;
        *room = *flipped;
        *flipped = temp;

        room->flipped_room = flipped->flipped_room;
        room->flip_status = RFS_UNFLIPPED;
        flipped->flipped_room = NO_ROOM;
        flipped->flip_status = RFS_FLIPPED;

        room->item_num = flipped->item_num;
        room->effect_num = flipped->effect_num;
        memcpy(
            &room->drawn_items, &flipped->drawn_items,
            sizeof(room->drawn_items));

        M_AddFlipItems(room);
    }

    m_FlipStatus = !m_FlipStatus;

    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const ROOM *const room = Room_Get(i);
        if (room->flip_status != RFS_NONE) {
            Output_DispatchRoomFlip(room);
        }
    }

    Level_Finalize_LoadWalkables(Level_Context_Get());
}

bool Room_GetFlipStatus(void)
{
    return m_FlipStatus;
}

int32_t Room_GetFlipEffect(void)
{
    return m_FlipEffect;
}

void Room_SetFlipEffect(const int32_t flip_effect)
{
    m_FlipEffect = flip_effect;
}

int32_t Room_GetFlipTimer(void)
{
    return m_FlipTimer;
}

void Room_SetFlipTimer(const int32_t flip_timer)
{
    m_FlipTimer = flip_timer;
}

void Room_IncrementFlipTimer(const int32_t num_frames)
{
    m_FlipTimer += num_frames;
}

int32_t Room_GetFlipSlotFlags(const int32_t slot_idx)
{
    return m_FlipSlotFlags[slot_idx];
}

void Room_SetFlipSlotFlags(const int32_t slot_idx, const int32_t flags)
{
    m_FlipSlotFlags[slot_idx] = flags;
}

int32_t Room_GetAdjoiningRooms(
    int16_t init_room_num, int16_t out_room_nums[],
    const int32_t max_room_num_count)
{
    int32_t count = 0;
    if (max_room_num_count >= 1) {
        out_room_nums[count++] = init_room_num;
    }

    const PORTALS *const portals = Room_Get(init_room_num)->portals;
    if (portals != nullptr) {
        for (int32_t i = 0; i < portals->count; i++) {
            if (count >= max_room_num_count) {
                break;
            }
            const int16_t room_num = portals->portal[i].room_num;
            out_room_nums[count++] = room_num;
        }
    }

    return count;
}

bool Room_PointInside(const ROOM *const room, const XYZ_32 point)
{
    if (room == nullptr) {
        return false;
    }
    const BOUNDS_32 bounds = Room_GetRoomBounds(room);
    const int32_t x1 = bounds.min.x;
    const int32_t y1 = bounds.min.y;
    const int32_t z1 = bounds.min.z;
    const int32_t x2 = bounds.max.x;
    const int32_t y2 = bounds.max.y;
    const int32_t z2 = bounds.max.z;
    if (point.x >= x1 && point.x < x2 && point.y >= y1 && point.y <= y2
        && point.z >= z1 && point.z < z2) {
        const SECTOR *sector = Room_GetWorldSector(room, point.x, point.z);
        const int32_t height = Room_GetHeight(sector, point);
        if (height != NO_HEIGHT) {
            return true;
        }
    }
    return false;
}

int16_t Room_GetIndexFromPos(const XYZ_32 point)
{
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const ROOM *const room = Room_Get(i);
        if (room->flip_status == RFS_FLIPPED) {
            continue;
        }
        if (Room_PointInside(room, point)) {
            return i;
        }
    }

    return NO_ROOM;
}

int32_t Room_GetFlippedBaseRoom(const int32_t room_num)
{
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const ROOM *const room = Room_Get(i);
        if (room->flipped_room == room_num) {
            return i;
        }
    }
    return NO_ROOM;
}

BOUNDS_32 Room_GetWorldBounds(void)
{
    BOUNDS_32 world_bounds = {
        .min.x = INT32_MAX,
        .min.z = INT32_MAX,
        .max.x = 0,
        .max.z = 0,
        .min.y = MAX_HEIGHT,
        .max.y = -MAX_HEIGHT,
    };
    for (int32_t i = 0; i < Room_GetCount(); i++) {
        const BOUNDS_32 room_bounds = Room_GetRoomBounds(Room_Get(i));
        world_bounds.min.x = MIN(world_bounds.min.x, room_bounds.min.x);
        world_bounds.max.x = MAX(world_bounds.max.x, room_bounds.max.x);
        world_bounds.min.z = MIN(world_bounds.min.z, room_bounds.min.z);
        world_bounds.max.z = MAX(world_bounds.max.z, room_bounds.max.z);
        world_bounds.min.y = MIN(world_bounds.min.y, room_bounds.min.y);
        world_bounds.max.y = MAX(world_bounds.max.y, room_bounds.max.y);
    }
    return world_bounds;
}

void Room_GetNearbyRooms(
    const XYZ_32 pos, const int32_t r, const int32_t h, const int16_t room_num)
{
    Room_DrawReset();
    Room_MarkToBeDrawn(room_num);

    M_GetNewRoom(pos.x + r, pos.y, pos.z + r, room_num);
    M_GetNewRoom(pos.x - r, pos.y, pos.z + r, room_num);
    M_GetNewRoom(pos.x + r, pos.y, pos.z - r, room_num);
    M_GetNewRoom(pos.x - r, pos.y, pos.z - r, room_num);
    M_GetNewRoom(pos.x + r, pos.y - h, pos.z + r, room_num);
    M_GetNewRoom(pos.x - r, pos.y - h, pos.z + r, room_num);
    M_GetNewRoom(pos.x + r, pos.y - h, pos.z - r, room_num);
    M_GetNewRoom(pos.x - r, pos.y - h, pos.z - r, room_num);
}

bool Room_CheckOverlap(const int16_t room_num_0, const int16_t room_num_1)
{
    const BOUNDS_32 room_0_bounds = Room_GetRoomBounds(Room_Get(room_num_0));
    const BOUNDS_32 room_1_bounds = Room_GetRoomBounds(Room_Get(room_num_1));

    // clang-format off
    return (
        room_0_bounds.min.x <= room_1_bounds.max.x &&
        room_0_bounds.max.x >= room_1_bounds.min.x &&
        room_0_bounds.min.y <= room_1_bounds.max.y &&
        room_0_bounds.max.y >= room_1_bounds.min.y &&
        room_0_bounds.min.z <= room_1_bounds.max.z &&
        room_0_bounds.max.z >= room_1_bounds.min.z);
    // clang-format on
}

bool Room_FindValidPos(XYZ_32 *const out_pos, int16_t *const out_room_num)
{
    ASSERT(out_pos != nullptr);
    ASSERT(out_room_num != nullptr);
    XYZ_32 initial_pos = *out_pos;
    int16_t room_num = *out_room_num;
    if (room_num == NO_ROOM) {
        room_num = Room_GetIndexFromPos(*out_pos);
    }
    if (room_num == NO_ROOM) {
        return false;
    }

    const ROOM *const room = Room_Get(room_num);
    if (room->flip_status == RFS_FLIPPED && Room_GetFlipStatus()) {
        room_num = Room_GetFlippedBaseRoom(room_num);
        if (room_num == NO_ROOM) {
            return false;
        }
    }

    const SECTOR *sector = Room_GetSector(*out_pos, &room_num);
    int32_t height = Room_GetHeight(sector, *out_pos);

    int32_t x = out_pos->x;
    int32_t y = out_pos->y;
    int32_t z = out_pos->z;
    if (height == NO_HEIGHT) {
        // Sample a sphere of points around target x, y, z
        // and teleport to the first available location.
        VECTOR *const points = Vector_Create(sizeof(XYZ_32));

        const int32_t radius = 10;
        const int32_t unit = STEP_L;
        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                if (SQUARE(dx) + SQUARE(dz) > SQUARE(radius)) {
                    continue;
                }

                const XYZ_32 point = {
                    .x = ROUND_TO_SECTOR(x + dx * unit) + WALL_L / 2,
                    .y = y,
                    .z = ROUND_TO_SECTOR(z + dz * unit) + WALL_L / 2,
                };
                sector = Room_GetSector(point, &room_num);
                height = Room_GetHeightEx(sector, point, true, NO_ITEM);
                if (height == NO_HEIGHT) {
                    continue;
                }
                Vector_Add(points, (void *)&point);
            }
        }

        int32_t best_distance = INT32_MAX;
        for (int32_t i = 0; i < points->count; i++) {
            const XYZ_32 *const point = (const XYZ_32 *)Vector_Get(points, i);
            const int32_t distance = XYZ_32_GetDistance(*point, initial_pos);
            if (distance < best_distance) {
                best_distance = distance;
                x = point->x;
                y = point->y;
                z = point->z;
            }
        }

        Vector_Free(points);
        if (best_distance == INT32_MAX) {
            return false;
        }
    }

    out_pos->x = x;
    out_pos->y = y;
    out_pos->z = z;
    *out_room_num = room_num;

    return true;
}

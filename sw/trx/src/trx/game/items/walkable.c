#include <trx/game/items/walkable.h>

#include <trx/core/log.h>
#include <trx/debug.h>
#include <trx/game/game_buf.h>
#include <trx/game/items.h>
#include <trx/game/objects/vars.h>
#include <trx/game/rooms.h>

#include <stdlib.h>

#define M_QUADRANT_COUNT 4

typedef struct {
    SECTOR *sectors[M_QUADRANT_COUNT];
    int32_t count;
} M_CANDIDATE_SECTORS;

typedef struct {
    WALKABLE *nodes;
    int32_t capacity;
    int32_t active_count;
} M_SETUP;

static M_SETUP *m_Setup = nullptr;
static int32_t m_SetupCount = 0;

static SECTOR *M_GetItemPitSector(const XYZ_32 pos, int16_t room_num)
{
    SECTOR *const sector = Room_GetSector(pos, &room_num);
    return Room_GetPitSector(sector, pos.x, pos.z);
}

static bool M_HasCandidateSector(
    const M_CANDIDATE_SECTORS *const candidates, const SECTOR *const sector)
{
    for (int32_t i = 0; i < candidates->count; i++) {
        if (candidates->sectors[i] == sector) {
            return true;
        }
    }
    return false;
}

static M_CANDIDATE_SECTORS M_GetCandidateSectors(
    const XYZ_32 base_pos, int16_t room_num)
{
    // Probe evenly around the centre position for cases where a walkable is
    // placed above a triangle portal, so detecting the correct sector at all
    // possible positions.
    M_CANDIDATE_SECTORS candidates = { 0 };

    const XYZ_32 mid_pos = {
        .x = ROUND_TO_SECTOR(base_pos.x) + STEP_L * 2,
        .y = base_pos.y,
        .z = ROUND_TO_SECTOR(base_pos.z) + STEP_L * 2,
    };

    const XZ_32 deltas[M_QUADRANT_COUNT] = {
        { -STEP_L, 0 },
        { STEP_L, 0 },
        { 0, -STEP_L },
        { 0, STEP_L },
    };

    for (int32_t i = 0; i < M_QUADRANT_COUNT; i++) {
        const XZ_32 delta = deltas[i];
        const XYZ_32 pos = {
            .x = mid_pos.x + delta.x,
            .y = base_pos.y,
            .z = mid_pos.z + delta.z,
        };
        SECTOR *const sector = M_GetItemPitSector(pos, room_num);
        if (!M_HasCandidateSector(&candidates, sector)) {
            candidates.sectors[candidates.count] = sector;
            candidates.count++;
        }
    }

    return candidates;
}

static M_SETUP *M_GetSetup(const int16_t item_num)
{
    ASSERT(m_Setup != nullptr);
    ASSERT(item_num >= 0 && item_num < m_SetupCount);
    return &m_Setup[item_num];
}

static bool M_SectorContainsWalkable(
    const SECTOR *const sector, const int16_t item_num)
{
    const WALKABLE *walkable = sector->walkable;
    while (walkable != nullptr) {
        if (walkable->item_num == item_num) {
            return true;
        }
        walkable = walkable->next;
    }
    return false;
}

static void M_InsertSorted(WALKABLE **walkables, WALKABLE *const node)
{
    while (*walkables != nullptr && (*walkables)->pos.y >= node->pos.y) {
        walkables = &(*walkables)->next;
    }

    node->next = *walkables;
    *walkables = node;
}

static void M_Remove(
    const int16_t item_num, const XYZ_32 pos, const int16_t room_num)
{
    const ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->add_walkable_func == nullptr) {
        return;
    }

    M_SETUP *const setup = M_GetSetup(item_num);
    if (setup->capacity == 0 || setup->nodes == nullptr) {
        return;
    }

    const M_CANDIDATE_SECTORS sectors = M_GetCandidateSectors(pos, room_num);
    for (int32_t i = 0; i < sectors.count; i++) {
        SECTOR *const sector = sectors.sectors[i];
        WALKABLE *walkable = sector->walkable;
        WALKABLE *prev = nullptr;
        while (walkable != nullptr) {
            if (walkable->item_num == item_num) {
                if (prev != nullptr) {
                    prev->next = walkable->next;
                } else {
                    sector->walkable = walkable->next;
                }
                break;
            }
            prev = walkable;
            walkable = walkable->next;
        }
    }

    setup->active_count = 0;
}

void Walkable_AllocateNodes(const ITEM *const item, const int32_t footprint)
{
    const int16_t item_num = Item_GetIndex(item);
    M_SETUP *const setup = M_GetSetup(item_num);
    setup->capacity = footprint * M_QUADRANT_COUNT;
    setup->active_count = 0;
    setup->nodes = nullptr;
    if (setup->capacity > 0) {
        setup->nodes =
            GameBuf_Alloc(sizeof(WALKABLE) * setup->capacity, GBUF_WALKABLES);
    }
}

void Walkable_Add(const int16_t item_num, const XYZ_32 pos)
{
    const ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->add_walkable_func == nullptr) {
        return;
    }

    M_SETUP *const setup = M_GetSetup(item_num);
    if (setup->capacity == 0 || setup->nodes == nullptr) {
        return;
    }
    const M_CANDIDATE_SECTORS sectors =
        M_GetCandidateSectors(pos, item->room_num);

    for (int32_t i = 0; i < sectors.count; i++) {
        SECTOR *const sector = sectors.sectors[i];
        if (M_SectorContainsWalkable(sector, item_num)) {
            continue;
        }

        if (setup->active_count >= setup->capacity) {
            LOG_WARNING(
                "Walkable %d at (%d, %d, %d) has no more allocated sector "
                "nodes.",
                item_num, pos.x, pos.y, pos.z);
            break;
        }

        WALKABLE *const node = &setup->nodes[setup->active_count];
        node->item_num = item_num;
        node->pos = pos;
        node->next = nullptr;
        M_InsertSorted(&sector->walkable, node);
        setup->active_count++;
    }
}

void Walkable_Remove(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    M_Remove(item_num, item->pos, item->room_num);
}

void Walkable_Reposition(
    const int16_t item_num, const GAME_VECTOR start, const GAME_VECTOR target)
{
    M_Remove(item_num, start.pos, start.room_num);
    Walkable_Add(item_num, target.pos);
}

void Walkable_Reset(void)
{
    const int32_t room_count = Room_GetCount();
    for (int32_t room_idx = 0; room_idx < room_count; room_idx++) {
        const ROOM *const room = Room_Get(room_idx);
        const int32_t num_sectors = room->size.x * room->size.z;
        for (int32_t i = 0; i < num_sectors; i++) {
            room->sectors[i].walkable = nullptr;
        }
    }

    for (int32_t i = 0; i < m_SetupCount; i++) {
        M_SETUP *const setup = M_GetSetup(i);
        setup->active_count = 0;
    }
}

void Walkable_ResetLevel(void)
{
    Walkable_Reset();
    const int32_t item_count = Item_GetLevelCount();
    m_SetupCount = item_count;
    m_Setup = nullptr;
    if (item_count > 0) {
        m_Setup = GameBuf_Alloc(sizeof(M_SETUP) * item_count, GBUF_WALKABLES);
    }
}

void Walkable_Shutdown(void)
{
    m_Setup = nullptr;
    m_SetupCount = 0;
}

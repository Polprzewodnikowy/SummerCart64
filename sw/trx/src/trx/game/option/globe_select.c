#include <trx/game/option/globe_select.h>

#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/overlay.h>
#include <trx/game/savegame.h>

typedef struct {
    GAME_STRING_ID gs_area_id;
} M_AREA_STRING_ENTRY;

static const M_AREA_STRING_ENTRY m_AreaStrings[] = {
    { .gs_area_id = GS_ID("general/globe_select/area_1") },
    { .gs_area_id = GS_ID("general/globe_select/area_2") },
    { .gs_area_id = GS_ID("general/globe_select/area_3") },
    { .gs_area_id = GS_ID("general/globe_select/area_4") }, // Unused Peru
    { .gs_area_id = GS_ID("general/globe_select/area_5") },
    { .gs_area_id = GS_ID("general/globe_select/area_6") },
};

static int32_t M_GetEntryCount(void)
{
    return MIN(g_GameFlow.globe.count, (int32_t)ARRAY_SIZE(m_AreaStrings));
}

static const GF_GLOBE_ENTRY *M_GetEntry(const int32_t idx)
{
    const int32_t entry_count = M_GetEntryCount();
    if (idx < 0 || idx >= entry_count) {
        return nullptr;
    }
    return &g_GameFlow.globe.entries[idx];
}

static bool M_IsLevelCompleted(const int32_t level_ordinal)
{
    if (level_ordinal < 0) {
        return false;
    }
    const GF_LEVEL *const level =
        GF_GetLevelByOrdinalNumber(GFLT_MAIN, level_ordinal);
    if (level == nullptr) {
        return false;
    }
    const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    return resume != nullptr && resume->level_completed;
}

static int32_t M_GetNextSelectableIndex(
    const INV_RING *const ring, const int32_t direction)
{
    if (direction == 0) {
        return ring->globe_select.selection;
    }

    const int32_t entry_count = M_GetEntryCount();
    if (entry_count <= 0) {
        return ring->globe_select.selection;
    }

    for (int32_t step = 0; step < entry_count; step++) {
        int32_t idx = ring->globe_select.selection + direction * (step + 1);
        while (idx < 0 && entry_count != 0) {
            idx += entry_count;
        }
        idx %= entry_count;
        if (ring->globe_select.selectable[idx]) {
            return idx;
        }
    }
    return ring->globe_select.selection;
}

static bool M_UpdateRotAxis(int16_t *const cur, const int16_t target)
{
    int16_t ang = target - *cur;
    if (ang >= 128 || ang <= -128) {
        *cur += ang >> 3;
        return false;
    }
    *cur = target;
    return true;
}

static bool M_IsAligned(INV_RING *const ring)
{
    const GF_GLOBE_ENTRY *const entry =
        M_GetEntry(ring->globe_select.selection);
    if (entry == nullptr) {
        return true;
    }
    int32_t axes = 0;
    axes += M_UpdateRotAxis(&ring->globe_select.rot.x, entry->rot.x) ? 1 : 0;
    axes += M_UpdateRotAxis(&ring->globe_select.rot.y, entry->rot.y) ? 1 : 0;
    axes += M_UpdateRotAxis(&ring->globe_select.rot.z, entry->rot.z) ? 1 : 0;
    return axes == 3;
}

int32_t Option_GlobeSelect_AreaFromMeshIdx(const int32_t mesh_idx)
{
    const int32_t entry_count = M_GetEntryCount();
    for (int32_t i = 0; i < entry_count; i++) {
        if (g_GameFlow.globe.entries[i].mesh_idx == mesh_idx) {
            return (int32_t)i;
        }
    }
    return -1;
}

void Option_GlobeSelect_UpdateSelectable(INV_RING *const ring)
{
    ring->globe_select.selection = -1;
    ring->globe_select.rot.x = 0;
    ring->globe_select.rot.y = 0;
    ring->globe_select.rot.z = 0;
    ring->globe_select.meshes_drawn = 0x0FFFu;
    ring->globe_select.confirmed = false;
    for (int32_t i = 0; i < MAX_GLOBE_ZONES; i++) {
        ring->globe_select.selectable[i] = false;
        ring->globe_select.start_level_num[i] = -1;
    }

    uint32_t completed_mask = 0u;
    const int32_t entry_count = M_GetEntryCount();
    for (int32_t i = 0; i < entry_count; i++) {
        if (M_IsLevelCompleted(
                g_GameFlow.globe.entries[i].completion_level_ordinal)) {
            completed_mask |= 1u << i;
        }
    }

    int32_t selectable_count = 0;
    for (int32_t i = 0; i < entry_count; i++) {
        ring->globe_select.selectable[i] = false;
        ring->globe_select.start_level_num[i] = -1;

        const GF_GLOBE_ENTRY *const entry = &g_GameFlow.globe.entries[i];

        const GF_LEVEL *const start_level =
            GF_GetLevelByOrdinalNumber(GFLT_MAIN, entry->start_level_ordinal);
        if (start_level != nullptr) {
            ring->globe_select.start_level_num[i] = start_level->num;
        }

        if ((completed_mask & (1u << i)) != 0u) {
            continue;
        }
        if ((completed_mask & entry->prereq_mask) != entry->prereq_mask) {
            continue;
        }

        if (start_level == nullptr) {
            continue;
        }

        ring->globe_select.selectable[i] = true;
        selectable_count++;
    }

    ring->globe_select.meshes_drawn = 0x0FFFu;
    for (int32_t i = 0; i < entry_count; i++) {
        if (ring->globe_select.start_level_num[i] < 0) {
            ring->globe_select.meshes_drawn &=
                ~(1 << g_GameFlow.globe.entries[i].mesh_idx);
        }
    }

    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCL, selectable_count > 1);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCR, selectable_count > 1);

    if (ring->globe_select.selection < 0
        || ring->globe_select.selection >= entry_count
        || !ring->globe_select.selectable[ring->globe_select.selection]) {
        ring->globe_select.selection = -1;
        for (int32_t i = 0; i < entry_count; i++) {
            if (ring->globe_select.selectable[i]) {
                ring->globe_select.selection = i;
                break;
            }
        }
    }
}

void Option_GlobeSelect_Control(
    INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    INV_RING *const ring = InvRing_GetActiveRing();
    if (ring == nullptr || ring->mode != INV_GLOBE_SELECT_MODE) {
        return;
    }

    Overlay_SetTopText((OVERLAY_TEXT) {
        .kind = UI_OVERLAY_TEXT_GS_KEY,
        .fmt_gs_key = GS_ID("general/inventory_ring/heading_fmt"),
        .literal = GS_ID("general/inventory_ring/heading_adventure"),
    });

    if (ring->globe_select.selection < 0) {
        Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
        ring->globe_select.confirmed = false;
        return;
    }

    const bool aligned = M_IsAligned(ring);

    if (aligned && !is_busy) {
        if (g_Input.menu_left) {
            ring->globe_select.selection = M_GetNextSelectableIndex(ring, -1);
        } else if (g_Input.menu_right) {
            ring->globe_select.selection = M_GetNextSelectableIndex(ring, 1);
        }
    }

    const int32_t entry_count = M_GetEntryCount();
    if (ring->globe_select.selection >= 0
        && ring->globe_select.selection < entry_count
        && ring->globe_select.selection < (int32_t)ARRAY_SIZE(m_AreaStrings)) {
        Overlay_SetBottomText((OVERLAY_TEXT) {
            .kind = UI_OVERLAY_TEXT_GS_KEY,
            .fmt_gs_key = GS_ID("general/inventory_ring/object_name_fmt"),
            .literal = m_AreaStrings[ring->globe_select.selection].gs_area_id,
        });
    } else {
        Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
    }

    if (g_InputDB.menu_confirm && !is_busy) {
        if (!aligned) {
            g_InputDB.menu_confirm = false;
            return;
        }
        ring->globe_select.confirmed = true;
    }
}

void Option_GlobeSelect_Draw(INVENTORY_ITEM *const inv_item)
{
}

void Option_GlobeSelect_Close(void)
{
}

void Option_GlobeSelect_Shutdown(void)
{
}

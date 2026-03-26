#include <trx/config.h>
#include <trx/core/benchmark.h>
#include <trx/core/enum_map.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/gun.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/objects/traps/movable_block.h>
#include <trx/game/pathing/lot.h>
#include <trx/game/savegame.h>
#include <trx/game/savegame/file.h>
#include <trx/game/shell.h>
#include <trx/version.h>

#include <stdio.h>
#include <string.h>

static SAVEGAME_VERSION m_InitialVersion = SG_VERSION_LEGACY;
static SAVEGAME_INFO *m_NormalSavegameInfo = nullptr;
static SAVEGAME_INFO *m_QuickSavegameInfo = nullptr;
static RESUME_INFO *m_ResumeInfo = nullptr;
static int32_t m_SaveSlots = 0;
static int32_t m_QuickSaveSlots = 0;
static int32_t m_SavedGames = 0;
static int32_t m_SaveCounter = 0;
static int32_t m_NextQuickSlot = 0;
static SAVEGAME_SLOT_REF m_MostRecentlyUsedSlot = { .index = -1 };
static SAVEGAME_SLOT_REF m_MostRecentlyCreatedSlot = { .index = -1 };
static SAVEGAME_SLOT_REF m_BoundSlot = { .index = -1 };

static const char *M_GetSaveWriteDir(void)
{
    const char *const saves_dir = TRXPath_Get(TRX_PATH_SAVES_DIR);
    const SHELL_ARGS *const args = Shell_GetArgs();
    if (args != nullptr && args->mod != nullptr && args->mod->name != nullptr) {
        return String_FormatStatic("%s/%s", saves_dir, args->mod->name);
    }
    return saves_dir;
}

static char *M_GetSaveWritePath(const char *const file_name)
{
    ASSERT(file_name != nullptr);
    return String_Format("%s/%s", M_GetSaveWriteDir(), file_name);
}

static SAVEGAME_INFO *M_GetSavegameInfoSlot(const SAVEGAME_SLOT_REF slot)
{
    switch (slot.pool) {
    case SAVEGAME_SLOT_POOL_NORMAL:
        if (slot.index >= 0 && slot.index < m_SaveSlots) {
            return &m_NormalSavegameInfo[slot.index];
        }
        break;
    case SAVEGAME_SLOT_POOL_QUICK:
        if (slot.index >= 0 && slot.index < m_QuickSaveSlots) {
            return &m_QuickSavegameInfo[slot.index];
        }
        break;
    case SAVEGAME_SLOT_POOL_NUMBER_OF:
        break;
    }
    return nullptr;
}

static const char *M_GetSaveFilePatternForPool(const SAVEGAME_SLOT_POOL pool)
{
    switch (pool) {
    case SAVEGAME_SLOT_POOL_NORMAL:
        return SG_File_GetSaveFilePattern();
    case SAVEGAME_SLOT_POOL_QUICK:
        return SG_File_GetQuickSaveFilePattern();
    case SAVEGAME_SLOT_POOL_NUMBER_OF:
        break;
    }
    return nullptr;
}

static void M_CopyResumeInfo(
    RESUME_INFO *const target, const RESUME_INFO *const source)
{
    memcpy(target, source, sizeof(RESUME_INFO));
}

static void M_ClearSlot(SAVEGAME_INFO *const savegame_info)
{
    savegame_info->counter = -1;
    savegame_info->level_num = -1;
    savegame_info->is_quick = false;
    Memory_FreePointer(&savegame_info->full_path);
    Memory_FreePointer(&savegame_info->level_title);
}

static void M_ClearSlots(void)
{
    if (m_NormalSavegameInfo != nullptr) {
        for (int32_t i = 0; i < m_SaveSlots; i++) {
            M_ClearSlot(&m_NormalSavegameInfo[i]);
        }
    }
    if (m_QuickSavegameInfo != nullptr) {
        for (int32_t i = 0; i < m_QuickSaveSlots; i++) {
            M_ClearSlot(&m_QuickSavegameInfo[i]);
        }
    }
}

static bool M_FillSlot(const SAVEGAME_SLOT_REF slot, const char *const path)
{
    SAVEGAME_INFO *const savegame_info = M_GetSavegameInfoSlot(slot);
    if (savegame_info == nullptr) {
        return false;
    }
    bool result = false;
    MYFILE *const fp = File_Open(path, FILE_OPEN_READ);
    if (fp != nullptr) {
        SAVEGAME_INFO tmp_savegame_info;
        if (SG_File_FillInfo(fp, &tmp_savegame_info)) {
            M_ClearSlot(savegame_info);
            *savegame_info = tmp_savegame_info;
            savegame_info->is_quick = slot.pool == SAVEGAME_SLOT_POOL_QUICK;
            savegame_info->full_path = Memory_DupStr(path);
            result = true;
        }
        File_Close(fp);
    }
    return result;
}

static void M_ScanSavedGamesDir(const char *const dir_path)
{
    void *const dir_handle = File_OpenDirectory(dir_path);
    if (dir_handle == nullptr) {
        return;
    }

    while (true) {
        const char *const file_name = File_ReadDirectory(dir_handle);
        if (file_name == nullptr) {
            break;
        }
        if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
            continue;
        }

        char *file_name_ci = String_ToUpper(file_name);
        for (SAVEGAME_SLOT_POOL pool = 0; pool < SAVEGAME_SLOT_POOL_NUMBER_OF;
             pool++) {
            const char *const pattern = M_GetSaveFilePatternForPool(pool);
            char *pattern_ci = String_ToUpperPattern(pattern);
            int32_t slot_idx = -1;
            const int32_t parsed = sscanf(file_name_ci, pattern_ci, &slot_idx);
            Memory_FreePointer(&pattern_ci);

            if (parsed != 1 || slot_idx < 0
                || slot_idx >= Savegame_GetSlotCount(pool)) {
                continue;
            }

            char *file_path = String_Format("%s/%s", dir_path, file_name);
            M_FillSlot(
                (SAVEGAME_SLOT_REF) { .pool = pool, .index = slot_idx },
                file_path);
            Memory_FreePointer(&file_path);
            break;
        }
        Memory_FreePointer(&file_name_ci);
    }

    File_CloseDirectory(dir_handle);
}

static void M_LoadPreprocess(void)
{
    Savegame_InitCurrentInfo();
}

static void M_LoadPostprocess(void)
{
    // TODO: tidy this; skidoo drivers currently require handle_save_func to be
    // called immediately on load within the strategies.
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        ITEM *const item = Item_Get(i);
        const OBJECT *const obj = Object_Get(item->object_id);

        if (obj->save_position && (obj->shadow_size != 0 || obj->load_floor)) {
            int16_t room_num = item->room_num;
            const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
            item->floor = Room_GetHeight(sector, item->pos);
        }

        if (obj->save_flags != 0) {
            item->flags &= 0xFF00;
        }
        // TODO: make this engine-agnostic
        if (g_TRVersion == 1 && obj->handle_save_func != nullptr) {
            obj->handle_save_func(item, SAVEGAME_STAGE_AFTER_LOAD);
        }
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (Game_GetBonusFlag() != GBF_NONE) {
        g_Config.profile.new_game_plus_unlock = true;
        Config_Update();
    }
    if (lara->burn && !g_Config.gameplay.enable_enhanced_saves) {
        lara->burn = false;
        Lara_CatchFire();
    }
}

static void M_DetermineLegacyGunTypes(RESUME_INFO *const resume)
{
    // Fallback logic to figure out holster and back gun items for saves from
    // TR1X 4.2 and earlier (including TombATI) and TR2X 1.2 and earlier, where
    // these values are missing. Make educated guesses based on the type of gun
    // equipped.
    if (resume->holsters_gun_type == LGT_UNKNOWN) {
        switch (resume->equipped_gun_type) {
        case LGT_PISTOLS:
        case LGT_MAGNUMS:
        case LGT_AUTOS:
        case LGT_DESERT_EAGLE:
        case LGT_UZIS:
            resume->holsters_gun_type = resume->equipped_gun_type;
            break;
        case LGT_SHOTGUN:
        case LGT_M16:
        case LGT_MP5:
        case LGT_GRENADE:
        case LGT_ROCKET:
        case LGT_HARPOON:
            if (resume->flags.has_pistols) {
                resume->holsters_gun_type = LGT_PISTOLS;
            } else if (resume->flags.has_magnums) {
                resume->holsters_gun_type = LGT_MAGNUMS;
            } else if (resume->flags.has_autos) {
                resume->holsters_gun_type = LGT_AUTOS;
            } else if (resume->flags.has_desert_eagle) {
                resume->holsters_gun_type = LGT_DESERT_EAGLE;
            } else if (resume->flags.has_uzis) {
                resume->holsters_gun_type = LGT_UZIS;
            } else {
                resume->holsters_gun_type = LGT_UNARMED;
            }
            break;
        default:
            resume->holsters_gun_type = LGT_UNARMED;
            break;
        }
    }
    if (resume->back_gun_type == LGT_UNKNOWN) {
        resume->back_gun_type = LGT_UNARMED;
        if (resume->flags.has_shotgun) {
            resume->back_gun_type = LGT_SHOTGUN;
        } else if (resume->flags.has_m16) {
            resume->back_gun_type = LGT_M16;
        } else if (resume->flags.has_mp5) {
            resume->back_gun_type = LGT_MP5;
        } else if (resume->flags.has_grenade) {
            resume->back_gun_type = LGT_GRENADE;
        } else if (resume->flags.has_rocket) {
            resume->back_gun_type = LGT_ROCKET;
        } else if (resume->flags.has_harpoon) {
            resume->back_gun_type = LGT_HARPOON;
        }
    }
}

SAVEGAME_VERSION Savegame_GetInitialVersion(void)
{
    return m_InitialVersion;
}

void Savegame_SetInitialVersion(const SAVEGAME_VERSION version)
{
    m_InitialVersion = version;
}

void Savegame_BindSlot(const SAVEGAME_SLOT_REF slot)
{
    if (!Savegame_IsValidSlotRef(slot)) {
        m_BoundSlot = Savegame_InvalidSlot();
        return;
    }
    m_BoundSlot = slot;
    m_MostRecentlyUsedSlot = slot;
    LOG_DEBUG("Binding save slot %d:%d", slot.pool, slot.index);
}

SAVEGAME_SLOT_REF Savegame_GetMostRecentlyUsedSlot(void)
{
    return m_MostRecentlyUsedSlot;
}

void Savegame_UnbindSlot(void)
{
    LOG_DEBUG("Resetting the save slot");
    m_BoundSlot = Savegame_InvalidSlot();
}

SAVEGAME_SLOT_REF Savegame_GetBoundSlot(void)
{
    return m_BoundSlot;
}

int32_t Savegame_GetLevelNumber(const SAVEGAME_SLOT_REF slot)
{
    const SAVEGAME_INFO *const info = Savegame_GetSavegameInfo(slot);
    return info != nullptr ? info->level_num : -1;
}

bool Savegame_IsSlotFree(const SAVEGAME_SLOT_REF slot)
{
    const SAVEGAME_INFO *const info = Savegame_GetSavegameInfo(slot);
    return info == nullptr || info->level_num == -1;
}

int32_t Savegame_GetCounter(void)
{
    return m_SaveCounter;
}

int32_t Savegame_GetTotalCount(void)
{
    return m_SavedGames;
}

SAVEGAME_SLOT_REF Savegame_GetMostRecentlyCreatedSlot(void)
{
    return m_MostRecentlyCreatedSlot;
}

SAVEGAME_SLOT_REF Savegame_NormalSlot(const int32_t index)
{
    return (SAVEGAME_SLOT_REF) {
        .pool = SAVEGAME_SLOT_POOL_NORMAL,
        .index = index,
    };
}

SAVEGAME_SLOT_REF Savegame_QuickSlot(const int32_t index)
{
    return (SAVEGAME_SLOT_REF) {
        .pool = SAVEGAME_SLOT_POOL_QUICK,
        .index = index,
    };
}

SAVEGAME_SLOT_REF Savegame_InvalidSlot(void)
{
    return (SAVEGAME_SLOT_REF) {
        .pool = SAVEGAME_SLOT_POOL_NORMAL,
        .index = -1,
    };
}

bool Savegame_IsValidSlotRef(const SAVEGAME_SLOT_REF slot)
{
    return slot.pool >= SAVEGAME_SLOT_POOL_NORMAL
        && slot.pool < SAVEGAME_SLOT_POOL_NUMBER_OF && slot.index >= 0
        && slot.index < Savegame_GetSlotCount(slot.pool);
}

int32_t Savegame_SlotToParam(const SAVEGAME_SLOT_REF slot)
{
    if (!Savegame_IsValidSlotRef(slot)) {
        return -1;
    }
    const uint32_t packed = ((uint32_t)slot.pool << 31) | (uint32_t)slot.index;
    return (int32_t)packed;
}

SAVEGAME_SLOT_REF Savegame_SlotFromParam(const int32_t param)
{
    if (param == -1) {
        return Savegame_InvalidSlot();
    }

    const uint32_t packed = (uint32_t)param;
    const SAVEGAME_SLOT_POOL pool = (packed >> 31) & 1;
    const int32_t index = (int32_t)(packed & 0x7FFFFFFF);
    return (SAVEGAME_SLOT_REF) {
        .pool = pool,
        .index = index,
    };
}

void Savegame_Init(void)
{
    m_ResumeInfo = Memory_Alloc(
        sizeof(RESUME_INFO)
        * (GF_GetLevelTable(GFLT_MAIN)->count
           + GF_GetLevelTable(GFLT_DEMOS)->count));

    m_SaveSlots = Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_NORMAL);
    m_QuickSaveSlots = Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_QUICK);
    m_NormalSavegameInfo = Memory_Alloc(sizeof(SAVEGAME_INFO) * m_SaveSlots);
    m_QuickSavegameInfo = m_QuickSaveSlots > 0
        ? Memory_Alloc(sizeof(SAVEGAME_INFO) * m_QuickSaveSlots)
        : nullptr;

    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_DEMOS);
    for (int32_t i = 0; i < level_table->count; i++) {
        RESUME_INFO *const resume_info =
            Savegame_GetCurrentInfo(&level_table->levels[i]);
        resume_info->lara_hitpoints = LARA_MAX_HITPOINTS;
        resume_info->flags.available = true;
        resume_info->flags.has_pistols = true;
        resume_info->pistol_ammo = 1000;
        resume_info->gun_status = LGS_ARMLESS;
        resume_info->equipped_gun_type = LGT_PISTOLS;
        resume_info->holsters_gun_type = LGT_PISTOLS;
        resume_info->back_gun_type = LGT_UNARMED;
        resume_info->prev_level = -1;
    }
}

bool Savegame_IsInitialised(void)
{
    return m_NormalSavegameInfo != nullptr;
}

void Savegame_Shutdown(void)
{
    M_ClearSlots();
    Memory_FreePointer(&m_ResumeInfo);
    Memory_FreePointer(&m_NormalSavegameInfo);
    Memory_FreePointer(&m_QuickSavegameInfo);
}

int32_t Savegame_GetSlotCount(const SAVEGAME_SLOT_POOL pool)
{
    switch (pool) {
    case SAVEGAME_SLOT_POOL_NORMAL:
        return g_Config.gameplay.maximum_save_slots;
    case SAVEGAME_SLOT_POOL_QUICK:
        return g_Config.gameplay.maximum_quick_save_slots;
    case SAVEGAME_SLOT_POOL_NUMBER_OF:
        break;
    }
    return 0;
}

SAVEGAME_SLOT_REF Savegame_GetNextQuickSlot(void)
{
    if (m_QuickSaveSlots <= 0) {
        return Savegame_InvalidSlot();
    }
    if (m_NextQuickSlot < 0 || m_NextQuickSlot >= m_QuickSaveSlots) {
        m_NextQuickSlot = 0;
    }
    return Savegame_QuickSlot(m_NextQuickSlot);
}

static bool M_IsQuickSlotSortedBefore(
    const SAVEGAME_SLOT_REF left, const SAVEGAME_SLOT_REF right)
{
    const SAVEGAME_INFO *const left_info = Savegame_GetSavegameInfo(left);
    const SAVEGAME_INFO *const right_info = Savegame_GetSavegameInfo(right);
    if (left_info == nullptr || right_info == nullptr) {
        return false;
    }
    if (left_info->counter != right_info->counter) {
        return left_info->counter > right_info->counter;
    }
    return left.index < right.index;
}

int32_t Savegame_GetQuickVisualCount(void)
{
    int32_t count = 0;
    const int32_t quick_slot_count =
        Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_QUICK);
    for (int32_t i = 0; i < quick_slot_count; i++) {
        if (!Savegame_IsSlotFree(Savegame_QuickSlot(i))) {
            count++;
        }
    }
    return count;
}

SAVEGAME_SLOT_REF Savegame_QuickFromVisualIndex(const int32_t visual_index)
{
    if (visual_index < 0) {
        return Savegame_InvalidSlot();
    }

    const int32_t quick_slot_count =
        Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_QUICK);
    for (int32_t i = 0; i < quick_slot_count; i++) {
        const SAVEGAME_SLOT_REF candidate = Savegame_QuickSlot(i);
        if (Savegame_IsSlotFree(candidate)) {
            continue;
        }

        int32_t better_count = 0;
        for (int32_t j = 0; j < quick_slot_count; j++) {
            const SAVEGAME_SLOT_REF other = Savegame_QuickSlot(j);
            if (Savegame_IsSlotFree(other)) {
                continue;
            }
            if (M_IsQuickSlotSortedBefore(other, candidate)) {
                better_count++;
            }
        }

        if (better_count == visual_index) {
            return candidate;
        }
    }

    return Savegame_InvalidSlot();
}

int32_t Savegame_QuickToVisualIndex(const SAVEGAME_SLOT_REF slot)
{
    if (!Savegame_IsValidSlotRef(slot) || slot.pool != SAVEGAME_SLOT_POOL_QUICK
        || Savegame_IsSlotFree(slot)) {
        return -1;
    }

    const int32_t quick_slot_count =
        Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_QUICK);
    int32_t better_count = 0;
    for (int32_t i = 0; i < quick_slot_count; i++) {
        const SAVEGAME_SLOT_REF other = Savegame_QuickSlot(i);
        if (Savegame_IsSlotFree(other)) {
            continue;
        }
        if (M_IsQuickSlotSortedBefore(other, slot)) {
            better_count++;
        }
    }
    return better_count;
}

RESUME_INFO *Savegame_GetCurrentInfo(const GF_LEVEL *const level)
{
    ASSERT(m_ResumeInfo != nullptr);
    if (level == nullptr) {
        return nullptr;
    }
    if (GF_GetLevelTableType(level->type) == GFLT_MAIN) {
        return &m_ResumeInfo[level->num];
    } else if (level->type == GFL_DEMO) {
        return &m_ResumeInfo[GF_GetLevelTable(GFLT_MAIN)->count];
    } else if (level->type == GFL_CUTSCENE || level->type == GFL_TITLE) {
        return nullptr;
    }
    LOG_WARNING(
        "Warning: unable to get resume info for level %d (type=%s)", level->num,
        ENUM_MAP_TO_STRING(GF_LEVEL_TYPE, level->type));
    return nullptr;
}

void Savegame_SetCurrentInfo(const int32_t current_slot, const int32_t src_slot)
{
    m_ResumeInfo[current_slot] = m_ResumeInfo[src_slot];
}

const SAVEGAME_INFO *Savegame_GetSavegameInfo(const SAVEGAME_SLOT_REF slot)
{
    return M_GetSavegameInfoSlot(slot);
}

void Savegame_InitCurrentInfo(void)
{
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        Savegame_ResetCurrentInfo(level);
        Savegame_ApplyLogicToCurrentInfo(level);
        RESUME_INFO *const current = Savegame_GetCurrentInfo(level);
        current->level_completed = false;
        current->flags.available = false;
    }

    if (GF_GetGymLevel() != nullptr) {
        Savegame_GetCurrentInfo(GF_GetGymLevel())->flags.available = true;
    }
    if (GF_GetFirstLevel() != nullptr) {
        Savegame_GetCurrentInfo(GF_GetFirstLevel())->flags.available = true;
    }
}

void Savegame_ResetCurrentInfo(const GF_LEVEL *const level)
{
    LOG_INFO("Resetting resume info for level #%d", level->num);
    RESUME_INFO *const current = Savegame_GetCurrentInfo(level);
    *current = (RESUME_INFO) { .prev_level = -1, .level_completed = false };
}

void Savegame_CarryCurrentInfoToNextLevel(
    const GF_LEVEL *const src_level, const GF_LEVEL *const dst_level)
{
    LOG_INFO(
        "Copying resume info from level #%d to level #%d", src_level->num,
        dst_level->num);
    RESUME_INFO *const src_resume = Savegame_GetCurrentInfo(src_level);
    RESUME_INFO *const dst_resume = Savegame_GetCurrentInfo(dst_level);
    if (src_resume != nullptr && dst_resume != nullptr) {
        const bool dst_level_completed = dst_resume->level_completed;
        M_CopyResumeInfo(dst_resume, src_resume);
        dst_resume->level_completed = dst_level_completed;
        dst_resume->prev_level = src_level->num;
    }
}

void Savegame_PersistGameToCurrentInfo(const GF_LEVEL *const level)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (resume == nullptr) {
        return;
    }

    resume->flags.available = true;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    if (lara_item != nullptr) {
        resume->lara_hitpoints = lara_item->hit_points;
    }
    resume->small_medipacks = Inv_RequestItem(O_SMALL_MEDIPACK_ITEM);
    resume->large_medipacks = Inv_RequestItem(O_LARGE_MEDIPACK_ITEM);

    resume->pistol_ammo = 1000;
    if (Inv_RequestItem(O_PISTOL_ITEM)) {
        resume->flags.has_pistols = true;
    } else {
        resume->flags.has_pistols = false;
    }

    if (Inv_RequestItem(O_SHOTGUN_ITEM)) {
        resume->flags.has_shotgun = true;
        resume->shotgun_ammo = lara->shotgun_ammo.ammo;
    } else {
        resume->flags.has_shotgun = false;
        resume->shotgun_ammo = Inv_RequestItem(O_SHOTGUN_AMMO_ITEM)
            * Gun_GetAmmoQuantity(LGT_SHOTGUN);
    }

    if (Inv_RequestItem(O_MAGNUM_ITEM)) {
        resume->flags.has_magnums = true;
        resume->magnum_ammo = lara->magnum_ammo.ammo;
    } else {
        resume->flags.has_magnums = false;
        resume->magnum_ammo = Inv_RequestItem(O_MAGNUM_AMMO_ITEM)
            * Gun_GetAmmoQuantity(LGT_MAGNUMS);
    }

    if (Inv_RequestItem(O_AUTOS_ITEM)) {
        resume->flags.has_autos = true;
        resume->autos_ammo = lara->autos_ammo.ammo;
    } else {
        resume->flags.has_autos = false;
        resume->autos_ammo =
            Inv_RequestItem(O_AUTOS_AMMO_ITEM) * Gun_GetAmmoQuantity(LGT_AUTOS);
    }

    if (Inv_RequestItem(O_DESERT_EAGLE_ITEM)) {
        resume->flags.has_desert_eagle = true;
        resume->desert_eagle_ammo = lara->desert_eagle_ammo.ammo;
    } else {
        resume->flags.has_desert_eagle = false;
        resume->desert_eagle_ammo = Inv_RequestItem(O_DESERT_EAGLE_AMMO_ITEM)
            * Gun_GetAmmoQuantity(LGT_DESERT_EAGLE);
    }

    if (Inv_RequestItem(O_UZI_ITEM)) {
        resume->flags.has_uzis = true;
        resume->uzi_ammo = lara->uzi_ammo.ammo;
    } else {
        resume->flags.has_uzis = false;
        resume->uzi_ammo =
            Inv_RequestItem(O_UZI_AMMO_ITEM) * Gun_GetAmmoQuantity(LGT_UZIS);
    }

    resume->flares = Inv_RequestItem(O_FLARE_ITEM);
    resume->num_scions = Inv_RequestItem(O_SCION_ITEM_1);
    resume->num_quest_item_1 = Inv_RequestItem(O_QUEST_ITEM_1);
    resume->num_quest_item_2 = Inv_RequestItem(O_QUEST_ITEM_2);
    resume->num_quest_item_3 = Inv_RequestItem(O_QUEST_ITEM_3);
    resume->num_quest_item_4 = Inv_RequestItem(O_QUEST_ITEM_4);

    if (Inv_RequestItem(O_M16_ITEM)) {
        resume->flags.has_m16 = true;
        resume->m16_ammo = lara->m16_ammo.ammo;
    } else {
        resume->flags.has_m16 = false;
        resume->m16_ammo =
            Inv_RequestItem(O_M16_AMMO_ITEM) * Gun_GetAmmoQuantity(LGT_M16);
    }

    if (Inv_RequestItem(O_MP5_ITEM)) {
        resume->flags.has_mp5 = true;
        resume->mp5_ammo = lara->mp5_ammo.ammo;
    } else {
        resume->flags.has_mp5 = false;
        resume->mp5_ammo =
            Inv_RequestItem(O_MP5_AMMO_ITEM) * Gun_GetAmmoQuantity(LGT_MP5);
    }

    if (Inv_RequestItem(O_HARPOON_ITEM)) {
        resume->flags.has_harpoon = true;
        resume->harpoon_ammo = lara->harpoon_ammo.ammo;
    } else {
        resume->flags.has_harpoon = false;
        resume->harpoon_ammo = Inv_RequestItem(O_HARPOON_AMMO_ITEM)
            * Gun_GetAmmoQuantity(LGT_HARPOON);
    }

    if (Inv_RequestItem(O_GRENADE_GUN_ITEM)) {
        resume->flags.has_grenade = true;
        resume->grenade_ammo = lara->grenade_ammo.ammo;
    } else {
        resume->flags.has_grenade = false;
        resume->grenade_ammo = Inv_RequestItem(O_GRENADE_AMMO_ITEM)
            * Gun_GetAmmoQuantity(LGT_GRENADE);
    }

    if (Inv_RequestItem(O_ROCKET_GUN_ITEM)) {
        resume->flags.has_rocket = true;
        resume->rocket_ammo = lara->rocket_ammo.ammo;
    } else {
        resume->flags.has_rocket = false;
        resume->rocket_ammo = Inv_RequestItem(O_ROCKET_AMMO_ITEM)
            * Gun_GetAmmoQuantity(LGT_ROCKET);
    }

    resume->equipped_gun_type = lara->last_gun_type;
    resume->holsters_gun_type = lara->holsters_gun_type;
    resume->back_gun_type = lara->back_gun_type;
    if (resume->back_gun_type == LGT_UNARMED
        && Gun_IsRifleType(resume->equipped_gun_type)
        && Inv_RequestItem(Gun_GetGunObject(resume->equipped_gun_type)) != 0) {
        // If a rifle is currently drawn, Lara's back mesh is temporarily
        // unarmed. Preserve the preferred rifle for next-level mesh restore.
        resume->back_gun_type = resume->equipped_gun_type;
    }
    if (lara->gun_status == LGS_READY) {
        resume->gun_status = LGS_READY;
    } else {
        resume->gun_status = LGS_ARMLESS;
    }
}

void Savegame_ApplyLogicToCurrentInfo(const GF_LEVEL *const level)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
    if (resume == nullptr) {
        return;
    }

    LOG_INFO("Applying game logic to level #%d", level->num);

    if (!g_Config.gameplay.disable_healing_between_levels
        || level == GF_GetGymLevel() || level == GF_GetFirstLevel()) {
        resume->lara_hitpoints = g_Config.gameplay.start_lara_hitpoints;
    }

    if (level == GF_GetGymLevel()) {
        resume->flags.available = true;
        resume->flags.costume = g_TRVersion == 1;

        resume->flags.has_pistols = false;
        resume->flags.has_shotgun = false;
        resume->flags.has_magnums = false;
        resume->flags.has_autos = false;
        resume->flags.has_desert_eagle = false;
        resume->flags.has_uzis = false;
        resume->flags.has_harpoon = false;
        resume->flags.has_m16 = false;
        resume->flags.has_mp5 = false;
        resume->flags.has_grenade = false;
        resume->flags.has_rocket = false;

        resume->pistol_ammo = 0;
        resume->shotgun_ammo = 0;
        resume->magnum_ammo = 0;
        resume->autos_ammo = 0;
        resume->desert_eagle_ammo = 0;
        resume->uzi_ammo = 0;
        resume->harpoon_ammo = 0;
        resume->m16_ammo = 0;
        resume->mp5_ammo = 0;
        resume->grenade_ammo = 0;
        resume->rocket_ammo = 0;

        resume->small_medipacks = 0;
        resume->large_medipacks = 0;
        resume->num_scions = 0;
        resume->num_quest_item_1 = 0;
        resume->num_quest_item_2 = 0;
        resume->num_quest_item_3 = 0;
        resume->num_quest_item_4 = 0;
        resume->flares = 0;

        resume->equipped_gun_type = LGT_UNARMED;
        resume->holsters_gun_type = LGT_UNARMED;
        resume->back_gun_type = LGT_UNARMED;
        resume->gun_status = LGS_ARMLESS;
    }

    if (level == GF_GetFirstLevel()) {
        resume->flags.available = true;
        resume->flags.costume = false;

        resume->flags.has_pistols = true;
        resume->flags.has_shotgun = false;
        resume->flags.has_magnums = false;
        resume->flags.has_autos = false;
        resume->flags.has_desert_eagle = false;
        resume->flags.has_uzis = false;

        resume->small_medipacks = 0;
        resume->large_medipacks = 0;
        resume->flares = 0;
        resume->pistol_ammo = 1000;
        resume->shotgun_ammo = 0;
        resume->magnum_ammo = 0;
        resume->autos_ammo = 0;
        resume->desert_eagle_ammo = 0;
        resume->uzi_ammo = 0;
        resume->num_scions = 0;
        resume->num_quest_item_1 = 0;
        resume->num_quest_item_2 = 0;
        resume->num_quest_item_3 = 0;
        resume->num_quest_item_4 = 0;
        resume->flags.has_harpoon = false;
        resume->flags.has_m16 = false;
        resume->flags.has_mp5 = false;
        resume->flags.has_grenade = false;
        resume->flags.has_rocket = false;
        resume->harpoon_ammo = 0;
        resume->m16_ammo = 0;
        resume->mp5_ammo = 0;
        resume->grenade_ammo = 0;
        resume->rocket_ammo = 0;
        resume->equipped_gun_type = LGT_PISTOLS;
        resume->holsters_gun_type = LGT_PISTOLS;
        resume->back_gun_type = LGT_UNARMED;
        resume->gun_status = LGS_ARMLESS;
    }

    if (Game_IsBonusFlagSet(GBF_NGPLUS) && level != GF_GetGymLevel()) {
        resume->flags.has_pistols = true;
        resume->flags.has_shotgun = true;
        resume->flags.has_magnums = g_Weapons[LGT_MAGNUMS].is_available;
        resume->flags.has_autos = g_Weapons[LGT_AUTOS].is_available;
        resume->flags.has_desert_eagle =
            g_Weapons[LGT_DESERT_EAGLE].is_available;
        resume->flags.has_uzis = true;
        resume->flags.has_m16 = g_Weapons[LGT_M16].is_available;
        resume->flags.has_mp5 = g_Weapons[LGT_MP5].is_available;
        resume->flags.has_grenade = g_Weapons[LGT_GRENADE].is_available;
        resume->flags.has_rocket = g_Weapons[LGT_ROCKET].is_available;
        resume->flags.has_harpoon = g_Weapons[LGT_HARPOON].is_available;

        resume->shotgun_ammo = 10000;
        resume->magnum_ammo = resume->flags.has_magnums ? 10000 : 0;
        resume->autos_ammo = resume->flags.has_autos ? 10000 : 0;
        resume->desert_eagle_ammo = resume->flags.has_desert_eagle ? 10000 : 0;
        resume->uzi_ammo = 10000;
        resume->flares = g_TRVersion == 1 ? 0 : -1;

        resume->m16_ammo = resume->flags.has_m16 ? 10000 : 0;
        resume->mp5_ammo = resume->flags.has_mp5 ? 10000 : 0;
        resume->grenade_ammo = resume->flags.has_grenade ? 10000 : 0;
        resume->rocket_ammo = resume->flags.has_rocket ? 10000 : 0;
        resume->harpoon_ammo = resume->flags.has_harpoon ? 10000 : 0;

        const bool should_force_ngplus_gun_setup =
            !g_Config.gameplay.remember_gun_status || resume->prev_level == -1;
        if (should_force_ngplus_gun_setup) {
            switch (g_TRVersion) {
            case 1:
                resume->equipped_gun_type = LGT_UZIS;
                resume->back_gun_type = LGT_SHOTGUN;
                resume->holsters_gun_type = LGT_UZIS;
                break;
            case 2:
                resume->equipped_gun_type = LGT_GRENADE;
                resume->back_gun_type = LGT_GRENADE;
                resume->holsters_gun_type = LGT_PISTOLS;
                break;
            case 3:
            default:
                resume->equipped_gun_type = LGT_ROCKET;
                resume->back_gun_type = LGT_ROCKET;
                resume->holsters_gun_type = LGT_PISTOLS;
                break;
            }
        }
    }

    resume->stats.secret_flags = 0;

    M_DetermineLegacyGunTypes(resume);
}

void Savegame_ProcessItemsBeforeSave(void)
{
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        ITEM *const item = Item_Get(i);
        const OBJECT *const obj = Object_Get(item->object_id);
        if (obj->handle_save_func != nullptr) {
            obj->handle_save_func(item, SAVEGAME_STAGE_BEFORE_SAVE);
        }
    }
}

void Savegame_ProcessItemsBeforeLoad(void)
{
    for (int32_t i = 0; i < Item_GetLevelCount(); i++) {
        ITEM *const item = Item_Get(i);
        const OBJECT *const obj = Object_Get(item->object_id);
        if (obj->handle_save_func != nullptr) {
            obj->handle_save_func(item, SAVEGAME_STAGE_BEFORE_LOAD);
        }
    }
}

void Savegame_ScanSavedGames(void)
{
    BENCHMARK benchmark = Benchmark_Start();
    M_ClearSlots();

    m_SaveCounter = 0;
    m_SavedGames = 0;
    m_MostRecentlyCreatedSlot = Savegame_InvalidSlot();
    m_NextQuickSlot = 0;
    int32_t newest_quick_counter = -1;
    int32_t newest_quick_slot = -1;

    // Scan low-priority locations first; the write directory is authoritative.
    M_ScanSavedGamesDir(".");
    M_ScanSavedGamesDir(TRXPath_Get(TRX_PATH_LEGACY_SAVES_DIR));
    M_ScanSavedGamesDir(TRXPath_Get(TRX_PATH_SAVES_DIR));

    {
        // M_GetSaveWriteDir may use static formatting storage, so copy it
        // before scanning because nested formatting calls during scan can
        // overwrite it.
        AUTO_FREE char *write_dir = Memory_DupStr(M_GetSaveWriteDir());
        M_ScanSavedGamesDir(write_dir);
    }

    for (SAVEGAME_SLOT_POOL pool = 0; pool < SAVEGAME_SLOT_POOL_NUMBER_OF;
         pool++) {
        for (int32_t i = 0; i < Savegame_GetSlotCount(pool); i++) {
            SAVEGAME_INFO *const savegame_info = M_GetSavegameInfoSlot(
                (SAVEGAME_SLOT_REF) { .pool = pool, .index = i });
            if (savegame_info->level_title == nullptr) {
                continue;
            }
            if (savegame_info->counter > m_SaveCounter) {
                m_SaveCounter = savegame_info->counter;
                m_MostRecentlyCreatedSlot =
                    (SAVEGAME_SLOT_REF) { .pool = pool, .index = i };
            }
            m_SavedGames++;

            if (pool == SAVEGAME_SLOT_POOL_QUICK
                && savegame_info->counter > newest_quick_counter) {
                newest_quick_counter = savegame_info->counter;
                newest_quick_slot = i;
            }
        }
    }

    if (m_QuickSaveSlots > 0 && newest_quick_slot >= 0) {
        m_NextQuickSlot = (newest_quick_slot + 1) % m_QuickSaveSlots;
    }

    Benchmark_End(&benchmark, nullptr);
}

bool Savegame_Save(const SAVEGAME_SLOT_REF slot)
{
    if (!Savegame_IsValidSlotRef(slot)) {
        return false;
    }

    bool result = false;
    Savegame_BindSlot(slot);

    const GF_LEVEL *const current_level = Game_GetCurrentLevel();

    Savegame_PersistGameToCurrentInfo(current_level);

    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        if (level->type == GFL_CURRENT) {
            Savegame_SetCurrentInfo(i, current_level->num);
        }
    }

    SAVEGAME_INFO *const savegame_info = M_GetSavegameInfoSlot(slot);
    const bool was_slot_empty = savegame_info->full_path == nullptr;

    m_SaveCounter++;
    const char *const save_pattern = M_GetSaveFilePatternForPool(slot.pool);
    char *file_name = String_Format(save_pattern, slot.index);
    char *full_path = M_GetSaveWritePath(file_name);
    File_EnsureParentDirectories(full_path);
    MYFILE *const fp = File_Open(full_path, FILE_OPEN_WRITE);
    if (fp != nullptr) {
        savegame_info->is_quick = slot.pool == SAVEGAME_SLOT_POOL_QUICK;
        SG_File_SaveToFile(fp, savegame_info);
        File_Close(fp);
        result = true;
    }
    if (result) {
        M_FillSlot(slot, full_path);
    }

    Memory_FreePointer(&file_name);
    Memory_FreePointer(&full_path);

    if (result) {
        m_MostRecentlyCreatedSlot = slot;
        if (was_slot_empty) {
            m_SavedGames++;
        }

        if (slot.pool == SAVEGAME_SLOT_POOL_QUICK && m_QuickSaveSlots > 0) {
            m_NextQuickSlot = (slot.index + 1) % m_QuickSaveSlots;
        }
    } else {
        m_SaveCounter--;
    }

    return result;
}

bool Savegame_Load(const SAVEGAME_SLOT_REF slot)
{
    const SAVEGAME_INFO *const savegame_info = Savegame_GetSavegameInfo(slot);
    if (savegame_info == nullptr) {
        return false;
    }
    ASSERT(savegame_info->full_path != nullptr);

    M_LoadPreprocess();

    bool result = false;
    MYFILE *const fp = File_Open(savegame_info->full_path, FILE_OPEN_READ);
    if (fp != nullptr) {
        result = SG_File_LoadFromFile(fp);
        File_Close(fp);
    }

    M_LoadPostprocess();
    m_InitialVersion = savegame_info->initial_version;
    return result;
}

bool Savegame_UpdateDeathCounters(
    const SAVEGAME_SLOT_REF slot, const int32_t death_count)
{
    const SAVEGAME_INFO *const savegame_info = Savegame_GetSavegameInfo(slot);
    if (savegame_info == nullptr) {
        return false;
    }
    ASSERT(savegame_info->full_path != nullptr);

    bool ret = false;
    MYFILE *const fp =
        File_Open(savegame_info->full_path, FILE_OPEN_READ_WRITE);
    if (fp != nullptr) {
        ret = SG_File_UpdateDeathCounters(
            fp, savegame_info->level_num, death_count, savegame_info->is_quick);
        File_Close(fp);
    }
    return ret;
}

bool Savegame_LoadOnlyResumeInfo(const SAVEGAME_SLOT_REF slot)
{
    const SAVEGAME_INFO *const savegame_info = Savegame_GetSavegameInfo(slot);
    if (savegame_info == nullptr) {
        return false;
    }
    ASSERT(savegame_info->full_path != nullptr);

    bool ret = false;
    MYFILE *const fp = File_Open(savegame_info->full_path, FILE_OPEN_READ);
    if (fp != nullptr) {
        ret = SG_File_LoadOnlyResumeInfo(fp);
        File_Close(fp);
    }

    Savegame_SetInitialVersion(savegame_info->initial_version);
    return ret;
}

bool Savegame_RestartAvailable(const SAVEGAME_SLOT_REF slot)
{
    if (!Savegame_IsValidSlotRef(slot)) {
        return true;
    }
    const SAVEGAME_INFO *const savegame_info = Savegame_GetSavegameInfo(slot);
    return savegame_info->features.restart;
}

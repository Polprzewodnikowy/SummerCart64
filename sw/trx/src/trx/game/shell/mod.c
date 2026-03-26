#include <trx/game/shell/mod.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/game_flow/reader.h>
#include <trx/game/shell/common.h>
#include <trx/game/shell/paths.h>
#include <trx/game/shell/state.h>
#include <trx/version.h>

#include <string.h>

typedef struct {
    GF_MOD_META meta;
    SHELL_MOD_TYPE mod_type;
} M_KNOWN_MOD;

static const M_KNOWN_MOD m_KnownModSeeds[] = {
    { .meta = { .name = "tr1", .engine = 1 }, .mod_type = MOD_BASE_GAME },
    { .meta = { .name = "tr1-ub", .engine = 1, .extends = "tr1" },
      .mod_type = MOD_EXPANSION_PACK },
    { .meta = { .name = "tr1-demo-pc", .engine = 1, .extends = "tr1" },
      .mod_type = MOD_MISC },
    { .meta = { .name = "tr1-level", .engine = 1, .extends = "tr1" },
      .mod_type = MOD_DIRECT_LEVEL },
    { .meta = { .name = "tr2", .engine = 2 }, .mod_type = MOD_BASE_GAME },
    { .meta = { .name = "tr2-gm", .engine = 2, .extends = "tr2" },
      .mod_type = MOD_EXPANSION_PACK },
    { .meta = { .name = "tr2-level", .engine = 2, .extends = "tr2" },
      .mod_type = MOD_DIRECT_LEVEL },
    { .meta = { .name = "tr3", .engine = 3 }, .mod_type = MOD_BASE_GAME },
    { .meta = { .name = "tr3-la", .engine = 3, .extends = "tr3" },
      .mod_type = MOD_EXPANSION_PACK },
    { .meta = { .name = "tr3-level", .engine = 3, .extends = "tr3" },
      .mod_type = MOD_DIRECT_LEVEL },
};

static VECTOR *m_Mods = nullptr;

static SHELL_MOD *M_FindMod(const char *const name)
{
    if (m_Mods == nullptr || name == nullptr) {
        return nullptr;
    }
    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (strcmp(mod->name, name) == 0) {
            return mod;
        }
    }
    return nullptr;
}

static void M_AddMod(
    const char *const name, const char *const title,
    const SHELL_MOD_TYPE mod_type, const int32_t engine_version,
    const char *const base_mod)
{
    const SHELL_MOD mod = {
        .name = Memory_DupStr(name),
        .title = title != nullptr ? Memory_DupStr(title) : nullptr,
        .mod_type = mod_type,
        .engine_version = engine_version,
        .base_mod = base_mod != nullptr ? Memory_DupStr(base_mod) : nullptr,
        .is_available = false,
        .is_valid = false,
    };
    Vector_Add(m_Mods, &mod);
}

static void M_SeedKnownMods(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_KnownModSeeds); i++) {
        const M_KNOWN_MOD *const seed = &m_KnownModSeeds[i];
        M_AddMod(
            seed->meta.name, nullptr, seed->mod_type, seed->meta.engine,
            seed->meta.extends);
    }
}

static void M_ScanForCustomMods(void)
{
    const char *const games_dir = TRXPath_Get(TRX_PATH_GAMES_DIR);
    if (games_dir == nullptr) {
        return;
    }

    void *const dir = File_OpenDirectory(games_dir);
    if (dir == nullptr) {
        return;
    }

    const char *entry;
    while ((entry = File_ReadDirectory(dir)) != nullptr) {
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) {
            continue;
        }

        if (M_FindMod(entry) != nullptr) {
            continue;
        }

        const char *const gameflow_path =
            String_FormatStatic("%s/%s/gameflow.json5", games_dir, entry);

        GF_MOD_META meta = {};
        if (!GF_ReadModMeta(gameflow_path, &meta)) {
            LOG_WARNING("Failed to read mod metadata from '%s'", gameflow_path);
            continue;
        }

        if (meta.engine <= 0) {
            LOG_WARNING(
                "Custom mod '%s' has no 'engine' field in gameflow; skipping",
                entry);
            Memory_FreePointer(&meta.name);
            Memory_FreePointer(&meta.extends);
            continue;
        }

        M_AddMod(entry, meta.name, MOD_CUSTOM, meta.engine, meta.extends);
        Memory_FreePointer(&meta.name);
        Memory_FreePointer(&meta.extends);
    }

    File_CloseDirectory(dir);
}

static void M_ReadModMetaForKnownMods(void)
{
    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (!mod->is_available || mod->mod_type == MOD_CUSTOM) {
            continue;
        }

        const char *const gameflow_path =
            TRXPath_Resolve(TRX_DYNAMIC_PATH_GAMEFLOW_FILE, mod->name);
        if (gameflow_path == nullptr) {
            continue;
        }

        GF_MOD_META meta = {};
        if (!GF_ReadModMeta(gameflow_path, &meta)) {
            continue;
        }

        if (meta.name != nullptr) {
            Memory_FreePointer(&mod->title);
            mod->title = meta.name;
            meta.name = nullptr;
        }

        if (meta.engine > 0) {
            mod->engine_version = meta.engine;
        }

        if (meta.extends != nullptr) {
            Memory_FreePointer(&mod->base_mod);
            mod->base_mod = meta.extends;
            meta.extends = nullptr;
        }

        Memory_FreePointer(&meta.name);
        Memory_FreePointer(&meta.extends);
    }
}

static void M_ValidateEngineVersions(void)
{
    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (mod->engine_version <= 0 && mod->is_available) {
            LOG_WARNING(
                "Mod '%s' has no valid engine version; disabling", mod->name);
            mod->is_available = false;
            mod->is_valid = false;
        }
    }
}

static void M_ValidateNoMixedModLayouts(void)
{
    const char *const games_dir = TRXPath_Get(TRX_PATH_GAMES_DIR);
    const char *const config_dir = TRXPath_Get(TRX_PATH_CONFIG_DIR);
    if (games_dir == nullptr || config_dir == nullptr
        || strcmp(games_dir, config_dir) == 0) {
        return;
    }

    for (int32_t i = 0; i < m_Mods->count; i++) {
        const SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (mod->mod_type == MOD_CUSTOM) {
            continue;
        }
        const char *const legacy_gameflow =
            String_FormatStatic("%s/%s/gameflow.json5", config_dir, mod->name);
        if (File_Exists(legacy_gameflow)) {
            Shell_ExitSystemFmt(
                "Mixed mod layout detected: found legacy mod data at '%s' "
                "while '%s' is used for mods. Move '%s' to '%s/%s/'.",
                legacy_gameflow, games_dir, mod->name, games_dir, mod->name);
        }
    }
}

static const char *M_GetModStringsPath(const char *const mod_id)
{
    ASSERT(mod_id != nullptr);
    return TRXPath_Join(
        TRX_PATH_GAMES_DIR, String_FormatStatic("%s/strings.json5", mod_id));
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    if (m_Mods == nullptr) {
        return;
    }
    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        Memory_FreePointer(&mod->name);
        Memory_FreePointer(&mod->title);
        Memory_FreePointer(&mod->base_mod);
    }
    Vector_Free(m_Mods);
    m_Mods = nullptr;
}

void Shell_ScanAvailableMods(void)
{
    if (m_Mods != nullptr) {
        M_Shutdown();
    }
    m_Mods = Vector_Create(sizeof(SHELL_MOD));

    M_SeedKnownMods();

    // Mark availability for all seeded mods.
    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        mod->is_available =
            TRXPath_Exists(TRX_DYNAMIC_PATH_GAMEFLOW_FILE, mod->name);
        mod->is_valid = mod->is_available;
    }

    M_ValidateNoMixedModLayouts();
    M_ScanForCustomMods();

    // Mark availability for newly added custom mods.
    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (mod->mod_type == MOD_CUSTOM) {
            mod->is_available =
                TRXPath_Exists(TRX_DYNAMIC_PATH_GAMEFLOW_FILE, mod->name);
            mod->is_valid = mod->is_available;
        }
    }

    M_ReadModMetaForKnownMods();
    M_ValidateEngineVersions();
}

void Shell_ValidateMods(void)
{
    const int32_t original_tr_version = g_TRVersion;

    for (int32_t i = 0; i < m_Mods->count; i++) {
        SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (!mod->is_available) {
            mod->is_valid = false;
            continue;
        }

        const SHELL_ARGS args = {
            .engine_version = mod->engine_version,
            .mod = mod,
            .level_to_select = -1,
            .save_to_load = -1,
        };
        g_TRVersion = mod->engine_version;
        TRXPath_Init(&args);

        mod->is_valid = GF_ValidateMod(mod->name, Shell_GetGameFlowPath(mod));
    }

    g_TRVersion = original_tr_version;
}

int32_t Shell_GetModCount(void)
{
    return m_Mods != nullptr ? m_Mods->count : 0;
}

const SHELL_MOD *Shell_GetMod(const int32_t index)
{
    if (index < 0 || index >= Shell_GetModCount()) {
        return nullptr;
    }
    return Vector_Get(m_Mods, index);
}

const SHELL_MOD *Shell_GetModByName(const char *const name)
{
    if (m_Mods == nullptr || name == nullptr) {
        return nullptr;
    }
    for (int32_t i = 0; i < m_Mods->count; i++) {
        const SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (mod->is_available && strcmp(mod->name, name) == 0) {
            return mod;
        }
    }
    return nullptr;
}

static bool M_MatchesEngineVersion(
    const SHELL_MOD *const mod, const int32_t engine_version)
{
    return engine_version == 0 || mod->engine_version == engine_version;
}

static const SHELL_MOD *M_GetFirstAvailableMod(const int32_t engine_version)
{
    for (int32_t i = 0; i < m_Mods->count; i++) {
        const SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (!Shell_CanSwitchToMod(mod)
            || !M_MatchesEngineVersion(mod, engine_version)) {
            continue;
        }
        return mod;
    }
    return nullptr;
}

const SHELL_MOD *Shell_SelectStartupMod(const int32_t engine_version)
{
    const char *const last_played_mod = ShellState_GetLastPlayedMod();
    if (last_played_mod != nullptr) {
        const SHELL_MOD *const mod = Shell_GetModByName(last_played_mod);
        if (mod != nullptr && Shell_CanSwitchToMod(mod)
            && M_MatchesEngineVersion(mod, engine_version)) {
            return mod;
        }
    }

    return M_GetFirstAvailableMod(engine_version);
}

const SHELL_MOD *Shell_GetModByType(
    const SHELL_MOD_TYPE mod_type, const int32_t engine_version)
{
    const SHELL_MOD *found = nullptr;

    for (int32_t i = 0; i < Shell_GetModCount(); i++) {
        const SHELL_MOD *const mod = Vector_Get(m_Mods, i);
        if (!mod->is_available || mod->mod_type != mod_type
            || (engine_version > 0 && mod->engine_version != engine_version)) {
            continue;
        }

        // match
        if (engine_version == 0) {
            if (found) {
                // more than one mod matches this engine version, abort
                return nullptr;
            }
            found = mod;
        } else {
            // exact version match
            return mod;
        }
    }

    return found;
}

bool Shell_CanSwitchToMod(const SHELL_MOD *const mod)
{
    return mod != nullptr && mod->is_available && mod->is_valid
        && mod->mod_type != MOD_DIRECT_LEVEL;
}

bool Shell_IsCurrentMod(const char *const name)
{
    if (name == nullptr) {
        return false;
    }

    const SHELL_ARGS *const args = Shell_GetArgs();
    if (args == nullptr || args->mod == nullptr || args->mod->name == nullptr) {
        return false;
    }

    return strcmp(args->mod->name, name) == 0;
}

const char *Shell_GetCommonStringsPath(void)
{
    return TRXPath_TryResolve(
        TRX_DYNAMIC_PATH_COMMON_CONFIG, "base_strings.json5");
}

const char *Shell_GetBaseGameStringsPath(const SHELL_MOD *const mod)
{
    const char *const base_mod =
        mod->base_mod != nullptr ? mod->base_mod : mod->name;
    return M_GetModStringsPath(base_mod);
}

const char *Shell_GetGameStringsPath(const SHELL_MOD *const mod)
{
    return M_GetModStringsPath(mod->name);
}

const char *Shell_GetGameFlowPath(const SHELL_MOD *const mod)
{
    return TRXPath_Resolve(TRX_DYNAMIC_PATH_GAMEFLOW_FILE, mod->name);
}

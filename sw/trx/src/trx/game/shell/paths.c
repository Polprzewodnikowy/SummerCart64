#include <trx/game/shell/paths.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/shell/common.h>
#include <trx/game/shell/mod.h>
#include <trx/version.h>

#include <SDL2/SDL_filesystem.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define M_MAX_MOD_CHAIN 8

typedef struct {
    char *trx_dir;
    char *config_dir;
    char *cache_dir;
    char *games_dir;
    char *screenshots_dir;
    char *saves_dir;
    char *legacy_saves_dir;
    char *music_dir;
    const SHELL_ARGS *args;
    const char *mod_chain[M_MAX_MOD_CHAIN];
    int32_t mod_chain_count;
    bool inited;
} M_CONTEXT;

typedef struct {
    const char *key;
    const char *value;
} M_PATH_TOKEN;

typedef struct {
    TRX_DYNAMIC_PATH id;
    const char *patterns[8];
    const char **extensions;
    bool check_exists;
    bool is_dir;
} M_DYNAMIC_PATH_POLICY;

typedef struct {
    char *name;
} M_DIR_ENTRY;

typedef struct {
    char *dir;
    bool exists;
    VECTOR *entries; // M_DIR_ENTRY
} M_DIR_CACHE_ENTRY;

typedef struct {
    uint32_t generation;
    TRX_DYNAMIC_PATH path;
    char *rel;
    char *resolved;
    bool found;
} M_RESOLVE_CACHE_ENTRY;

typedef bool (*M_RESOLVE_ATTEMPT_CALLBACK)(
    const char *attempt_path, void *user_data);

static const char *m_FMVExtensions[] = {
    ".mp4", ".mkv", ".mpeg", ".avi", ".webm", ".ogv", ".rpl", ".fmv", nullptr,
};

static const M_DYNAMIC_PATH_POLICY m_PathPolicies[TRX_DYNAMIC_PATH_NUMBER_OF] = {
    [TRX_DYNAMIC_PATH_COMMON_CONFIG] = {
        .patterns = {
            "%mod_dir%/%rel%",
            "%base_mod_dir%/%rel%",
            "%config_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_CATALOG] = {
        .patterns = {
            "%mod_dir%/%rel%",
            "%base_mod_dir%/%rel%",
            "%config_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_GAMEFLOW_FILE] = {
        .patterns = {
            "%games_dir%/%rel%/gameflow.json5",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_LEVEL_FILE] = {
        .patterns = {
            "%mod_dir%/levels/%rel%",
            "%trx_dir%/data/%rel%",
            "%trx_dir%/%rel%",
            // TR3 legacy cutscenes
            "%trx_dir%/cuts/%rel%",
            "%mod_dir%/cuts/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_SHARED_LEVEL_FILE] = {
        .patterns = {
            "%mod_dir%/levels/%rel%",
            "%base_mod_dir%/levels/%rel%",
            "%trx_dir%/data/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_IMAGE_FILE] = {
        .patterns = {
            "%mod_dir%/images/%rel%",
            "%base_mod_dir%/images/%rel%",
            "%trx_dir%/data/images/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_INJECTION_FILE] = {
        .patterns = {
            "%mod_dir%/injections/%rel%",
            "%base_mod_dir%/injections/%rel%",
            "%trx_dir%/data/injections/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_SCRIPT_FILE] = {
        .patterns = {
            "%mod_dir%/scripts/%rel%",
            "%base_mod_dir%/scripts/%rel%",
            "%trx_dir%/data/scripts/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_FMV_FILE] = {
        .patterns = {
            "%mod_dir%/fmv/%rel%",
            "%base_mod_dir%/fmv/%rel%",
            "%trx_dir%/fmv/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .extensions = m_FMVExtensions,
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_SFX_FILE] = {
        .patterns = {
            "%mod_dir%/%rel%",
            "%base_mod_dir%/%rel%",
            "%trx_dir%/data/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_CDAUDIO_FILE] = {
        .patterns = {
            "%mod_dir%/music/%rel%",
            "%mod_dir%/audio/%rel%",
            "%base_mod_dir%/music/%rel%",
            "%base_mod_dir%/audio/%rel%",
            "%trx_dir%/audio/%rel%",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_MUSIC_DIR] = {
        .patterns = {
            "%mod_dir%/music",
            "%base_mod_dir%/music",
            "%trx_dir%/music",
            "%trx_dir%/%rel%",
            nullptr,
        },
        .check_exists = true,
        .is_dir = true,
    },
    [TRX_DYNAMIC_PATH_SHADER_FILE] = {
        .patterns = {
            "%mod_dir%/shaders/%rel%",
            "%base_mod_dir%/shaders/%rel%",
            "%trx_dir%/shaders/%rel%",
            "%trx_dir%/cfg/shaders/%rel%",
            nullptr,
        },
        .check_exists = true,
    },
    [TRX_DYNAMIC_PATH_SCREENSHOT_WRITE_FILE] = {
        .patterns = {
            "%trx_dir%/screenshots/%mod%/%rel%",
            "%trx_dir%/screenshots/%rel%",
            nullptr,
        },
        .check_exists = false,
    },
};

static M_CONTEXT m_Context = {};
static VECTOR *m_DirCache = nullptr; // M_DIR_CACHE_ENTRY
static VECTOR *m_ResolveCache = nullptr; // M_RESOLVE_CACHE_ENTRY
static uint32_t m_ResolveCacheGeneration = 0;

static void M_ClearResolveCache(void)
{
    if (m_ResolveCache == nullptr) {
        return;
    }
    for (int32_t i = 0; i < m_ResolveCache->count; i++) {
        M_RESOLVE_CACHE_ENTRY *const e = Vector_Get(m_ResolveCache, i);
        Memory_FreePointer(&e->rel);
        Memory_FreePointer(&e->resolved);
    }
    Vector_Free(m_ResolveCache);
    m_ResolveCache = nullptr;
}

// Returns a non-owning pointer that may reference static formatting storage.
// Do not free it.
static const char *M_JoinPathStatic(const char *const a, const char *const b)
{
    ASSERT(b != nullptr);
    if (a == nullptr) {
        return b;
    }
    if (String_IsEmpty(b)) {
        return a;
    }
    const bool a_has_sep = String_EndsWith(a, "/") || String_EndsWith(a, "\\");
    const bool b_has_sep = b[0] == '/' || b[0] == '\\';
    const char *const b_join = a_has_sep && b_has_sep ? b + 1 : b;
    const char *const sep = (!a_has_sep && !b_has_sep) ? "/" : "";
    return String_FormatStatic("%s%s%s", a, sep, b_join);
}

static char *M_JoinPathAlloc(const char *const a, const char *const b)
{
    ASSERT(a != nullptr);
    ASSERT(b != nullptr);
    return Memory_DupStr(M_JoinPathStatic(a, b));
}

static M_DIR_CACHE_ENTRY *M_FindDirCache(const char *const dir)
{
    if (m_DirCache == nullptr || dir == nullptr) {
        return nullptr;
    }
    for (int32_t i = 0; i < m_DirCache->count; i++) {
        M_DIR_CACHE_ENTRY *const entry = Vector_Get(m_DirCache, i);
        if (strcmp(entry->dir, dir) == 0) {
            return entry;
        }
    }
    return nullptr;
}

static M_DIR_CACHE_ENTRY *M_LoadDirCache(const char *const dir)
{
    if (dir == nullptr) {
        return nullptr;
    }
    if (m_DirCache == nullptr) {
        m_DirCache = Vector_Create(sizeof(M_DIR_CACHE_ENTRY));
    }
    M_DIR_CACHE_ENTRY *existing = M_FindDirCache(dir);
    if (existing != nullptr) {
        return existing;
    }

    M_DIR_CACHE_ENTRY entry = {
        .dir = Memory_DupStr(dir),
        .exists = false,
        .entries = Vector_Create(sizeof(M_DIR_ENTRY)),
    };
    void *const d = File_OpenDirectory(dir);
    if (d != nullptr) {
        entry.exists = true;
        const char *name = nullptr;
        while ((name = File_ReadDirectory(d)) != nullptr) {
            M_DIR_ENTRY de = { .name = Memory_DupStr(name) };
            Vector_Add(entry.entries, &de);
        }
        File_CloseDirectory(d);
    }
    Vector_Add(m_DirCache, &entry);
    return Vector_Get(m_DirCache, m_DirCache->count - 1);
}

static const char *M_FindDirEntryCaseAware(
    const M_DIR_CACHE_ENTRY *const dir_cache, const char *const segment)
{
    ASSERT(dir_cache != nullptr);
    ASSERT(segment != nullptr);
    for (int32_t i = 0; i < dir_cache->entries->count; i++) {
        const M_DIR_ENTRY *const e = Vector_Get(dir_cache->entries, i);
        if (strcmp(e->name, segment) == 0) {
            return e->name;
        }
    }
    for (int32_t i = 0; i < dir_cache->entries->count; i++) {
        const M_DIR_ENTRY *const e = Vector_Get(dir_cache->entries, i);
        if (String_Equivalent(e->name, segment)) {
            return e->name;
        }
    }
    return nullptr;
}

static char *M_ResolveCasePathCached(const char *const path)
{
    if (path == nullptr || String_IsEmpty(path)) {
        return nullptr;
    }
    char *path_copy = Memory_DupStr(path);
    char *path_piece = path_copy;
    char *current_path = Memory_Alloc(strlen(path) + 2);

    if (path_copy[0] == '/') {
        strcpy(current_path, "/");
        path_piece++;
    } else if (strstr(path_copy, ":\\") || strstr(path_copy, ":/")) {
        strcpy(current_path, path_copy);
        char *drive_sep = strstr(current_path, ":\\");
        if (drive_sep == nullptr) {
            drive_sep = strstr(current_path, ":/");
        }
        ASSERT(drive_sep != nullptr);
        drive_sep[2] = '\0';
        path_piece += 3;
    } else {
        strcpy(current_path, ".");
    }

    while (path_piece != nullptr) {
        char *delim = strpbrk(path_piece, "/\\");
        if (delim != nullptr) {
            *delim = '\0';
        }

        if (path_piece[0] == '\0') {
            if (delim != nullptr) {
                path_piece = delim + 1;
                continue;
            }
            break;
        }

        M_DIR_CACHE_ENTRY *const dir_cache = M_LoadDirCache(current_path);
        if (dir_cache == nullptr || !dir_cache->exists) {
            Memory_FreePointer(&path_copy);
            Memory_FreePointer(&current_path);
            return nullptr;
        }

        const char *const resolved_piece =
            M_FindDirEntryCaseAware(dir_cache, path_piece);
        if (resolved_piece == nullptr) {
            Memory_FreePointer(&path_copy);
            Memory_FreePointer(&current_path);
            return nullptr;
        }
        char *next = M_JoinPathAlloc(current_path, resolved_piece);
        Memory_FreePointer(&current_path);
        current_path = next;

        if (delim != nullptr) {
            path_piece = delim + 1;
        } else {
            break;
        }
    }

    Memory_FreePointer(&path_copy);
    return current_path;
}

static char *M_GuessExtensionCached(
    const char *const path, const char **const extensions)
{
    if (path == nullptr || extensions == nullptr) {
        return nullptr;
    }

    char *parent_dir = File_GetParentDirectory(path);
    if (parent_dir == nullptr) {
        return nullptr;
    }
    char *resolved_parent = M_ResolveCasePathCached(parent_dir);
    Memory_FreePointer(&parent_dir);
    if (resolved_parent == nullptr) {
        return nullptr;
    }
    Memory_FreePointer(&resolved_parent);

    char *resolved = M_ResolveCasePathCached(path);
    if (resolved != nullptr) {
        return resolved;
    }

    const char *const dot = strrchr(path, '.');
    if (dot == nullptr) {
        return nullptr;
    }

    for (const char **ext = &extensions[0]; *ext != nullptr; ext++) {
        const size_t out_size = (size_t)(dot - path) + strlen(*ext) + 1;
        char *out = Memory_Alloc(out_size);
        strncpy(out, path, (size_t)(dot - path));
        out[dot - path] = '\0';
        strcat(out, *ext);

        resolved = M_ResolveCasePathCached(out);
        Memory_FreePointer(&out);
        if (resolved != nullptr) {
            return resolved;
        }
    }

    return nullptr;
}

static M_RESOLVE_CACHE_ENTRY *M_FindResolveCache(
    const TRX_DYNAMIC_PATH path, const char *const rel)
{
    if (m_ResolveCache == nullptr || rel == nullptr) {
        return nullptr;
    }
    for (int32_t i = 0; i < m_ResolveCache->count; i++) {
        M_RESOLVE_CACHE_ENTRY *const e = Vector_Get(m_ResolveCache, i);
        if (e->generation == m_ResolveCacheGeneration && e->path == path
            && strcmp(e->rel, rel) == 0) {
            return e;
        }
    }
    return nullptr;
}

static void M_SetResolveCache(
    const TRX_DYNAMIC_PATH path, const char *const rel,
    const char *const resolved)
{
    if (rel == nullptr) {
        return;
    }
    if (m_ResolveCache == nullptr) {
        m_ResolveCache = Vector_Create(sizeof(M_RESOLVE_CACHE_ENTRY));
    }
    M_RESOLVE_CACHE_ENTRY *const existing = M_FindResolveCache(path, rel);
    if (existing != nullptr) {
        Memory_FreePointer(&existing->resolved);
        existing->resolved =
            resolved != nullptr ? Memory_DupStr(resolved) : nullptr;
        existing->found = resolved != nullptr;
        return;
    }
    M_RESOLVE_CACHE_ENTRY entry = {
        .generation = m_ResolveCacheGeneration,
        .path = path,
        .rel = Memory_DupStr(rel),
        .resolved = resolved != nullptr ? Memory_DupStr(resolved) : nullptr,
        .found = resolved != nullptr,
    };
    Vector_Add(m_ResolveCache, &entry);
}

static const char *M_GetCurrentModID(void)
{
    if (m_Context.mod_chain_count > 0) {
        return m_Context.mod_chain[0];
    }
    if (m_Context.args != nullptr && m_Context.args->mod != nullptr) {
        return m_Context.args->mod->name;
    }
    return nullptr;
}

static const char *M_GetModDir(const char *const mod_id)
{
    if (mod_id == nullptr || String_IsEmpty(mod_id)
        || m_Context.games_dir == nullptr) {
        return nullptr;
    }
    return M_JoinPathStatic(m_Context.games_dir, mod_id);
}

static const char *M_GetCurrentModDir(void)
{
    return M_GetModDir(M_GetCurrentModID());
}

static const char *M_GetBaseModID(void)
{
    if (m_Context.mod_chain_count > 1) {
        return m_Context.mod_chain[1];
    }
    if (m_Context.args != nullptr && m_Context.args->mod != nullptr) {
        return m_Context.args->mod->base_mod;
    }
    return nullptr;
}

static const char *M_GetDirectLevelArg(void)
{
    return m_Context.args != nullptr && m_Context.args->level_to_play != nullptr
        ? m_Context.args->level_to_play
        : "";
}

static const char *M_GetBaseModDir(void)
{
    return M_GetModDir(M_GetBaseModID());
}

static const char *M_GetLegacyDataDir(void)
{
    return M_JoinPathStatic(m_Context.trx_dir, "data");
}

static const char *M_GetBaseDirForDynamicPath(const TRX_DYNAMIC_PATH path)
{
    switch (path) {
    case TRX_DYNAMIC_PATH_COMMON_CONFIG:
    case TRX_DYNAMIC_PATH_CATALOG:
        return m_Context.config_dir;
    case TRX_DYNAMIC_PATH_LEVEL_FILE:
    case TRX_DYNAMIC_PATH_SHARED_LEVEL_FILE:
    case TRX_DYNAMIC_PATH_SFX_FILE:
        return M_GetLegacyDataDir();
    case TRX_DYNAMIC_PATH_IMAGE_FILE:
        return M_JoinPathStatic(M_GetLegacyDataDir(), "images");
    case TRX_DYNAMIC_PATH_INJECTION_FILE:
        return M_JoinPathStatic(M_GetLegacyDataDir(), "injections");
    case TRX_DYNAMIC_PATH_SCRIPT_FILE:
        return M_JoinPathStatic(M_GetLegacyDataDir(), "scripts");
    case TRX_DYNAMIC_PATH_SHADER_FILE:
        return M_JoinPathStatic(m_Context.trx_dir, "shaders");
    case TRX_DYNAMIC_PATH_FMV_FILE:
        return M_JoinPathStatic(m_Context.trx_dir, "fmv");
    case TRX_DYNAMIC_PATH_CDAUDIO_FILE:
        return M_JoinPathStatic(M_GetLegacyDataDir(), "audio");
    case TRX_DYNAMIC_PATH_MUSIC_DIR:
        return M_JoinPathStatic(M_GetLegacyDataDir(), "music");
    default:
        return m_Context.trx_dir;
    }
}

static const char *M_StripOptionalPrefix(
    const char *const value, const char *const prefix)
{
    if (value != nullptr && prefix != nullptr
        && String_CaseSubstring(value, prefix) == value) {
        return value + strlen(prefix);
    }
    return value;
}

static void M_TrimTrailingSeparators(char *const path)
{
    if (path == nullptr) {
        return;
    }

    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        // Keep Windows drive roots like C:\ intact.
        if (len == 3 && path[1] == ':') {
            break;
        }
        path[len - 1] = '\0';
        len--;
    }
}

static bool M_SetDirFromEnv(
    char **const target, const char *const env_value,
    const char *const default_suffix, const bool check_existence)
{
    ASSERT(target != nullptr);
    ASSERT(default_suffix != nullptr);

    Memory_FreePointer(target);
    if (env_value != nullptr && !String_IsEmpty(env_value)) {
        *target = Memory_DupStr(env_value);
    } else {
        *target = String_Format("%s/%s", m_Context.trx_dir, default_suffix);
    }
    M_TrimTrailingSeparators(*target);
    if (check_existence && !File_DirExists(*target)) {
        Memory_FreePointer(target);
        return false;
    }
    return true;
}

static char *M_ReplacePathTokens(
    char *result, const M_PATH_TOKEN *const tokens, const size_t token_count)
{
    ASSERT(result != nullptr);
    ASSERT(tokens != nullptr);

    for (size_t i = 0; i < token_count; i++) {
        const char *const tok = tokens[i].key;
        const char *const val = tokens[i].value;
        if (val == nullptr || strstr(result, tok) == nullptr) {
            continue;
        }

        const size_t tok_len = strlen(tok);
        const size_t val_len = strlen(val);
        const size_t src_len = strlen(result);
        int32_t tok_count = 0;
        const char *scan = result;
        while ((scan = strstr(scan, tok)) != nullptr) {
            tok_count++;
            scan += tok_len;
        }
        const int64_t delta = (int64_t)val_len - (int64_t)tok_len;
        const int64_t out_len_signed = (int64_t)src_len + delta * tok_count;
        ASSERT(out_len_signed >= 0);
        const size_t out_len = (size_t)out_len_signed;
        char *out = Memory_Alloc(out_len + 1);
        char *dst = out;
        scan = result;
        const char *hit = nullptr;
        while ((hit = strstr(scan, tok)) != nullptr) {
            const size_t prefix_len = (size_t)(hit - scan);
            memcpy(dst, scan, prefix_len);
            dst += prefix_len;
            memcpy(dst, val, val_len);
            dst += val_len;
            scan = hit + tok_len;
        }
        strcpy(dst, scan);
        Memory_FreePointer(&result);
        result = out;
    }

    return result;
}

char *TRXPath_ExpandVars(const char *const in)
{
    if (in == nullptr) {
        return nullptr;
    }
    if (!m_Context.inited) {
        TRXPath_Init(m_Context.args);
    }

    char *result = Memory_DupStr(in);
    const char *const mod_id = M_GetCurrentModID();
    const char *const base_mod_id = M_GetBaseModID();

    const M_PATH_TOKEN tokens[] = {
#define M_PATH_TOKEN_ITEM(name, field, token) { token, m_Context.field },
        TRX_PATH_DIR_LIST(M_PATH_TOKEN_ITEM)
#undef M_PATH_TOKEN_ITEM
            { "%mod%", mod_id != nullptr ? mod_id : "" },
        { "%mod_dir%", M_GetCurrentModDir() },
        { "%levels_dir%",
          M_GetBaseDirForDynamicPath(TRX_DYNAMIC_PATH_LEVEL_FILE) },
        { "%images_dir%",
          M_GetBaseDirForDynamicPath(TRX_DYNAMIC_PATH_IMAGE_FILE) },
        { "%injections_dir%",
          M_GetBaseDirForDynamicPath(TRX_DYNAMIC_PATH_INJECTION_FILE) },
        { "%scripts_dir%",
          M_GetBaseDirForDynamicPath(TRX_DYNAMIC_PATH_SCRIPT_FILE) },
        { "%base_mod%", base_mod_id != nullptr ? base_mod_id : "" },
        { "%base_mod_dir%", M_GetBaseModDir() },
        { "%direct_level%", M_GetDirectLevelArg() },
        { "%tr_version%", String_FormatStatic("%d", g_TRVersion) },
    };

    return M_ReplacePathTokens(result, tokens, ARRAY_SIZE(tokens));
}

static void M_BuildModChain(const SHELL_ARGS *const args)
{
    m_Context.mod_chain_count = 0;
    if (args == nullptr || args->mod == nullptr) {
        return;
    }

    const SHELL_MOD *mod = args->mod;
    while (mod != nullptr && m_Context.mod_chain_count < M_MAX_MOD_CHAIN) {
        for (int32_t i = 0; i < m_Context.mod_chain_count; i++) {
            if (strcmp(m_Context.mod_chain[i], mod->name) == 0) {
                LOG_WARNING(
                    "Mod chain cycle detected at '%s'; stopping traversal",
                    mod->name);
                return;
            }
        }
        m_Context.mod_chain[m_Context.mod_chain_count++] = mod->name;
        if (mod->base_mod == nullptr) {
            break;
        }
        mod = Shell_GetModByName(mod->base_mod);
        if (mod == nullptr) {
            break;
        }
    }
    if (mod != nullptr && m_Context.mod_chain_count == M_MAX_MOD_CHAIN) {
        LOG_WARNING(
            "Mod chain truncated at %d entries; remaining base_mod links will "
            "be ignored",
            M_MAX_MOD_CHAIN);
    }
}

static void M_SeedResolverCaches(void)
{
    const char *const dirs[] = {
        m_Context.trx_dir,
        m_Context.config_dir,
        m_Context.cache_dir,
        m_Context.games_dir,
        m_Context.screenshots_dir,
        m_Context.saves_dir,
        m_Context.legacy_saves_dir,
        M_GetCurrentModDir(),
        M_GetBaseModDir(),
        M_GetLegacyDataDir(),
        M_JoinPathStatic(M_GetLegacyDataDir(), "levels"),
        M_JoinPathStatic(M_GetLegacyDataDir(), "images"),
        M_JoinPathStatic(M_GetLegacyDataDir(), "injections"),
        M_JoinPathStatic(M_GetLegacyDataDir(), "scripts"),
        M_JoinPathStatic(m_Context.trx_dir, "cuts"),
        M_JoinPathStatic(m_Context.trx_dir, "fmv"),
        M_JoinPathStatic(m_Context.trx_dir, "audio"),
        M_JoinPathStatic(m_Context.trx_dir, "music"),
        M_JoinPathStatic(m_Context.trx_dir, "shaders"),
        M_JoinPathStatic(m_Context.trx_dir, "cfg"),
        nullptr,
    };
    for (int32_t i = 0; dirs[i] != nullptr; i++) {
        M_LoadDirCache(dirs[i]);
    }
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    Memory_FreePointer(&m_Context.trx_dir);
    Memory_FreePointer(&m_Context.config_dir);
    Memory_FreePointer(&m_Context.cache_dir);
    Memory_FreePointer(&m_Context.games_dir);
    Memory_FreePointer(&m_Context.screenshots_dir);
    Memory_FreePointer(&m_Context.saves_dir);
    Memory_FreePointer(&m_Context.legacy_saves_dir);
    if (m_DirCache != nullptr) {
        for (int32_t i = 0; i < m_DirCache->count; i++) {
            M_DIR_CACHE_ENTRY *const e = Vector_Get(m_DirCache, i);
            for (int32_t j = 0; j < e->entries->count; j++) {
                M_DIR_ENTRY *const de = Vector_Get(e->entries, j);
                Memory_FreePointer(&de->name);
            }
            Vector_Free(e->entries);
            Memory_FreePointer(&e->dir);
        }
        Vector_Free(m_DirCache);
        m_DirCache = nullptr;
    }
    M_ClearResolveCache();
    m_Context.args = nullptr;
    m_Context.mod_chain_count = 0;
    m_Context.inited = false;
}

void TRXPath_Init(const SHELL_ARGS *const args)
{
    m_Context.args = args;
    m_ResolveCacheGeneration++;

    if (m_Context.trx_dir == nullptr) {
        const char *const base = SDL_GetBasePath();
        if (base != nullptr) {
            m_Context.trx_dir = Memory_DupStr(base);
            SDL_free((void *)base);
        } else {
            m_Context.trx_dir = Memory_DupStr(".");
        }
        M_TrimTrailingSeparators(m_Context.trx_dir);
    }

    M_SetDirFromEnv(
        &m_Context.config_dir, getenv("TRX_CONFIG_DIR"), "cfg", false);
    M_SetDirFromEnv(
        &m_Context.cache_dir, getenv("TRX_CACHE_DIR"), "cache", false);

    if (!M_SetDirFromEnv(
            &m_Context.games_dir, getenv("TRX_GAMES_DIR"), "games", true)) {
        M_SetDirFromEnv(&m_Context.games_dir, nullptr, "cfg", false);
    }

    M_SetDirFromEnv(
        &m_Context.screenshots_dir, getenv("TRX_SCREENSHOTS_DIR"),
        "screenshots", false);

    M_SetDirFromEnv(
        &m_Context.saves_dir, getenv("TRX_SAVES_DIR"), "saves", false);

    Memory_FreePointer(&m_Context.legacy_saves_dir);
    m_Context.legacy_saves_dir = String_Format("%s/saves", m_Context.trx_dir);

    M_BuildModChain(args);
    M_SeedResolverCaches();
    m_Context.inited = true;
}

const char *TRXPath_Get(const TRX_PATH path)
{
    if (!m_Context.inited) {
        TRXPath_Init(m_Context.args);
    }

    switch (path) {
#define M_GET_CASE(name, field, token)                                         \
    case TRX_PATH_##name:                                                      \
        return m_Context.field;
        TRX_PATH_DIR_LIST(M_GET_CASE)
#undef M_GET_CASE
    default:
        ASSERT_FAIL_FMT("Unknown TRX_PATH %d", path);
        return nullptr;
    }
}

const char *TRXPath_Join(const TRX_PATH path, const char *const rel)
{
    const char *const root = TRXPath_Get(path);
    if (root == nullptr || rel == nullptr || String_IsEmpty(rel)) {
        return root;
    }
    return M_JoinPathStatic(root, rel);
}

static const char *M_ExpandDynamicPattern(
    const TRX_DYNAMIC_PATH path, const char *const pattern,
    const char *const rel, const char *const mod_dir_override)
{
    ASSERT(pattern != nullptr);

    const char *rel_value = rel;
    const char *const mod_id = M_GetCurrentModID();
    const char *const base_mod_id = M_GetBaseModID();

    const M_PATH_TOKEN tokens[] = {
#define M_DYNAMIC_PATH_TOKEN_ITEM(name, field, token)                          \
    { token, m_Context.field },
        TRX_PATH_DIR_LIST(M_DYNAMIC_PATH_TOKEN_ITEM)
#undef M_DYNAMIC_PATH_TOKEN_ITEM
            { "%rel%", rel_value },
        { "%mod%", mod_id != nullptr ? mod_id : "" },
        { "%base_mod%", base_mod_id != nullptr ? base_mod_id : "" },
        { "%mod_dir%",
          mod_dir_override != nullptr ? mod_dir_override
                                      : M_GetCurrentModDir() },
        { "%base_mod_dir%", M_GetBaseModDir() },
        { "%base_dir%", M_GetBaseDirForDynamicPath(path) },
        { "%tr_version%", String_FormatStatic("%d", g_TRVersion) },
    };

    char *expanded = Memory_DupStr(pattern);
    expanded = M_ReplacePathTokens(expanded, tokens, ARRAY_SIZE(tokens));

    const char *const resolved = String_FormatStatic("%s", expanded);
    Memory_FreePointer(&expanded);
    return resolved;
}

static bool M_ForEachResolveAttempt(
    const TRX_DYNAMIC_PATH path, const char *const rel,
    const M_RESOLVE_ATTEMPT_CALLBACK callback, void *const user_data,
    const bool stop_after_first_pattern)
{
    ASSERT(path >= 0 && path < TRX_DYNAMIC_PATH_NUMBER_OF);
    ASSERT(callback != nullptr);

    char *expanded_rel = TRXPath_ExpandVars(rel);
    const char *const effective_rel =
        expanded_rel != nullptr ? expanded_rel : rel;

    if (effective_rel != nullptr && File_IsAbsolute(effective_rel)) {
        const bool result = callback(effective_rel, user_data);
        Memory_FreePointer(&expanded_rel);
        return result;
    }

    const M_DYNAMIC_PATH_POLICY *const policy = &m_PathPolicies[path];
    ASSERT(policy != nullptr);

    for (size_t i = 0; i < ARRAY_SIZE(policy->patterns); i++) {
        const char *const pattern = policy->patterns[i];
        if (pattern == nullptr) {
            break;
        }

        char *candidate = Memory_DupStr(
            M_ExpandDynamicPattern(path, pattern, effective_rel, nullptr));
        if (strchr(candidate, '%') != nullptr) {
            Memory_FreePointer(&candidate);
            continue;
        }
        if (!callback(candidate, user_data)) {
            Memory_FreePointer(&candidate);
            return false;
        }

        if (policy->extensions == nullptr || policy->is_dir) {
            Memory_FreePointer(&candidate);
            continue;
        }

        const char *const dot = strrchr(candidate, '.');
        if (dot == nullptr) {
            Memory_FreePointer(&candidate);
            continue;
        }

        for (const char **ext = &policy->extensions[0]; *ext != nullptr;
             ext++) {
            const size_t out_size =
                (size_t)(dot - candidate) + strlen(*ext) + 1;
            char *out = Memory_Alloc(out_size);
            strncpy(out, candidate, (size_t)(dot - candidate));
            out[dot - candidate] = '\0';
            strcat(out, *ext);
            const bool keep_going = callback(out, user_data);
            Memory_FreePointer(&out);
            if (!keep_going) {
                Memory_FreePointer(&candidate);
                return false;
            }
        }

        if (stop_after_first_pattern) {
            Memory_FreePointer(&candidate);
            break;
        }
        Memory_FreePointer(&candidate);
    }

    Memory_FreePointer(&expanded_rel);
    return true;
}

typedef struct {
    const M_DYNAMIC_PATH_POLICY *policy;
    const char *resolved;
} M_RESOLVE_VISITOR_CONTEXT;

static bool M_ResolveAttemptVisitor(
    const char *const attempt_path, void *const user_data)
{
    ASSERT(attempt_path != nullptr);
    ASSERT(user_data != nullptr);

    M_RESOLVE_VISITOR_CONTEXT *const ctx = user_data;
    ASSERT(ctx->policy != nullptr);

    if (ctx->policy->is_dir) {
        if (!ctx->policy->check_exists) {
            ctx->resolved = String_FormatStatic("%s", attempt_path);
            return false;
        }

        char *dir_path = M_ResolveCasePathCached(attempt_path);
        if (dir_path == nullptr) {
            return true;
        }
        M_DIR_CACHE_ENTRY *const dir_cache = M_LoadDirCache(dir_path);
        if (dir_cache != nullptr && dir_cache->exists) {
            ctx->resolved = String_FormatStatic("%s", dir_path);
            Memory_FreePointer(&dir_path);
            return false;
        }
        Memory_FreePointer(&dir_path);
        return true;
    }

    if (!ctx->policy->check_exists) {
        ctx->resolved = String_FormatStatic("%s", attempt_path);
        return false;
    }

    char *full_path = M_ResolveCasePathCached(attempt_path);
    if (full_path == nullptr) {
        return true;
    }
    ctx->resolved = String_FormatStatic("%s", full_path);
    Memory_FreePointer(&full_path);
    return false;
}

static char *M_AppendResolveAttempt(
    char *attempts, const char *const attempt_path)
{
    if (attempt_path == nullptr || String_IsEmpty(attempt_path)) {
        return attempts;
    }
    if (attempts == nullptr) {
        return String_Format("%s", attempt_path);
    }
    char *const joined = String_Format("%s\n  - %s", attempts, attempt_path);
    Memory_FreePointer(&attempts);
    return joined;
}

static bool M_CollectResolveAttemptVisitor(
    const char *const attempt_path, void *const user_data)
{
    ASSERT(user_data != nullptr);
    char **const attempts = user_data;
    *attempts = M_AppendResolveAttempt(*attempts, attempt_path);
    return true;
}

static char *M_GetResolveAttempts(
    const TRX_DYNAMIC_PATH path, const char *const rel)
{
    char *attempts = nullptr;
    M_ForEachResolveAttempt(
        path, rel, M_CollectResolveAttemptVisitor, &attempts, false);
    return attempts;
}

static const char *M_GetResolveError(
    const TRX_DYNAMIC_PATH path, const char *const rel)
{
    char *attempts = M_GetResolveAttempts(path, rel);
    const char *const out = String_FormatStatic(
        "Failed to resolve path \"%s\". Searched paths:\n"
        "  - %s)",
        rel != nullptr ? rel : "(null)",
        attempts != nullptr ? attempts : "(none)");
    Memory_FreePointer(&attempts);
    return out;
}

const char *TRXPath_PeekResolve(
    const TRX_DYNAMIC_PATH path, const char *const rel)
{
    if (!m_Context.inited) {
        TRXPath_Init(m_Context.args);
    }
    ASSERT(path >= 0 && path < TRX_DYNAMIC_PATH_NUMBER_OF);
    const M_DYNAMIC_PATH_POLICY *const policy = &m_PathPolicies[path];
    ASSERT(policy != nullptr);

    if (rel != nullptr && File_IsAbsolute(rel)) {
        return rel;
    }
    if (rel != nullptr) {
        const M_RESOLVE_CACHE_ENTRY *const cached =
            M_FindResolveCache(path, rel);
        if (cached != nullptr) {
            return cached->found ? cached->resolved : nullptr;
        }
    }

    M_RESOLVE_VISITOR_CONTEXT ctx = {
        .policy = policy,
        .resolved = nullptr,
    };
    M_ForEachResolveAttempt(
        path, rel, M_ResolveAttemptVisitor, &ctx, !policy->check_exists);
    if (ctx.resolved != nullptr) {
        M_SetResolveCache(path, rel, ctx.resolved);
        if (rel != nullptr) {
            const M_RESOLVE_CACHE_ENTRY *const cached =
                M_FindResolveCache(path, rel);
            if (cached != nullptr && cached->found) {
                return cached->resolved;
            }
        }
        return ctx.resolved;
    }

    M_SetResolveCache(path, rel, nullptr);
    return nullptr;
}

const char *TRXPath_TryResolve(
    const TRX_DYNAMIC_PATH path, const char *const rel)
{
    const char *const resolved = TRXPath_PeekResolve(path, rel);
    if (resolved == nullptr) {
        LOG_ERROR("%s", M_GetResolveError(path, rel));
    }
    return resolved;
}

const char *TRXPath_Resolve(const TRX_DYNAMIC_PATH path, const char *const rel)
{
    const char *const resolved = TRXPath_PeekResolve(path, rel);
    if (resolved == nullptr) {
        Shell_ExitSystem(M_GetResolveError(path, rel));
    }
    return resolved;
}

MYFILE *TRXPath_OpenFile(
    const TRX_DYNAMIC_PATH path, const char *const rel,
    const FILE_OPEN_MODE mode)
{
    const char *const resolved = TRXPath_TryResolve(path, rel);
    if (resolved == nullptr) {
        return nullptr;
    }
    return File_Open(resolved, mode);
}

bool TRXPath_LoadFile(
    const TRX_DYNAMIC_PATH path, const char *const rel, char **const out_data,
    size_t *const out_size)
{
    const char *const resolved = TRXPath_TryResolve(path, rel);
    if (resolved == nullptr) {
        if (out_data != nullptr) {
            *out_data = nullptr;
        }
        if (out_size != nullptr) {
            *out_size = 0;
        }
        return false;
    }
    return File_Load(resolved, out_data, out_size);
}

bool TRXPath_Exists(const TRX_DYNAMIC_PATH path, const char *const rel)
{
    const char *const resolved = TRXPath_PeekResolve(path, rel);
    if (resolved == nullptr) {
        return false;
    }
    return File_Exists(resolved);
}

char *TRXPath_GuessExtension(const char *const path, const char **extensions)
{
    if (!m_Context.inited) {
        TRXPath_Init(m_Context.args);
    }
    return M_GuessExtensionCached(path, extensions);
}

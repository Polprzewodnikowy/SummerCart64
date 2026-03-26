#pragma once

#include <trx/core/filesystem.h>
#include <trx/game/shell/args.h>

// Shell path module.
// This layer owns high-level path policy: token expansion (%trx_dir%),
// mod/base fallback order, case-aware canonicalization, and extension
// guessing/caching.
//
// Use these APIs for game asset/config resolution instead of ad-hoc file
// probes.

// clang-format off
#define TRX_PATH_DIR_LIST(X)                                                   \
    X(TRX_DIR,          trx_dir,          "%trx_dir%")                         \
    X(CONFIG_DIR,       config_dir,       "%config_dir%")                      \
    X(CACHE_DIR,        cache_dir,        "%cache_dir%")                      \
    X(GAMES_DIR,        games_dir,        "%games_dir%")                       \
    X(SCREENSHOTS_DIR,  screenshots_dir,  "%screenshots_dir%")                 \
    X(SAVES_DIR,        saves_dir,        "%saves_dir%")                       \
    X(LEGACY_SAVES_DIR, legacy_saves_dir, "%legacy_saves_dir%")                                                            \
    // clang-format on

typedef enum {
#define M_DIR_ENUM(name, field, token) TRX_PATH_##name,
    TRX_PATH_DIR_LIST(M_DIR_ENUM)
#undef M_DIR_ENUM
} TRX_PATH;

typedef enum {
    TRX_DYNAMIC_PATH_COMMON_CONFIG,
    TRX_DYNAMIC_PATH_CATALOG,
    TRX_DYNAMIC_PATH_GAMEFLOW_FILE,
    TRX_DYNAMIC_PATH_SHADER_FILE,
    TRX_DYNAMIC_PATH_FMV_FILE,
    TRX_DYNAMIC_PATH_LEVEL_FILE,
    TRX_DYNAMIC_PATH_SHARED_LEVEL_FILE,
    TRX_DYNAMIC_PATH_IMAGE_FILE,
    TRX_DYNAMIC_PATH_INJECTION_FILE,
    TRX_DYNAMIC_PATH_SCRIPT_FILE,
    TRX_DYNAMIC_PATH_SFX_FILE,
    TRX_DYNAMIC_PATH_CDAUDIO_FILE,
    TRX_DYNAMIC_PATH_MUSIC_DIR,
    TRX_DYNAMIC_PATH_SCREENSHOT_WRITE_FILE,
    TRX_DYNAMIC_PATH_NUMBER_OF,
} TRX_DYNAMIC_PATH;

// Initialize resolver state from shell args and environment variables.
// Safe to call multiple times; later calls refresh context-derived values.
void TRXPath_Init(const SHELL_ARGS *args);

// Expand `%token%` variables in an arbitrary string.
// Returns an owning string; caller must free.
char *TRXPath_ExpandVars(const char *in);

// Return configured root directory for a static TRX_PATH id.
// Returned pointer is owned by resolver context; do not free.
const char *TRXPath_Get(TRX_PATH path);

// Join static root and relative path.
// Returned pointer may use static formatting storage; copy if you need to keep
// it.
const char *TRXPath_Join(TRX_PATH path, const char *rel);

// Resolve with policy fallback and existence checks.
// Returns nullptr on miss; does not log.
const char *TRXPath_PeekResolve(TRX_DYNAMIC_PATH path, const char *rel);
// Same as TRXPath_PeekResolve, but logs an error on miss.
const char *TRXPath_TryResolve(TRX_DYNAMIC_PATH path, const char *rel);
// Same as TRXPath_PeekResolve, but terminates the game on miss.
const char *TRXPath_Resolve(TRX_DYNAMIC_PATH path, const char *rel);

// Resolve and open a file in one call.
// Returns nullptr if resolution/open fails.
MYFILE *TRXPath_OpenFile(
    TRX_DYNAMIC_PATH path, const char *rel, FILE_OPEN_MODE mode);

// Resolve and load file contents into memory.
// On failure, sets `out_data` to nullptr and `out_size` to 0 when provided.
bool TRXPath_LoadFile(
    TRX_DYNAMIC_PATH path, const char *rel, char **out_data, size_t *out_size);

// Resolve and check whether the file exists.
bool TRXPath_Exists(TRX_DYNAMIC_PATH path, const char *rel);

// Guess file extension (e.g. ".mp4" vs ".rpl") with case-aware resolver cache.
// Returns an owning canonical path or nullptr if no candidate exists; caller
// must free.
char *TRXPath_GuessExtension(const char *path, const char **extensions);

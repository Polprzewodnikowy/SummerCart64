#pragma once

#include <trx/core/vector.h>
#include <trx/game/shell/mod.h>

typedef struct {
    // Owned argv snapshot used as backing storage for pointer-valued fields
    // (e.g. level/replay/test paths). Freed by Shell_FreeArgs.
    VECTOR *original_args;

    int32_t engine_version;
    const SHELL_MOD *mod;
    int32_t level_to_select;
    const char *level_to_play;
    int32_t save_to_load;
    const char *test_record_path;
    const char *test_replay_path;
    bool headless;
    bool debug_render_performance;
    int32_t headless_fps; // in headless mode, force fixed fps (0 = unlocked)
    bool quiet;
} SHELL_ARGS;

// Adopts `raw_args`.
// Requirements:
// - `raw_args` items must be `char *` allocated on heap (or nullptr).
// - ownership of vector + strings transfers to returned SHELL_ARGS.
// - caller must not free or mutate `raw_args` after this call.
SHELL_ARGS *Shell_ParseArgs(VECTOR *raw_args);
// Frees SHELL_ARGS and its adopted `original_args` vector + strings.
void Shell_FreeArgs(SHELL_ARGS *args);

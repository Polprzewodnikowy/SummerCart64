#pragma once

#include <trx/core/virtual_file.h>
#include <trx/game/game_flow/types.h>

typedef enum {
    LEVEL_FORMAT_PROBE_MINIMAL,
    LEVEL_FORMAT_PROBE_STATS,
} LEVEL_FORMAT_PROBE_MODE;

typedef enum {
    LEVEL_FORMAT_LAYOUT_UNKNOWN = -1,
    LEVEL_FORMAT_LAYOUT_TR1X,
    LEVEL_FORMAT_LAYOUT_TR1,
    LEVEL_FORMAT_LAYOUT_TR1_DEMO_PC,
    LEVEL_FORMAT_LAYOUT_TR2X,
    LEVEL_FORMAT_LAYOUT_TR2,
    LEVEL_FORMAT_LAYOUT_TR3,
    LEVEL_FORMAT_LAYOUT_TR3X,
    LEVEL_FORMAT_LAYOUT_NUMBER_OF,
} LEVEL_FORMAT_LAYOUT;

typedef struct LEVEL_FORMAT_LOADER {
    int32_t game_version;
    LEVEL_FORMAT_LAYOUT layout;
    bool (*probe)(
        const struct LEVEL_FORMAT_LOADER *, VFILE *file,
        LEVEL_FORMAT_PROBE_MODE mode);
    bool (*load)(const struct LEVEL_FORMAT_LOADER *, VFILE *file);
} LEVEL_FORMAT_LOADER;

LEVEL_FORMAT_LAYOUT Level_Format_GuessLayout(VFILE *file);
const LEVEL_FORMAT_LOADER *Level_Format_GuessLoader(VFILE *file);
const LEVEL_FORMAT_LOADER *Level_Format_LoadFromFile(const GF_LEVEL *level);

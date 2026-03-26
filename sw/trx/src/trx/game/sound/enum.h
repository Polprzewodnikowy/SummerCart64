#pragma once

typedef enum {
    SAMPLE_MODE_NORMAL = 0,
    SAMPLE_MODE_WAIT = 1,
    SAMPLE_MODE_RESTART = 2,
    SAMPLE_MODE_LOOPED = 3,
} SAMPLE_MODE;

// clang-format off
typedef enum {
    SPM_NORMAL     = 0,
    SPM_UNDERWATER = 1,
    SPM_ALWAYS     = 2,
    SPM_PITCH      = 4,
    SPM_STATIC_POS = 8,
} SOUND_PLAY_MODE;
// clang-format on

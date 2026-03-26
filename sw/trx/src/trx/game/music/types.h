#pragma once

#include <stdint.h>

typedef struct MUSIC_BACKEND {
    bool (*init)(struct MUSIC_BACKEND *backend);
    const char *(*describe)(const struct MUSIC_BACKEND *backend);
    int32_t (*play)(const struct MUSIC_BACKEND *backend, int32_t track_id);
    void (*shutdown)(struct MUSIC_BACKEND *backend);
    void *data;
} MUSIC_BACKEND;

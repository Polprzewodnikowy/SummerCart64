#pragma once

#include <trx/core/math.h>
#include <trx/game/items/types.h>

#include <stdint.h>

typedef struct {
    union {
        struct {
            int32_t x;
            int32_t y;
            int32_t z;
        };
        XYZ_32 pos;
    };
    int16_t room_num;
} GAME_VECTOR;

typedef struct {
    union {
        struct {
            int32_t x;
            int32_t y;
            int32_t z;
        };
        XYZ_32 pos;
    };
    int16_t data;
    int16_t flags;
} OBJECT_VECTOR;

typedef struct {
    int32_t vertex_count;

    union {
        uint16_t texture_idx;
        uint16_t palette_idx;
    };
    uint16_t vertices[4];

    // trapezoid ratios for textured quads
    // that cannot be really shared between vertices
    TEXTURE_ZW_F texture_zw[4];

    bool double_sided;
    bool enable_reflections;
} FACE;

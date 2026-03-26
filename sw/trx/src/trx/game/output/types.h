#pragma once

#include <trx/core/colors.h>
#include <trx/core/math/types.h>
#include <trx/game/output/const.h>

#include <stdint.h>

typedef enum {
    CLIP_NOT_VISIBLE = 0,
    CLIP_PARTIALLY_VISIBLE = -1,
    CLIP_FULLY_VISIBLE = 1,
} CLIP;

typedef enum {
    TS_HEADING,
    TS_BACKGROUND,
    TS_BACKGROUND_HEAVY,
    TS_REQUESTED,
} TEXT_STYLE;

typedef enum {
    DRAW_OPAQUE = 0,
    DRAW_BLEND = 1,
    DRAW_BLEND_ADD = 2,
    DRAW_BLEND_SUB = 3,
    DRAW_REFLECTIVE_OPAQUE = 8,
    DRAW_REFLECTIVE_BLEND_ADD = 9,
} DRAW_TYPE;

typedef struct {
    int16_t value_1;
    int16_t value_2;
} SHADE;

typedef struct {
    int32_t value_1;
    int32_t value_2;
} FALLOFF;

typedef struct {
    uint16_t u;
    uint16_t v;
} TEXTURE_UV;

typedef struct {
    union {
        struct {
            float z;
            float w;
        };
        float zw[2];
    };
} TEXTURE_ZW_F;

typedef struct {
    uint16_t draw_type;
    uint16_t tex_page;
    int32_t uv_count;
    TEXTURE_UV uv[4];
} OBJECT_TEXTURE;

typedef struct {
    uint16_t tex_page;
    uint16_t offset;
    uint16_t width;
    uint16_t height;
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t flags;
} SPRITE_TEXTURE;

typedef struct ANIMATED_TEXTURE_RANGE {
    int16_t num_textures;
    int16_t *textures;
    struct ANIMATED_TEXTURE_RANGE *next_range;
} ANIMATED_TEXTURE_RANGE;

typedef struct {
    uint8_t index[256];
} LIGHT_MAP;

typedef struct {
    uint8_t index[LIGHT_MAP_SIZE];
} SHADE_MAP;

typedef struct {
    XYZ_32 from;
    XYZ_32 to;
    int32_t thickness;
} LIGHTNING_SEGMENT;

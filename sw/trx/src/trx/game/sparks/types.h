#pragma once

#include <trx/core/colors.h>
#include <trx/core/math/types.h>
#include <trx/game/output/types.h>
#include <trx/game/sparks/enum.h>

#include <stdint.h>

typedef struct SPARK {
    bool on;

    uint8_t s_life;
    uint8_t life;

    // NOTE: `pos` is either absolute world position, or a relative offset when
    // attached to an FX/ITEM and not using `SPARK_F_ATTACHED_POS`.
    XYZ_32 pos;
    XYZ_32 prev_pos;
    XYZ_32 prev_world_pos;
    XYZ_32 vel;
    struct {
        uint8_t width;
        uint8_t height;
    } src_size, dst_size, size, prev_size;

    RGB_888 src_color;
    RGB_888 dst_color;
    RGB_888 color;
    RGB_888 prev_color;

    uint8_t scalar;
    uint8_t col_fade_speed;
    uint8_t fade_to_black;
    int16_t gravity;
    int8_t max_y_vel;
    uint8_t friction;

    uint16_t flags;
    union {
        // effect/item index depending on flags (SF_FX/SF_ITEM)
        int16_t effect_num;
        int16_t item_num;
    };
    uint8_t room_num;
    uint8_t node_num;

    uint8_t extras;
    int8_t dynamic;

    uint16_t rot_angle; // 0..0xFFF
    uint16_t prev_rot_angle; // 0..0xFFF
    int8_t rot_add;

    int32_t sprite_idx;
    DRAW_TYPE draw_type;
} SPARK;

#pragma once

#include <trx/core/math.h>
#include <trx/game/items/types.h>

typedef struct {
    int32_t floor;
    int32_t ceiling;
    int32_t type;
} COLL_SIDE;

typedef enum {
    // clang-format off
    COLL_NONE      = 0x00,
    COLL_FRONT     = 0x01,
    COLL_LEFT      = 0x02,
    COLL_RIGHT     = 0x04,
    COLL_TOP       = 0x08,
    COLL_TOP_FRONT = 0x10,
    COLL_CLAMP     = 0x20,
    // clang-format on
} COLL_TYPE;

typedef struct {
    COLL_SIDE side_mid;
    COLL_SIDE side_front;
    COLL_SIDE side_left;
    COLL_SIDE side_right;
    COLL_SIDE side_left2;
    COLL_SIDE side_right2;
    int32_t radius;
    int32_t bad_pos;
    int32_t bad_neg;
    int32_t bad_ceiling;
    XYZ_32 shift;
    XYZ_32 old;
    int16_t old_anim_state;
    int16_t old_anim_num;
    int16_t old_frame_num;
    int16_t facing;
    DIRECTION quadrant;
    int16_t coll_type;
    int8_t tilt_x;
    int8_t tilt_z;
    int8_t hit_by_baddie;
    int8_t hit_static;
    // clang-format off
    uint16_t slopes_are_walls:   1; // 0x01 1
    uint16_t slopes_are_pits:    1; // 0x02 2
    uint16_t lava_is_pit:        1; // 0x04 4
    uint16_t enable_baddie_push: 1; // 0x08 8
    uint16_t enable_hit:         1; // 0x10 16
    uint16_t pad:                11;
    // clang-format on
} COLL_INFO;

typedef struct {
    XYZ_32 pos;
    int32_t r;
} SPHERE;

typedef struct {
    BOUNDS_16 bounds;
    XYZ_32 pos;
    XYZ_16 rot;
} COLL_ITEM;

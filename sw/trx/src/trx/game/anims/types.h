#pragma once

#include <trx/core/math.h>
#include <trx/game/anims/enum.h>

typedef struct {
    int16_t goal_anim_state;
    int16_t num_ranges;
    int16_t range_idx;
} ANIM_CHANGE;

typedef struct {
    int16_t start_frame;
    int16_t end_frame;
    int16_t link_anim_num;
    int16_t link_frame_num;
} ANIM_RANGE;

typedef struct {
    ANIM_COMMAND_TYPE type;
    void *data;
} ANIM_COMMAND;

typedef struct {
    int16_t fall_speed;
    int16_t speed;
} ANIM_COMMAND_VELOCITY_DATA;

typedef struct {
    int16_t frame_num;
    int16_t effect_num;
    ANIM_COMMAND_ENVIRONMENT environment;
    int16_t fx_type;
} ANIM_COMMAND_EFFECT_DATA;

typedef struct {
    bool matrix_pop;
    bool matrix_push;
    XYZ_BOOL rot;
    XYZ_32 pos;
} ANIM_BONE;

typedef struct {
    BOUNDS_16 bounds;
    XYZ_16 offset;
    XYZ_16 *mesh_rots;
} ANIM_FRAME;

typedef struct {
    ANIM_FRAME *frame_ptr;
    uint32_t frame_ofs;
    uint8_t interpolation;
    uint8_t frame_size;
    int16_t current_anim_state;
    int32_t velocity;
    int32_t acceleration;
    int16_t frame_base;
    int16_t frame_end;
    int16_t jump_anim_num;
    int16_t jump_frame_num;
    int16_t num_changes;
    int16_t change_idx;
    int16_t num_commands;
    int16_t command_idx;
    ANIM_COMMAND *commands;
} ANIM;

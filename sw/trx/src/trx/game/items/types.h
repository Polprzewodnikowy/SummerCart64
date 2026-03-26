#pragma once

#include <trx/core/math.h>
#include <trx/game/items/enum.h>
#include <trx/game/objects/ids.h>
#include <trx/game/output/types.h>

typedef struct CARRIED_ITEM {
    OBJECT_ID object_id;
    int16_t spawn_num;
    XYZ_32 pos;
    XYZ_16 rot;
    int16_t room_num;
    int16_t fall_speed;
    DROP_STATUS status;
    struct CARRIED_ITEM *next_item;
} CARRIED_ITEM;

typedef struct TRAP_DATA TRAP_DATA;
typedef struct CREATURE CREATURE;

typedef struct {
    int32_t floor;
    uint32_t touch_bits;
    uint32_t mesh_bits;
    int16_t after_death;
    OBJECT_ID object_id;
    int16_t current_anim_state;
    int16_t goal_anim_state;
    int16_t required_anim_state;
    int16_t anim_num;
    int16_t frame_num;
    int16_t prev_frame_num;
    int16_t room_num;
    int16_t next_item;
    int16_t next_active;
    int16_t speed;
    int16_t fall_speed;
    int16_t hit_points;
    int16_t max_hit_points;
    int16_t box_num;
    int16_t timer;
    uint16_t flags;
    uint8_t ai_bits;
    int16_t ai_tag;

    SHADE shade;
    union {
        CREATURE *creature_data;
        TRAP_DATA *trap_data;
    };
    int16_t *extra_rotations;
    void *priv;
    CARRIED_ITEM *carried_item;
    char *name;

    XYZ_32 pos;
    XYZ_16 rot;

    ITEM_STATUS status;
    bool enable_interpolation;
    bool enable_shadow;
    bool active;
    bool gravity;
    bool hit_status;
    bool collidable;
    bool looked_at;
    bool dynamic_light;
    bool clear_body;
    bool include_in_kill_stats;

    struct {
        struct {
            int32_t floor;
            XYZ_32 pos;
            XYZ_16 rot;
        } result, prev;
    } interp;
} ITEM;

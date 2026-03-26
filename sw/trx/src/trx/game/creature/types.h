#pragma once

#include <trx/game/creature/enum.h>
#include <trx/game/items.h>
#include <trx/game/pathing/types.h>

typedef struct CREATURE {
    union {
        // NOTE: creature extra rotations are provided via this array.
        // Intelligent objects wire item->extra_rotations to joint_rotation.
        int16_t joint_rotation[4];
        struct {
            // These are old TR1-2 aliases.
            int16_t head_rotation;
            int16_t neck_rotation;
            int16_t _extra_rotation[2];
        };
    };
    int16_t maximum_turn;
    int16_t flags;
    bool alerted;
    bool head_left;
    bool head_right;
    bool reached_goal;
    bool hurt_by_lara;
    int32_t damage_from_lara;
    bool patrol_2;
    int16_t item_num;
    MOOD_TYPE mood;
    LOT_INFO lot;
    XYZ_32 target;
    ITEM *enemy;
} CREATURE;

typedef struct {
    int16_t zone_num;
    int16_t enemy_zone_num;
    int32_t distance;
    bool ahead;
    bool bite;
    int16_t angle;
    int16_t x_angle;
    int16_t enemy_facing;
} AI_INFO;

typedef struct {
    XYZ_32 pos;
    int32_t mesh_num;
} BITE;

typedef struct {
    BITE muzzle;
    bool tr3_enemy_flash;
    BITE tr3_flash;
    int16_t tr3_enemy_weapon_flags;
    int16_t tr3_flash_shade;
    int16_t tr3_flash_rot_x;
    struct {
        BITE bite;
        RGBA_8888 color;
        float width;
    } tr3_laser;
} CREATURE_GUN;

typedef struct {
    struct {
        OBJECT_ID id;
        int16_t active_anim;
        int16_t death_anim;
        int16_t death_state;
    } land, water;
} HYBRID_INFO;

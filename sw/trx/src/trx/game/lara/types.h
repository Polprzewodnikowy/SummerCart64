#pragma once

#include <trx/game/anims.h>
#include <trx/game/creature.h>
#include <trx/game/effects/types.h>
#include <trx/game/items/types.h>
#include <trx/game/lara/enum.h>
#include <trx/game/matrix.h>
#include <trx/game/objects/common.h>
#include <trx/game/types.h>

typedef struct {
    ANIM_FRAME *frame_base;
    int16_t frame_num;
    int16_t anim_num;
    int16_t lock;
    XYZ_16 rot;
    int16_t flash_gun;

    struct {
        struct {
            XYZ_16 rot;
        } result, prev;
    } interp;
} LARA_ARM;

typedef struct {
    int32_t ammo;
} AMMO_INFO;

typedef struct {
    int16_t item_num;
    LARA_GUN_STATE gun_status;
    LARA_GUN_TYPE gun_type;
    LARA_GUN_TYPE request_gun_type;
    LARA_GUN_TYPE last_gun_type;

    LARA_WATER_STATE water_status;
    int32_t water_surface_dist;
    int16_t turn_rate;
    int16_t move_angle;
    XYZ_16 head_rot;
    XYZ_16 torso_rot;
    int16_t calc_fall_speed;
    int16_t pose_count;
    int16_t hit_frame;
    int16_t hit_direction;
    int16_t air;
    int16_t dive_timer;
    int16_t death_timer;
    int16_t sprint_timer;
    int16_t exposure_timer;
    int16_t poison_timer;
    int32_t idle_timer;
    struct {
        int32_t active;
        XZ_16 vel;
    } current;
    LOT_INFO lot;
    XYZ_32 last_pos;

    int16_t hit_effect_count;
    EFFECT *hit_effect;
    int32_t mesh_effects;
    OBJECT_MESH *mesh_ptrs[LM_NUMBER_OF];

    ITEM *target;
    int16_t target_angles[2];

    LARA_ARM left_arm;
    LARA_ARM right_arm;
    AMMO_INFO pistol_ammo;
    AMMO_INFO magnum_ammo;
    AMMO_INFO autos_ammo;
    AMMO_INFO desert_eagle_ammo;
    AMMO_INFO uzi_ammo;
    AMMO_INFO shotgun_ammo;

    struct {
        struct {
            XYZ_16 head_rot;
            XYZ_16 torso_rot;
        } result, prev;
    } interp;

    bool extra_anim;
    bool burn;
    int16_t electric;
    bool climb_status;
    bool is_crouched;
    bool keep_crouched;
    bool killed_loyal_item;

    struct {
        int32_t item_num;
        int32_t move_count;
        bool is_moving;
        XYZ_32 initial_pos;
    } interact_target;

    LARA_GUN_TYPE holsters_gun_type;
    LARA_GUN_TYPE back_gun_type;
    int16_t gun_item_num;
    AMMO_INFO harpoon_ammo;
    AMMO_INFO grenade_ammo;
    AMMO_INFO rocket_ammo;
    AMMO_INFO m16_ammo;
    AMMO_INFO mp5_ammo;
    struct {
        bool control;
        int16_t age;
        int16_t frame_num;
    } flare;

    MATRIX mesh_pos_matrices[LM_NUMBER_OF];
    bool mesh_pos_matrices_valid;

    // TR3: persistent gun smoke spawned from muzzle after firing.
    int32_t tr3_smoke_count_l;
    int32_t tr3_smoke_count_r;
    LARA_GUN_TYPE tr3_smoke_weapon;
    bool has_fired;

    // TRR modern controls stuff
    bool crouching;
    bool sprinting;
} LARA_INFO;

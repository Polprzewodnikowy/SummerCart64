#pragma once

#include <trx/config/enum.h>
#include <trx/game/fader.h>
#include <trx/game/inventory_ring/enum.h>
#include <trx/game/matrix.h>
#include <trx/game/objects/types.h>

#include <stdint.h>

#define MAX_QTY 999999
#define MAX_GLOBE_ZONES 6

typedef struct {
    int16_t shape;
    XYZ_16 pos;
    int32_t param1;
    int32_t param2;
    int16_t sprite_num;
} INVENTORY_SPRITE;

typedef enum {
    ACTION_USE = 0,
    ACTION_EXAMINE = 1,
} INVENTORY_ITEM_ACTION;

typedef struct {
    OBJECT_ID object_id;
    int16_t frames_total;
    int16_t current_frame;
    int16_t goal_frame;
    int16_t open_frame;
    int16_t anim_direction;
    int16_t anim_speed;
    int16_t anim_count;
    int16_t x_rot_pt_sel;
    int16_t x_rot_pt;
    int16_t x_rot_sel;
    int16_t x_rot_nosel;
    int16_t x_rot;
    int16_t y_rot_sel;
    int16_t y_rot;
    int16_t prev_y_rot;
    int32_t y_trans_sel;
    int32_t y_trans;
    int32_t prev_y_trans;
    int32_t z_trans_sel;
    int32_t z_trans;
    int32_t prev_z_trans;
    int16_t prev_x_rot_pt;
    int16_t prev_x_rot;
    MATRIX manual_rot;
    MATRIX prev_manual_rot;
    bool has_manual_rot;
    uint32_t meshes_sel;
    uint32_t meshes_drawn;
    int16_t inv_pos;
    INVENTORY_ITEM_ACTION action;
} INVENTORY_ITEM;

typedef struct {
    int16_t radius_target;
    int16_t radius_rate;
    int16_t camera_y_target;
    int16_t camera_y_rate;
    int16_t camera_pitch_target;
    int16_t camera_pitch_rate;
    int16_t rotate_target;
    int16_t rotate_rate;
    int16_t item_pt_x_rot_target;
    int16_t item_pt_x_rot_rate;
    int16_t item_x_rot_target;
    int16_t item_x_rot_rate;
    int32_t item_y_trans_target;
    int32_t item_y_trans_rate;
    int32_t item_z_trans_target;
    int32_t item_z_trans_rate;
    int32_t misc;
} INV_RING_MOTION;

typedef struct {
    int16_t current;
    int16_t count;
    int32_t qtys[24];
    INVENTORY_ITEM *items[24];
} INV_RING_SOURCE;

typedef struct {
    INVENTORY_MODE mode;
    INVENTORY_ITEM **list;
    RING_TYPE type;
    int16_t radius;
    int16_t prev_radius;
    int16_t camera_pitch;
    int16_t prev_camera_pitch;
    bool rotating;
    int16_t rotate_from_object;
    int16_t rotate_to_object;
    int16_t rot_count;
    int16_t current_object;
    int16_t target_object;
    int16_t number_of_objects;
    RING_STATUS status;
    RING_STATUS status_target;
    int16_t status_frames;
    int16_t angle_adder;
    int16_t rot_adder;
    int16_t rot_adder_l;
    int16_t rot_adder_r;
    struct {
        XYZ_32 pos;
        XYZ_16 rot;
    } ring_pos;
    int16_t prev_ring_rot_y;
    struct {
        XYZ_32 pos;
        XYZ_16 rot;
    } camera;
    int16_t prev_camera_y;
    XYZ_32 light;
    INV_RING_MOTION motion;

    bool is_demo_needed;
    bool is_pass_open;
    bool is_done;
    bool has_spun_out;
    int32_t old_fov;

    FADER top_fader;
    FADER back_fader;

    BACKGROUND_TYPE background_style;
    const char *background_path;
    struct {
        XYZ_16 rot;
        int32_t selection;
        uint32_t meshes_drawn;
        bool confirmed;
        bool selectable[MAX_GLOBE_ZONES];
        int32_t start_level_num[MAX_GLOBE_ZONES];
    } globe_select;
} INV_RING;

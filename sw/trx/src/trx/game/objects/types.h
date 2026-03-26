#pragma once

#include <trx/game/anims/types.h>
#include <trx/game/collision.h>
#include <trx/game/effects/types.h>
#include <trx/game/items/types.h>
#include <trx/game/pathing/types.h>
#include <trx/game/rooms/types.h>
#include <trx/game/savegame/enum.h>
#include <trx/game/types.h>

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const OBJECT_ID key_id;
    const OBJECT_ID value_id;
} GAME_OBJECT_PAIR;

typedef struct {
    void *priv;
    XYZ_16 center;
    int32_t radius;
    int16_t num_lights;
    int16_t num_vertices;
    union {
        XYZ_16 *normals;
        int16_t *lights;
    } lighting;
    XYZ_16 *vertices;

    struct {
        int16_t count;
        FACE *data;
    } all_faces, tex_faces, tex_face4s, tex_face3s, flat_faces, flat_face4s,
        flat_face3s;

    float depth_adjustment;
    bool enable_reflections;
    bool enable_caustics;
} OBJECT_MESH;

typedef struct {
    struct {
        XYZ_16 min;
        XYZ_16 max;
    } shift, rot;
    bool ignore_rot;
} OBJECT_BOUNDS;

typedef struct JSON_READ_IO JSON_READ_IO;
typedef struct JSON_WRITE_IO JSON_WRITE_IO;

typedef enum {
    OBJECT_EVENT_ALERT,
} OBJECT_EVENT;

typedef struct OBJECT {
    int16_t mesh_count;
    int16_t mesh_idx;
    int32_t bone_idx;
    uint32_t frame_ofs;
    ANIM_FRAME *frame_base;

    void (*setup_func)(struct OBJECT *obj);
    void (*initialise_func)(int16_t item_num);

    void (*control_func)(int16_t item_num);
    bool (*draw_func)(const ITEM *item);
    // NOTE: not to be union'd with draw_func, due to default draw_func impl
    // being Object_DrawAnimatingItem which takes an ITEM*
    bool (*effect_draw_func)(const EFFECT *item);

    void (*collision_func)(int16_t item_num, ITEM *lara_item, COLL_INFO *coll);
    int16_t (*floor_height_func)(
        const ITEM *item, int32_t x, int32_t y, int32_t z, int16_t height);
    int16_t (*ceiling_height_func)(
        const ITEM *item, int32_t x, int32_t y, int32_t z, int16_t height);
    void (*activate_func)(ITEM *item);
    void (*event_func)(ITEM *item, OBJECT_EVENT event, const void *data);
    bool (*trigger_func)(ITEM *item, const TRIGGER *trigger);
    bool (*gun_hit_func)(
        ITEM *item, const GAME_VECTOR *start, const GAME_VECTOR *hit_pos,
        int32_t *damage);
    void (*handle_flip_func)(ITEM *item, ROOM_FLIP_STATUS flip_status);
    void (*handle_save_func)(ITEM *item, SAVEGAME_STAGE stage);
    void (*priv_load_func)(ITEM *item, JSON_READ_IO *io);
    void (*priv_save_func)(const ITEM *item, JSON_WRITE_IO *io);
    const OBJECT_BOUNDS *(*bounds_func)(void);
    bool (*is_usable_func)(int16_t item_num);
    void (*add_walkable_func)(int16_t item_num);
    int16_t (*carrier_item_num_func)(const ITEM *item);
    bool (*can_drop_items_func)(const ITEM *item);
    bool (*can_interpolate_func)(
        const ITEM *item, int32_t frame_a, int32_t frame_b);
    bool (*should_spawn_blood_func)(const ITEM *item);
    bool (*is_alive_func)(const ITEM *item);
    bool (*is_targetable_func)(const ITEM *item);
    bool (*can_take_damage_func)(const ITEM *item);
    bool (*can_be_projectile_target_func)(const ITEM *item);
    bool (*can_be_exploded_func)(const ITEM *item);
    int32_t (*get_mesh_index_func)(const ITEM *item, int32_t mesh_idx);

    int16_t anim_idx;
    int16_t anim_count;
    int16_t hit_points;
    int16_t pivot_length;
    int16_t radius;
    int16_t shadow_size;
    int16_t smartness;
    XYZ_BOOL base_rot;
    LOT_SETUP lot_setup;

    size_t priv_size;

    bool enable_interpolation;
    bool loaded;
    bool intelligent;
    bool save_position;
    bool save_hitpoints;
    bool save_flags;
    bool save_anim;
    bool load_floor;
    bool semi_transparent;
} OBJECT;

typedef struct {
    bool loaded;
    int16_t mesh_idx;
    bool collidable;
    bool visible;
    BOUNDS_16 draw_bounds;
    BOUNDS_16 collision_bounds;
} STATIC_OBJECT_3D;

typedef struct {
    bool loaded;
    int16_t frame_count;
    int16_t texture_idx;
} STATIC_OBJECT_2D;

typedef enum {
    TRAP_SET = 0,
    TRAP_ACTIVATE = 1,
    TRAP_WORKING = 2,
    TRAP_FINISHED = 3,
} TRAP_ANIM;

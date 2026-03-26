/*
 * TRX-N64: Compatibility shim for TRX game logic
 *
 * This layer bridges TRX's portable game logic to our N64 data structures.
 * TRX uses a set of global variables and accessor functions for game state.
 * We provide implementations that operate on our tr_level_t and entity data.
 *
 * The shim covers:
 *   - Room/sector lookup (floor heights, ceiling heights, portal traversal)
 *   - Item management (entity state, linked lists per room)
 *   - Global game state (camera, Lara pointer, frame counter)
 *   - Memory allocation (mapped to our bump allocator)
 *   - Sound/output stubs (no-ops until those milestones)
 *
 * FAITHFULNESS: All game logic values (physics constants, state machine
 * transitions, collision margins, etc.) come directly from TRX source
 * with zero modifications. The shim only translates data access patterns.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TR_GAME_SHIM_H
#define TR_GAME_SHIM_H

#include "../level/tr_types.h"
#include "../anim/tr_anim.h"
#include "../render/n64_transform.h"

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Game constants (exact TR1 values)
 * ------------------------------------------------------------------- */

#define WALL_SHIFT          10      /* 1 sector = 1024 units */
#define STEP_L              256     /* Standard step height */
#define NO_HEIGHT           (-32512)
#define NO_ROOM             (-1)
#define NO_ITEM             (-1)
#define MAX_ITEMS_COUNT     256

/* Physics constants (exact TR1 values) */
#define GRAVITY             6       /* Units per frame² */
#define FASTFALL_SPEED      128     /* Terminal velocity */
#define LARA_HEIGHT         762     /* Lara's standing height */
#define LARA_RADIUS         100     /* Collision radius */
#define LARA_HITPOINTS      1000    /* Starting health */
#define LARA_AIR            1800    /* Breath frames underwater */

/* TR1 angle units */
#define DEG_1               (65536 / 360)   /* ~182 per degree */
#define DEG_90              (65536 / 4)     /* 16384 */
#define DEG_180             (65536 / 2)     /* 32768 */

/* -------------------------------------------------------------------
 * Item (entity) runtime state
 *
 * Extends the level's tr_item_t with mutable game state.
 * This is what TRX calls ITEM internally.
 * ------------------------------------------------------------------- */

typedef enum {
    IS_INACTIVE = 0,
    IS_ACTIVE = 1,
    IS_DEACTIVATED = 2,
    IS_INVISIBLE = 3,
} item_status_t;

typedef struct {
    /* Static data from level (position, rotation, object type) */
    int16_t     object_id;
    int16_t     room_num;
    tr_xyz32_t  pos;
    tr_xyz16_t  rot;

    /* Dynamic state */
    int16_t     speed;
    int16_t     fall_speed;
    int16_t     hit_points;
    int16_t     timer;
    uint16_t    flags;
    item_status_t status;

    /* Floor data */
    int32_t     floor;

    /* Animation */
    tr_anim_state_t anim;

    /* Linked list for room item chains */
    int16_t     next_item;
    int16_t     next_active;

    /* Collision bits */
    uint32_t    touch_bits;
    uint32_t    mesh_bits;

    /* State flags */
    bool        active;
    bool        gravity;
    bool        collidable;
    bool        hit_status;
} game_item_t;

/* -------------------------------------------------------------------
 * Lara-specific state
 * ------------------------------------------------------------------- */

typedef enum {
    LGT_UNARMED = 0,
    LGT_PISTOLS = 1,
    LGT_MAGNUMS = 2,
    LGT_UZIS = 3,
    LGT_SHOTGUN = 4,
} lara_gun_type_t;

typedef struct {
    int16_t     item_num;           /* Index into items array */
    int16_t     gun_status;
    lara_gun_type_t gun_type;
    lara_gun_type_t request_gun_type;

    int16_t     air;                /* Breath timer (underwater) */
    int16_t     dive_count;
    int16_t     death_count;
    int16_t     current_active;

    int32_t     water_surface_dist;
    int16_t     water_status;

    /* Head/torso rotation for look */
    tr_xyz16_t  head_rot;
    tr_xyz16_t  torso_rot;

    /* Calculated move values */
    int32_t     move_angle;

    /* Extra rotation joints */
    int16_t     *extra_rotations;
} lara_info_t;

/* -------------------------------------------------------------------
 * Collision info (used by movement code)
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t floor;
    int32_t ceiling;
    int32_t type;
} coll_side_t;

typedef struct {
    coll_side_t side_mid;
    coll_side_t side_front;
    coll_side_t side_left;
    coll_side_t side_right;
    int32_t     radius;
    int32_t     bad_pos;
    int32_t     bad_neg;
    int32_t     bad_ceiling;
    tr_xyz32_t  shift;
    tr_xyz32_t  old;
    int16_t     old_anim_state;
    int16_t     old_anim_num;
    int16_t     old_frame_num;
    int16_t     facing;
    int16_t     quadrant;
    int16_t     coll_type;
    int8_t      tilt_x;
    int8_t      tilt_z;
    bool        slopes_are_walls;
    bool        slopes_are_pits;
    bool        lava_is_pit;
    bool        enable_baddie_push;
    bool        enable_hit;
    bool        hit_static;
} coll_info_t;

/* -------------------------------------------------------------------
 * Global game state
 * ------------------------------------------------------------------- */

typedef struct {
    /* Level data (read-only) */
    const tr_level_t *level;

    /* Items (mutable runtime state) */
    game_item_t items[MAX_ITEMS_COUNT];
    int         item_count;

    /* Per-room item linked lists (first item in each room) */
    int16_t     room_item_head[TR_MAX_ROOMS];

    /* Lara state */
    lara_info_t lara;
    game_item_t *lara_item;         /* Pointer into items[] */

    /* Camera */
    tr_camera_t camera;
    int16_t     camera_room;

    /* Input (mapped from N64 controller) */
    uint32_t    input;              /* Bitmask of active inputs */
    uint32_t    input_prev;

    /* Frame counter */
    int32_t     frame_count;

    /* Flip rooms state */
    bool        flip_status;
    int16_t     flip_effect;
} game_state_t;

/* Single global game state (TRX uses globals heavily) */
extern game_state_t g_game;

/* -------------------------------------------------------------------
 * Input bits (maps to TRX's INPUT_STATE)
 * ------------------------------------------------------------------- */

#define IN_FORWARD  (1 << 0)
#define IN_BACK     (1 << 1)
#define IN_LEFT     (1 << 2)
#define IN_RIGHT    (1 << 3)
#define IN_JUMP     (1 << 4)
#define IN_ACTION   (1 << 5)
#define IN_DRAW     (1 << 6)
#define IN_LOOK     (1 << 7)
#define IN_ROLL     (1 << 8)
#define IN_WALK     (1 << 9)
#define IN_STEP_L   (1 << 10)
#define IN_STEP_R   (1 << 11)
#define IN_OPTION   (1 << 12)

/* -------------------------------------------------------------------
 * API: Initialization
 * ------------------------------------------------------------------- */

/* Initialize game state from parsed level data */
void game_init(game_state_t *gs, const tr_level_t *level);

/* -------------------------------------------------------------------
 * API: Floor/Ceiling height queries
 *
 * These are the most-called functions in TR1's game logic.
 * They determine floor and ceiling heights at any point in the world.
 * ------------------------------------------------------------------- */

/* Get the sector at a world position within a room */
const tr_sector_t *game_get_sector(const game_state_t *gs,
                                    int16_t room_num,
                                    int32_t x, int32_t z);

/* Get floor height at a point. May traverse portals to adjacent rooms.
 * Updates *room_num if the point is in a different room. */
int32_t game_get_floor_height(const game_state_t *gs,
                               const tr_sector_t *sector,
                               int32_t x, int32_t y, int32_t z);

/* Get ceiling height at a point */
int32_t game_get_ceiling_height(const game_state_t *gs,
                                 const tr_sector_t *sector,
                                 int32_t x, int32_t y, int32_t z);

/* Get the room number at a world position (portal traversal) */
int16_t game_get_room_at(const game_state_t *gs,
                          int16_t start_room,
                          int32_t x, int32_t y, int32_t z);

/* -------------------------------------------------------------------
 * API: Collision testing
 * ------------------------------------------------------------------- */

/* Test collision at entity's current position */
void game_get_collision_info(game_state_t *gs,
                              coll_info_t *coll,
                              const game_item_t *item,
                              int32_t height);

/* Shift entity out of walls */
void game_shift_item(game_item_t *item, const coll_info_t *coll);

/* -------------------------------------------------------------------
 * API: Trigger system
 * ------------------------------------------------------------------- */

/* Execute trigger commands at a sector (when Lara steps on it) */
void game_execute_trigger(game_state_t *gs, const tr_sector_t *sector);

/* Get floor height with full tilt calculation */
int32_t game_get_floor_height_full(const game_state_t *gs,
                                    const tr_sector_t *sector,
                                    int32_t x, int32_t y, int32_t z);

/* Control all active items (enemies, traps, doors) */
void game_control_items(game_state_t *gs);

/* -------------------------------------------------------------------
 * API: Per-frame update
 * ------------------------------------------------------------------- */

/* Main game tick — called once per frame at 30fps */
void game_frame_tick(game_state_t *gs);

/* Update Lara's position and state */
void game_lara_control(game_state_t *gs);

/* Update camera to follow Lara */
void game_camera_update(game_state_t *gs);

/* Map N64 controller state to game input bits */
void game_map_input(game_state_t *gs, const void *n64_input);

#endif /* TR_GAME_SHIM_H */

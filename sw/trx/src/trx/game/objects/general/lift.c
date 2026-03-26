#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/log.h>
#include <trx/core/math.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/objects/traps/movable_block.h>

#define LIFT_WAIT_TIME (3 * LOGIC_FPS) // = 90
#define LIFT_SHIFT 16
#define LIFT_HEIGHT (STEP_L * 5) // = 1280
#define LIFT_TRAVEL_DIST (STEP_L * 22)
#define M_LIFT_NUM_FLOOR_SECTORS 4
#define M_LIFT_NUM_SECTORS 8

typedef enum {
    LIFT_STATE_DOOR_CLOSED = 0,
    LIFT_STATE_DOOR_OPEN = 1,
} LIFT_STATE;

typedef enum {
    LIFT_ANIM_CLOSED = 0,
} LIFT_ANIM;

typedef struct {
    int32_t start_height;
    int32_t wait_time;
    bool is_moving;
    GAME_VECTOR linked[M_LIFT_NUM_SECTORS];
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "start_height", &p->start_height));
    JSON_SHOULD(JSON_READ(io, "wait_time", &p->wait_time));
    JSON_SHOULD(JSON_READ(io, "is_moving", &p->is_moving));
    for (int32_t i = 0; i < M_LIFT_NUM_SECTORS; i++) {
        const char *const key = String_FormatStatic("linked_%d", i);
        if (JSON_SHOULD(JSON_PUSH(io, key))) {
            JSON_SHOULD(JSON_READ(io, "x", &p->linked[i].pos.x));
            JSON_SHOULD(JSON_READ(io, "y", &p->linked[i].pos.y));
            JSON_SHOULD(JSON_READ(io, "z", &p->linked[i].pos.z));
            JSON_SHOULD(JSON_POP(io));
        }
    }
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "start_height", p->start_height);
    JSONW_WRITE(io, "wait_time", p->wait_time);
    JSONW_WRITE(io, "is_moving", p->is_moving);
    for (int32_t i = 0; i < M_LIFT_NUM_SECTORS; i++) {
        const char *const key = String_FormatStatic("linked_%d", i);
        JSONW_PUSH_OBJECT(io);
        JSONW_WRITE(io, "x", p->linked[i].pos.x);
        JSONW_WRITE(io, "y", p->linked[i].pos.y);
        JSONW_WRITE(io, "z", p->linked[i].pos.z);
        JSONW_POP_AND_SET(io, key);
    }
}

static void M_FloorCeiling(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    int32_t *const out_floor, int32_t *const out_ceiling)
{
    const XZ_32 lift_tile = {
        .x = item->pos.x >> WALL_SHIFT,
        .z = item->pos.z >> WALL_SHIFT,
    };

    ITEM *const lara_item = Lara_GetItem();
    const XZ_32 lara_tile = {
        .x = lara_item->pos.x >> WALL_SHIFT,
        .z = lara_item->pos.z >> WALL_SHIFT,
    };

    const XZ_32 test_tile = {
        .x = x >> WALL_SHIFT,
        .z = z >> WALL_SHIFT,
    };

    const DIRECTION direction = Math_GetDirection(item->rot.y);
    int32_t dx = 0;
    int32_t dz = 0;
    switch (direction) {
    case DIR_NORTH:
        dx = -1;
        dz = 1;
        break;
    case DIR_EAST:
        dx = 1;
        dz = 1;
        break;
    case DIR_SOUTH:
        dx = 1;
        dz = -1;
        break;
    case DIR_WEST:
        dx = -1;
        dz = -1;
        break;
    default:
        break;
    }

    // clang-format off
    const bool point_in_shaft =
        (test_tile.x == lift_tile.x || test_tile.x + dx == lift_tile.x) &&
        (test_tile.z == lift_tile.z || test_tile.z + dz == lift_tile.z);

    const bool lara_in_shaft =
        (lara_tile.x == lift_tile.x || lara_tile.x + dx == lift_tile.x) &&
        (lara_tile.z == lift_tile.z || lara_tile.z + dz == lift_tile.z);

    const int32_t lift_bottom = item->pos.y + STEP_L;
    const int32_t lift_floor = item->pos.y;
    const int32_t lift_ceiling = item->pos.y - LIFT_HEIGHT + STEP_L;
    const int32_t lift_top = item->pos.y - LIFT_HEIGHT;

    const bool lara_inside_lift = (lara_item->pos.y < lift_bottom) &&
                                  (lara_item->pos.y > lift_ceiling);
    // clang-format on

    *out_floor = 0x7FFF;
    *out_ceiling = -0x7FFF;

    if (lara_in_shaft) {
        if (item->current_anim_state == LIFT_STATE_DOOR_CLOSED
            && lara_inside_lift) {
            if (point_in_shaft) {
                *out_floor = lift_floor;
                *out_ceiling = lift_ceiling;
            } else {
                *out_floor = NO_HEIGHT;
                *out_ceiling = 0x7FFF;
            }
        } else if (point_in_shaft) {
            if (lara_item->pos.y < lift_ceiling) {
                *out_floor = lift_top;
            } else if (lara_item->pos.y < lift_bottom) {
                *out_floor = lift_floor;
                *out_ceiling = lift_ceiling;
            } else {
                *out_ceiling = lift_bottom;
            }
        }
    } else if (point_in_shaft) {
        if (y <= lift_top) {
            *out_floor = lift_top;
        } else if (y >= lift_bottom) {
            *out_ceiling = lift_bottom;
        } else if (item->current_anim_state == LIFT_STATE_DOOR_OPEN) {
            *out_floor = lift_floor;
            *out_ceiling = lift_ceiling;
        } else {
            *out_floor = NO_HEIGHT;
            *out_ceiling = 0x7FFF;
        }
    }
}

static int16_t M_GetFloorHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    int32_t new_floor;
    int32_t new_ceiling;
    M_FloorCeiling(item, x, y, z, &new_floor, &new_ceiling);
    if (new_floor >= height) {
        return height;
    }
    return new_floor;
}

static int16_t M_GetCeilingHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    int32_t new_floor;
    int32_t new_ceiling;
    M_FloorCeiling(item, x, y, z, &new_floor, &new_ceiling);
    if (new_ceiling <= height) {
        return height;
    }
    return new_ceiling;
}

static void M_GetSectorPositions(
    const ITEM *const item, VECTOR *const sector_pos)
{
    const XZ_32 lift_tile = {
        .x = item->pos.x >> WALL_SHIFT,
        .z = item->pos.z >> WALL_SHIFT,
    };

    // Orient.
    const DIRECTION dir = Math_GetDirection(item->rot.y);
    int32_t dx = 0, dz = 0;
    switch (dir) {
    case DIR_NORTH:
        dx = -1;
        dz = 1;
        break;
    case DIR_EAST:
        dx = 1;
        dz = 1;
        break;
    case DIR_SOUTH:
        dx = 1;
        dz = -1;
        break;
    case DIR_WEST:
        dx = -1;
        dz = -1;
        break;
    default:
        break;
    }

    // Collect a 2×2 footprint that lines up with the shaft tiles.
    for (int32_t ix = 0; ix < 2; ix++) {
        for (int32_t iz = 0; iz < 2; iz++) {
            const int32_t sx = lift_tile.x - dx * ix;
            const int32_t sz = lift_tile.z - dz * iz;

            const XYZ_32 pos = {
                .x = sx * WALL_L + WALL_L / 2,
                .y = item->pos.y,
                .z = sz * WALL_L + WALL_L / 2,
            };
            Vector_Add(sector_pos, &pos);
        }
    }

    // Collect a 2×2 footprint that lines up with the shaft ceiling tiles.
    for (int32_t ix = 0; ix < 2; ix++) {
        for (int32_t iz = 0; iz < 2; iz++) {
            const int32_t sx = lift_tile.x - dx * ix;
            const int32_t sz = lift_tile.z - dz * iz;

            const XYZ_32 pos = {
                .x = sx * WALL_L + WALL_L / 2,
                .y = item->pos.y - LIFT_HEIGHT,
                .z = sz * WALL_L + WALL_L / 2,
            };
            Vector_Add(sector_pos, &pos);
        }
    }
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->start_height = item->pos.y;
    p->wait_time = 0;
    p->is_moving = false;

    VECTOR *const positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    for (int32_t i = 0; i < positions->count; i++) {
        const GAME_VECTOR linked = {
            .pos = *(const XYZ_32 *)Vector_Get(positions, i),
            .room_num = item->room_num,
        };
        p->linked[i] = linked;
    }
    Walkable_AllocateNodes(item, positions->count);
    Vector_Free(positions);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    const int32_t bottom = p->start_height;
    const int32_t top = bottom + LIFT_TRAVEL_DIST;
    const int32_t target = Item_IsTriggerActive(item) ? top : bottom;

    if (item->pos.y == target) {
        item->goal_anim_state = LIFT_STATE_DOOR_OPEN;
        p->wait_time = 0;
        if (p->is_moving) {
            for (int32_t i = 0; i < M_LIFT_NUM_FLOOR_SECTORS; i++) {
                MovableBlock_ShiftStackY(
                    p->linked[i].pos.y, p->linked[i].pos, item->pos.y,
                    item->room_num, true);
                // Don't reposition because item->pos links to a single sector.
                p->linked[i].pos.y = item->pos.y;
            }
            for (int32_t i = M_LIFT_NUM_FLOOR_SECTORS; i < M_LIFT_NUM_SECTORS;
                 i++) {
                MovableBlock_ShiftStackY(
                    p->linked[i].pos.y, p->linked[i].pos,
                    item->pos.y - LIFT_HEIGHT, item->room_num, true);
                // Don't reposition because item->pos links to a single sector.
                p->linked[i].pos.y = item->pos.y - LIFT_HEIGHT;
            }
        }
        p->is_moving = false;
    } else if (p->wait_time < LIFT_WAIT_TIME) {
        item->goal_anim_state = LIFT_STATE_DOOR_OPEN;
        p->wait_time++;
        // Prevent Lara from interacting with blocks about to move.
        for (int32_t i = 0; i < M_LIFT_NUM_FLOOR_SECTORS; i++) {
            MovableBlock_ShiftStackY(
                p->linked[i].pos.y, p->linked[i].pos, item->pos.y,
                item->room_num, false);
        }
        for (int32_t i = M_LIFT_NUM_FLOOR_SECTORS; i < M_LIFT_NUM_SECTORS;
             i++) {
            MovableBlock_ShiftStackY(
                p->linked[i].pos.y, p->linked[i].pos, item->pos.y - LIFT_HEIGHT,
                item->room_num, false);
        }
    } else {
        item->goal_anim_state = LIFT_STATE_DOOR_CLOSED;
        p->is_moving = true;
        const int32_t delta = target - item->pos.y;
        const int32_t step = (delta > 0)
            ? (delta < LIFT_SHIFT ? delta : LIFT_SHIFT)
            : (delta > -LIFT_SHIFT ? delta : -LIFT_SHIFT);
        item->pos.y += step;
        // Raise/lower possible movable blocks on top.
        for (int32_t i = 0; i < M_LIFT_NUM_FLOOR_SECTORS; i++) {
            MovableBlock_ShiftStackY(
                p->linked[i].pos.y, p->linked[i].pos, item->pos.y,
                item->room_num, false);
        }
        // Double check linked positions on save vs load.
        for (int32_t i = M_LIFT_NUM_FLOOR_SECTORS; i < M_LIFT_NUM_SECTORS;
             i++) {
            MovableBlock_ShiftStackY(
                p->linked[i].pos.y, p->linked[i].pos, item->pos.y - LIFT_HEIGHT,
                item->room_num, false);
        }
    }

    Item_Animate(item);

    // Update room number one click up to avoid lift on a room portal.
    int16_t room_num = item->room_num;
    Room_GetSector(
        (XYZ_32) { item->pos.x, item->pos.y - STEP_L, item->pos.z }, &room_num);
    Item_UpdateRoom(item_num, room_num);
}

static void M_AddWalkable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    VECTOR *positions = Vector_Create(sizeof(XYZ_32));
    M_GetSectorPositions(item, positions);
    for (int32_t i = 0; i < positions->count; i++) {
        Walkable_Add(item_num, *(const XYZ_32 *)Vector_Get(positions, i));
    }
    Vector_Free(positions);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->floor_height_func = M_GetFloorHeight;
    obj->ceiling_height_func = M_GetCeilingHeight;
    obj->add_walkable_func = M_AddWalkable;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_LIFT, M_Setup)

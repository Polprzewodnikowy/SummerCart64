#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/game/const.h>
#include <trx/game/items/walkable.h>
#include <trx/game/objects.h>
#include <trx/game/objects/traps/movable_block.h>
#include <trx/game/rooms.h>

typedef enum {
    // clang-format off
    PILLAR_STATE_READY_FORWARD = 0,
    PILLAR_STATE_READY_BACK    = 1,
    PILLAR_STATE_MOVING        = 2,
    // clang-format on
} PILLAR_STATE;

typedef enum {
    // clang-format off
    PILLAR_ANIM_READY_FORWARD = 0,
    PILLAR_ANIM_READY_BACK    = 1,
    PILLAR_ANIM_FORWARD       = 2,
    PILLAR_ANIM_BACK          = 3,
    // clang-format on
} PILLAR_ANIM;

typedef struct {
    GAME_VECTOR initial;
    GAME_VECTOR linked;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    if (JSON_SHOULD(JSON_PUSH(io, "linked"))) {
        JSON_SHOULD(JSON_READ(io, "x", &p->linked.pos.x));
        JSON_SHOULD(JSON_READ(io, "y", &p->linked.pos.y));
        JSON_SHOULD(JSON_READ(io, "z", &p->linked.pos.z));
        JSON_SHOULD(JSON_POP(io));
    }
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "x", p->linked.pos.x);
    JSONW_WRITE(io, "y", p->linked.pos.y);
    JSONW_WRITE(io, "z", p->linked.pos.z);
    JSONW_POP_AND_SET(io, "linked");
}

static void M_SetInitial(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    p->initial.pos = item->pos;
    p->initial.room_num = item->room_num;
}

static GAME_VECTOR M_GetInitial(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p->initial;
}

static void M_SetLinked(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    p->linked.pos = item->pos;
    p->linked.room_num = item->room_num;
}

static GAME_VECTOR M_GetLinked(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p->linked;
}

static bool M_IsItemOnTop(
    const ITEM *const item, const int32_t x, const int32_t z)
{
    int32_t dx = x - item->pos.x;
    int32_t dz = z - item->pos.z;

    // Movable blocks' bounds don't match sector so estimate.
    return (dx >= -WALL_L / 2 && dx < WALL_L / 2)
        && (dz >= -WALL_L / 2 && dz < WALL_L / 2);
}

static int16_t M_GetFloorHeight(
    const ITEM *const item, int32_t x, int32_t y, int32_t z, int16_t height)
{
    if (item->status == IS_INVISIBLE) {
        return height;
    }

    if (item->current_anim_state == PILLAR_STATE_MOVING) {
        return height;
    }

    if (!M_IsItemOnTop(item, x, z)) {
        return height;
    }

    // If under the bottom of the block.
    if (y > item->pos.y) {
        return height;
    }

    // If the the top of the block is under the floor height.
    if (item->pos.y - WALL_L * 2 >= height) {
        return height;
    }

    return item->pos.y - WALL_L * 2;
}

static int16_t M_GetCeilingHeight(
    const ITEM *item, int32_t x, int32_t y, int32_t z, int16_t height)
{
    if (item->status == IS_INVISIBLE) {
        return height;
    }

    if (item->current_anim_state == PILLAR_STATE_MOVING) {
        return height;
    }

    // Only care if we are inside the block footprint.
    if (!M_IsItemOnTop(item, x, z)) {
        return height;
    }

    // If above the top of the block.
    if (y <= item->pos.y - WALL_L * 2) {
        return height;
    }

    // If the the bottom of the block is above the ceiling height.
    if (item->pos.y <= height) {
        return height;
    }

    return item->pos.y;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    MovableBlock_UpdateBox(item, false);
    M_SetInitial(item);
    M_SetLinked(item);
    Walkable_AllocateNodes(item, 1);
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_BEFORE_LOAD) {
        MovableBlock_UpdateBox(item, false);
    } else if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        const int16_t item_num = Item_GetIndex(item);
        // Reposition walkable to its linked sector.
        Walkable_Reposition(item_num, M_GetInitial(item), M_GetLinked(item));
        MovableBlock_UpdateBox(item, true);
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (Item_IsTriggerActive(item)) {
        if (item->current_anim_state == PILLAR_STATE_READY_FORWARD) {
            item->goal_anim_state = PILLAR_STATE_READY_BACK;
        }
    } else if (item->current_anim_state == PILLAR_STATE_READY_BACK) {
        item->goal_anim_state = PILLAR_STATE_READY_FORWARD;
    }

    Item_Animate(item);
    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    const GAME_VECTOR linked = M_GetLinked(item);

    if (item->status == IS_ACTIVE
        && (item->pos.x != linked.pos.x || item->pos.z != linked.pos.z)) {
        MovableBlock_SlideStack(
            item->pos.y - WALL_L * 2, linked.pos, item, false);
        MovableBlock_UpdateBox(item, false);
    } else if (
        item->status == IS_DEACTIVATED
        && (item->pos.x != linked.pos.x || item->pos.z != linked.pos.z)) {
        item->pos.x &= -WALL_L;
        item->pos.x += WALL_L / 2;
        item->pos.z &= -WALL_L;
        item->pos.z += WALL_L / 2;
        const GAME_VECTOR target = {
            .pos = item->pos,
            .room_num = item->room_num,
        };
        Walkable_Reposition(item_num, linked, target);
        M_SetLinked(item);

        // Reposition possible movable blocks on top.
        MovableBlock_SlideStack(
            item->pos.y - WALL_L * 2, linked.pos, item, true);

        MovableBlock_UpdateBox(item, true);
        item->status = IS_ACTIVE;
    }
}

static void M_AddWalkable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    Walkable_Add(item_num, item->pos);
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->control_func = M_Control;
    obj->floor_height_func = M_GetFloorHeight;
    obj->ceiling_height_func = M_GetCeilingHeight;
    obj->save_position = true;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->add_walkable_func = M_AddWalkable;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
}

REGISTER_OBJECT(O_SLIDING_PILLAR, M_Setup)

#include <trx/game/objects/traps/movable_block.h>

#include <trx/config.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define LF_PPREADY 19

typedef enum {
    MOVABLE_BLOCK_STATE_STILL = 1,
    MOVABLE_BLOCK_STATE_PUSH = 2,
    MOVABLE_BLOCK_STATE_PULL = 3,
} MOVABLE_BLOCK_STATE;

typedef struct {
    uint16_t gravity_frames;
    bool is_push_pull;
    bool is_forced_moving;
    int16_t extra_rotations[3];
    int16_t original_rot;
    int16_t interaction_rot;
    GAME_VECTOR initial;
    GAME_VECTOR linked;
} M_PRIV;

static const OBJECT_BOUNDS m_MovableBlock_Bounds = {
    .shift = {
        .min = { .x = -300, .y = 0, .z = -WALL_L / 2 - (LARA_RADIUS + 80), },
        .max = { .x = +300, .y = 0, .z = -WALL_L / 2, },
    },
    .rot = {
        .min = { .x = -10 * DEG_1, .y = -30 * DEG_1, .z = -10 * DEG_1, },
        .max = { .x = +10 * DEG_1, .y = +30 * DEG_1, .z = +10 * DEG_1, },
    },
};

// Collect a stack of blocks.
static void M_GetStack(
    VECTOR *stack, XYZ_32 stack_pos, int32_t stack_height, int32_t step_y,
    int16_t room_num);

// Restores blocks' original texturing in case they have unique textures on each
// side. The game rotates the block in order to align the block with Lara when
// she tries to push or pull.
static void M_UpdateRotation(ITEM *const item, const int16_t rot_y)
{
    item->rot.y = rot_y;
    M_PRIV *const p = item->priv;
    // All 3 indices are potentially used in other parts of the code that can
    // cast item->extra_rotations to structs such as XYZ_16. This is similar to
    // things such as the compass needle that apply extra rotation.
    p->extra_rotations[0] = p->original_rot - rot_y;
}

// Indicates if Lara is currently pushing or pulling a block.
static void M_SetPushPull(ITEM *const item, const bool enable)
{
    M_PRIV *const p = item->priv;
    p->is_push_pull = enable;
}

static bool M_IsPushPull(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr && p->is_push_pull;
}

// Indicates if blocks are being forcefully moved by other objects such as
// lifts.
static void M_SetForcedMoving(ITEM *const item, const bool enable)
{
    M_PRIV *const p = item->priv;
    p->is_forced_moving = enable;
}

static bool M_IsForcedMoving(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr && p->is_forced_moving;
}

// If a stack of multiple blocks need to drop, each subsequently stacked block
// is delayed by incrementing frames so that higher blocks don't "land" on lower
// blocks and stop moving.
static void M_SetGravityFrames(ITEM *const item, const uint8_t frames)
{
    M_PRIV *const p = item->priv;
    p->gravity_frames = frames;
}

static uint16_t M_GetGravityFrames(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr ? p->gravity_frames : 0;
}

// Handles the block's initial position and room number for walkables.
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

// Handles the block's linked position and room number for walkables.
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

static void M_UpdateStoppers(const ITEM *const item, const bool enabled)
{
    const M_PRIV *const p = item->priv;
    int16_t dir = p->interaction_rot;
    if (!enabled) {
        dir += DEG_180;
    }
    const ROOM *room = Room_Get(item->room_num);
    SECTOR *sector = Room_GetWorldSector(room, item->pos.x, item->pos.z);
    sector->stopper = enabled;

    const XYZ_32 pos = XYZ_32_OffsetYaw(item->pos, dir, WALL_L);
    int16_t room_num = item->room_num;
    Room_GetSector(pos, &room_num);
    room = Room_Get(room_num);
    sector = Room_GetWorldSector(room, pos.x, pos.z);
    sector->stopper = enabled;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "gravity_frames", &p->gravity_frames));
    JSON_SHOULD(JSON_READ(io, "is_push_pull", &p->is_push_pull));
    JSON_SHOULD(JSON_READ(io, "is_forced_moving", &p->is_forced_moving));

    if (JSON_SHOULD(JSON_PUSH(io, "linked"))) {
        JSON_SHOULD(JSON_READ(io, "x", &p->linked.pos.x));
        JSON_SHOULD(JSON_READ(io, "y", &p->linked.pos.y));
        JSON_SHOULD(JSON_READ(io, "z", &p->linked.pos.z));
        JSON_SHOULD(JSON_POP(io));
    }

    JSON_SHOULD(JSON_READ(io, "counter_rot_0", &p->extra_rotations[0]));
    JSON_SHOULD(JSON_READ(io, "counter_rot_1", &p->extra_rotations[1]));
    JSON_SHOULD(JSON_READ(io, "counter_rot_2", &p->extra_rotations[2]));
    JSON_SHOULD(JSON_READ(io, "original_rot", &p->original_rot));
    JSON_SHOULD(JSON_READ(io, "interaction_rot", &p->interaction_rot));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "gravity_frames", p->gravity_frames);
    JSONW_WRITE(io, "is_push_pull", p->is_push_pull);
    JSONW_WRITE(io, "is_forced_moving", p->is_forced_moving);

    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "x", p->linked.pos.x);
    JSONW_WRITE(io, "y", p->linked.pos.y);
    JSONW_WRITE(io, "z", p->linked.pos.z);
    JSONW_POP_AND_SET(io, "linked");

    JSONW_WRITE(io, "counter_rot_0", p->extra_rotations[0]);
    JSONW_WRITE(io, "counter_rot_1", p->extra_rotations[1]);
    JSONW_WRITE(io, "counter_rot_2", p->extra_rotations[2]);
    JSONW_WRITE(io, "original_rot", p->original_rot);
    JSONW_WRITE(io, "interaction_rot", p->interaction_rot);
}

static bool M_TestCurrentSector(ITEM *item, int32_t block_height)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);

    // Check if there is a hard wall above.
    if (Room_GetHeight(sector, item->pos) == NO_HEIGHT) {
        return true;
    }

    // Make sure there is nothing on top of the block.
    if (Room_GetHeight(
            sector,
            (XYZ_32) { item->pos.x, item->pos.y - block_height, item->pos.z })
        != item->pos.y - block_height) {
        return false;
    }

    return true;
}

static bool M_TestPush(ITEM *item, int32_t block_height, DIRECTION quadrant)
{
    if (!M_TestCurrentSector(item, block_height)) {
        return false;
    }

    XYZ_32 base_pos = item->pos;
    int16_t room_num = item->room_num;

    switch (quadrant) {
    case DIR_NORTH:
        base_pos.z += WALL_L;
        break;
    case DIR_EAST:
        base_pos.x += WALL_L;
        break;
    case DIR_SOUTH:
        base_pos.z -= WALL_L;
        break;
    case DIR_WEST:
        base_pos.x -= WALL_L;
        break;
    default:
        break;
    }

    const SECTOR *sector = Room_GetSector(base_pos, &room_num);
    COLL_INFO coll = {
        .quadrant = quadrant,
        .radius = 500,
    };
    if (Collide_CollideStaticObjects(
            &coll, base_pos.x, base_pos.y, base_pos.z, room_num, 1000)) {
        return false;
    }

    if (Room_GetHeight(sector, base_pos) != base_pos.y) {
        return false;
    }

    if (sector->stopper) {
        return false;
    }

    const XYZ_32 sample_pos = { base_pos.x, base_pos.y - block_height,
                                base_pos.z };
    sector = Room_GetSector(sample_pos, &room_num);
    if (Room_GetCeiling(sector, sample_pos) > base_pos.y - block_height) {
        return false;
    }

    return true;
}

static bool M_TestPull(ITEM *item, int32_t block_height, DIRECTION quadrant)
{
    if (!M_TestCurrentSector(item, block_height)) {
        return false;
    }

    int32_t x_add = 0;
    int32_t z_add = 0;
    switch (quadrant) {
    case DIR_NORTH:
        z_add = -WALL_L;
        break;
    case DIR_EAST:
        x_add = -WALL_L;
        break;
    case DIR_SOUTH:
        z_add = WALL_L;
        break;
    case DIR_WEST:
        x_add = WALL_L;
        break;
    default:
        break;
    }

    // Test block destination sector.
    XYZ_32 base_pos = {
        .x = item->pos.x + x_add,
        .y = item->pos.y,
        .z = item->pos.z + z_add,
    };
    int16_t room_num = item->room_num;
    const SECTOR *sector = Room_GetSector(base_pos, &room_num);

    COLL_INFO coll = {
        .quadrant = quadrant,
        .radius = 500,
    };
    if (Collide_CollideStaticObjects(
            &coll, base_pos.x, base_pos.y, base_pos.z, room_num, 1000)) {
        return false;
    }

    if (Room_GetHeight(sector, base_pos) != base_pos.y) {
        return false;
    }

    if (sector->stopper) {
        return false;
    }

    XYZ_32 sample_pos = { base_pos.x, base_pos.y - block_height, base_pos.z };
    sector = Room_GetSector(sample_pos, &room_num);
    if (Room_GetCeiling(sector, sample_pos) > base_pos.y - block_height) {
        return false;
    }

    // Test Lara destination sector.
    base_pos.x += x_add;
    base_pos.z += z_add;
    room_num = item->room_num;
    sector = Room_GetSector(base_pos, &room_num);
    if (Room_GetHeight(sector, base_pos) != base_pos.y) {
        return false;
    }

    sample_pos = (XYZ_32) { base_pos.x, base_pos.y - LARA_HEIGHT, base_pos.z };
    sector = Room_GetSector(sample_pos, &room_num);
    if (Room_GetCeiling(sector, sample_pos) > base_pos.y - LARA_HEIGHT) {
        return false;
    }

    ITEM *const lara_item = Lara_GetItem();
    base_pos.x = lara_item->pos.x + x_add;
    base_pos.y = lara_item->pos.y;
    base_pos.z = lara_item->pos.z + z_add;
    room_num = lara_item->room_num;
    sector = Room_GetSector(base_pos, &room_num);
    coll.radius = LARA_RADIUS;
    coll.quadrant = (quadrant + 2) & 3;
    if (Collide_CollideStaticObjects(
            &coll, base_pos.x, base_pos.y, base_pos.z, room_num, LARA_HEIGHT)) {
        return false;
    }

    return true;
}

static bool M_TestDoor(ITEM *lara_item, COLL_INFO *coll)
{
    // OG fix: stop pushing blocks through doors

    const int32_t shift = 8; // constant shift to avoid overflow errors
    const int32_t max_dist = SQUARE((WALL_L * 2) >> shift);
    for (int item_num = 0; item_num < Item_GetLevelCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (!Object_IsType(item->object_id, g_DoorObjects)) {
            continue;
        }

        const int32_t dx = (item->pos.x - lara_item->pos.x) >> shift;
        const int32_t dy = (item->pos.y - lara_item->pos.y) >> shift;
        const int32_t dz = (item->pos.z - lara_item->pos.z) >> shift;
        const int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
        if (dist > max_dist) {
            continue;
        }

        if (Lara_TestBoundsCollide(item, coll->radius)
            && Collide_TestCollision(item, lara_item)) {
            return true;
        }
    }
    return false;
}

static bool M_TestDeathCollision(const ITEM *const item, const ITEM *const lara)
{
    return g_Config.gameplay.enable_killer_pushblocks
        && !g_Config.debug.enable_invulnerability && item->gravity
        && Lara_TestBoundsCollide(item, 0);
}

static bool M_IsItemOnTop(
    const ITEM *const item, const int32_t x, const int32_t z)
{
    const int32_t dx = x - item->pos.x;
    const int32_t dz = z - item->pos.z;

    // Movable blocks' bounds don't match sector so estimate.
    return (dx >= -WALL_L / 2 && dx < WALL_L / 2)
        && (dz >= -WALL_L / 2 && dz < WALL_L / 2);
}

static bool M_TestEmbedCollision(const ITEM *const item, const ITEM *const lara)
{
    return M_IsItemOnTop(item, lara->pos.x, lara->pos.z)
        && lara->pos.y <= item->pos.y && lara->pos.y > item->pos.y - WALL_L
        && !item->gravity && !lara->gravity
        && item->current_anim_state == MOVABLE_BLOCK_STATE_STILL
        && lara->current_anim_state != LS(LS_PULL_BLOCK)
        && lara->current_anim_state != LS(LS_PUSH_BLOCK);
}

static void M_KillLara(const ITEM *const item, ITEM *const lara)
{
    if (lara->hit_points <= 0) {
        return;
    }

    lara->hit_points = -1;
    lara->pos.y = lara->floor;
    lara->speed = 0;
    lara->fall_speed = 0;
    lara->gravity = false;
    lara->rot.x = 0;
    lara->rot.z = 0;
    lara->enable_shadow = false;
    lara->current_anim_state = LS(LS_SPECIAL);
    lara->goal_anim_state = LS(LS_SPECIAL);
    Item_SwitchToAnim(lara, LA(LA_BOULDER_DEATH), 0);

    for (int32_t i = 0; i < 15; i++) {
        const int32_t x = lara->pos.x + (Random_GetControl() - 0x4000) / 256;
        const int32_t z = lara->pos.z + (Random_GetControl() - 0x4000) / 256;
        const int32_t y = lara->pos.y - Random_GetControl() / 64;
        const int32_t d = lara->rot.y + (Random_GetControl() - 0x4000) / 8;
        Spawn_Blood(x, y, z, item->speed * 2, d, lara->room_num);
    }

    if (!Object_Get(O_CAMERA_TARGET)->loaded) {
        return;
    }

    const int16_t target_num = Item_Spawn(lara, O_CAMERA_TARGET);
    if (target_num != NO_ITEM) {
        ITEM *const target = Item_Get(target_num);
        target->rot.y = g_Camera.target_angle;
        target->pos.y = lara->floor - WALL_L;
        Lara_SetDeathCameraTarget(target_num);
    }
}

static bool M_IsAgainstFloor(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    return sector->floor.tilt == 0 && sector->floor.height == item->pos.y;
}

static bool M_IsAgainstCeiling(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const SECTOR *const sky_sector =
        Room_GetSkySector(sector, item->pos.x, item->pos.z);
    return sky_sector->ceiling.tilt == 0
        && sky_sector->ceiling.height == item->pos.y - WALL_L;
}

static const OBJECT_BOUNDS *M_Bounds(void)
{
    return &m_MovableBlock_Bounds;
}

static bool M_Draw(const ITEM *const item)
{
    if (item->status == IS_ACTIVE) {
        return Object_DrawUnclippedItem(item);
    } else {
        return Object_DrawAnimatingItem(item);
    }
}

static void M_Initialise(const int16_t item_num)
{
    // Ensure the block is snapped to the grid, otherwise the snapping occurs
    // during collision tests and can appear jarring. Additional angles are
    // stored to preserve item appearance in spite of control angle changes.
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    item->extra_rotations = p->extra_rotations;
    p->original_rot = (((item->rot.y + DEG_180) / DEG_90) * DEG_90) - DEG_180;

    M_UpdateRotation(item, p->original_rot);
    M_SetGravityFrames(item, 0);
    M_SetPushPull(item, false);
    M_SetForcedMoving(item, false);
    M_SetInitial(item);
    M_SetLinked(item);
    MovableBlock_UpdateBox(item, true);
    Walkable_AllocateNodes(item, 1);
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_BEFORE_LOAD) {
        MovableBlock_UpdateBox(item, false);
    } else if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        const OBJECT *const obj = Object_Get(item->object_id);
        if (item->anim_num < obj->anim_idx
            || item->anim_num >= obj->anim_idx + obj->anim_count) {
            // #4735 - resolve save issues caused by injections shifting anim
            // numbers. Remove after some time.
            Item_SwitchToAnim(item, 0, 0);
        }

        const int16_t item_num = Item_GetIndex(item);
        if (item->flags & IF_KILLED) {
            Walkable_Remove(item_num);
            return;
        }
        if (item->status == IS_ACTIVE && !item->gravity
            && !M_IsForcedMoving(item)
            && item->current_anim_state == MOVABLE_BLOCK_STATE_STILL) {
            Item_RemoveActive(Item_GetIndex(item));
            item->status = IS_INACTIVE;
        }

        // Reposition walkable to its linked sector.
        Walkable_Reposition(item_num, M_GetInitial(item), M_GetLinked(item));
        MovableBlock_UpdateBox(item, true);
    }
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);

    if (item->status == IS_INVISIBLE) {
        return;
    }

    if (M_TestDeathCollision(item, lara_item)) {
        M_KillLara(item, lara_item);
        return;
    }

    if (M_TestEmbedCollision(item, lara_item)) {
        lara_item->pos.y = item->pos.y - WALL_L;
    }

    if (item->current_anim_state == MOVABLE_BLOCK_STATE_STILL) {
        M_SetPushPull(item, false);
    }

    if (!g_Input.action || item->status == IS_ACTIVE || lara_item->gravity
        || lara_item->pos.y != item->pos.y || M_IsForcedMoving(item)) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const DIRECTION quadrant = Math_GetDirection(lara_item->rot.y);
    if (lara_item->current_anim_state == LS(LS_STOP)) {
        if (g_Input.forward || g_Input.back
            || lara->gun_status != LGS_ARMLESS) {
            return;
        }

        switch (quadrant) {
        case DIR_NORTH:
            M_UpdateRotation(item, 0);
            break;
        case DIR_EAST:
            M_UpdateRotation(item, DEG_90);
            break;
        case DIR_SOUTH:
            M_UpdateRotation(item, -DEG_180);
            break;
        case DIR_WEST:
            M_UpdateRotation(item, -DEG_90);
            break;
        default:
            break;
        }

        if (!Lara_TestPosition(item, obj->bounds_func())) {
            return;
        }

        // OG fix: stop pushing blocks through doors
        if (M_TestDoor(lara_item, coll)) {
            return;
        }

        int16_t room_num = lara_item->room_num;
        Room_GetSector(
            (XYZ_32) { item->pos.x, item->pos.y - STEP_L / 2, item->pos.z },
            &room_num);
        if (room_num != item->room_num) {
            return;
        }

        switch (quadrant) {
        case DIR_NORTH:
            lara_item->pos.z = ROUND_TO_SECTOR(lara_item->pos.z);
            lara_item->pos.z += WALL_L - LARA_RADIUS;
            break;
        case DIR_SOUTH:
            lara_item->pos.z = ROUND_TO_SECTOR(lara_item->pos.z);
            lara_item->pos.z += LARA_RADIUS;
            break;
        case DIR_EAST:
            lara_item->pos.x = ROUND_TO_SECTOR(lara_item->pos.x);
            lara_item->pos.x += WALL_L - LARA_RADIUS;
            break;
        case DIR_WEST:
            lara_item->pos.x = ROUND_TO_SECTOR(lara_item->pos.x);
            lara_item->pos.x += LARA_RADIUS;
            break;
        default:
            break;
        }

        lara_item->rot.y = item->rot.y;
        lara_item->goal_anim_state = LS(LS_PP_READY);

        Lara_Animate(lara_item);

        if (lara_item->current_anim_state == LS(LS_PP_READY)) {
            lara->gun_status = LGS_HANDS_BUSY;
        }
    } else if (Item_TestAnimEqual(lara_item, LA(LA_PUSHABLE_GRAB))) {
        if (!Item_TestFrameEqual(lara_item, LF_PPREADY)) {
            return;
        }

        if (!Lara_TestPosition(item, obj->bounds_func())) {
            return;
        }

        M_PRIV *const p = item->priv;
        if (g_Input.forward) {
            if (!M_TestPush(item, WALL_L, quadrant)) {
                return;
            }
            p->interaction_rot = lara_item->rot.y;
            item->goal_anim_state = MOVABLE_BLOCK_STATE_PUSH;
            lara_item->goal_anim_state = LS(LS_PUSH_BLOCK);
        } else if (g_Input.back) {
            if (!M_TestPull(item, WALL_L, quadrant)) {
                return;
            }
            p->interaction_rot = lara_item->rot.y + DEG_180;
            item->goal_anim_state = MOVABLE_BLOCK_STATE_PULL;
            lara_item->goal_anim_state = LS(LS_PULL_BLOCK);
        } else {
            return;
        }

        M_SetLinked(item);
        item->status = IS_ACTIVE;
        Item_AddActive(item_num);
        M_UpdateStoppers(item, true);
        MovableBlock_UpdateBox(item, false);
        Item_Animate(item);
        Lara_Animate(lara_item);
        M_SetPushPull(item, true);
    }
}

static void M_ResetPosition(ITEM *const item)
{
    const int16_t item_num = Item_GetIndex(item);
    const GAME_VECTOR linked_pos = M_GetLinked(item);
    const GAME_VECTOR initial_pos = M_GetInitial(item);

    MovableBlock_UpdateBox(item, false);
    item->pos = initial_pos.pos;
    Item_UpdateRoom(item_num, initial_pos.room_num);
    Walkable_Reposition(item_num, linked_pos, initial_pos);
    M_SetLinked(item);
    MovableBlock_UpdateBox(item, true);

    Item_RemoveActive(item_num);
    item->timer = -1;
    item->status = IS_INACTIVE;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->status == IS_INVISIBLE) {
        return;
    }

    if (item->timer > 0 && !M_IsPushPull(item) && !M_IsForcedMoving(item)) {
        M_ResetPosition(item);
        return;
    }

    if (M_GetGravityFrames(item) > 0) {
        M_SetGravityFrames(item, M_GetGravityFrames(item) - 1);
        return;
    }

    if ((item->flags & IF_ONE_SHOT) != 0) {
        Item_Kill(item_num);
        Walkable_Remove(item_num);
        MovableBlock_UpdateBox(item, false);
        return;
    }

    Item_Animate(item);

    // Check if the block is floating, on a walkable, or on the pit floor.
    // ROUND_TO_HALF_CLICK because block can fall through floor to undefined
    // sector.
    int16_t room_num = item->room_num;
    XYZ_32 sample_pos = {
        item->pos.x,
        ROUND_TO_HALF_CLICK(item->pos.y),
        item->pos.z,
    };
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);
    int32_t under_block_height =
        Room_GetHeightEx(sector, item->pos, true, item_num);

    bool update_room_num = true;

    // Check if tunneled into floor below.
    if (item->gravity && item->fall_speed > 0) {
        const int32_t y_prev = item->pos.y - item->fall_speed;

        // Query floor at previous y position.
        sample_pos.y = y_prev;
        const SECTOR *prev_sector = Room_GetSector(sample_pos, &room_num);
        int32_t prev_height =
            Room_GetHeightEx(prev_sector, sample_pos, true, item_num);

        // If on a walkable at the previous y position, use the rounded previous
        // y position as the floor.
        if (Room_IsOnWalkable(
                prev_sector,
                (XYZ_32) {
                    item->pos.x,
                    ROUND_TO_HALF_CLICK(y_prev),
                    item->pos.z,
                },
                ROUND_TO_HALF_CLICK(y_prev), item_num)) {
            prev_height = ROUND_TO_HALF_CLICK(y_prev);
        }

        // If tunneled into the floor, clamp to previous floor height.
        if (prev_height != NO_HEIGHT && y_prev < prev_height
            && item->pos.y >= prev_height) {
            under_block_height = prev_height;
            update_room_num = false;
        }
    }

    if (item->pos.y < under_block_height && !M_IsPushPull(item)
        && !M_IsForcedMoving(item)) {
        // Block is activated and floating in the air.
        item->gravity = true;
    } else if (item->gravity) {
        // Block hits the ground or another walkable.
        item->gravity = false;
        item->fall_speed = 0;
        item->pos.y = under_block_height;
        item->status = IS_DEACTIVATED;
        ItemAction_Run(ITEM_ACTION_FLOOR_SHAKE, item);
        Sound_Effect(SFX_PUSHBLOCK_LAND, &item->pos, SPM_NORMAL);
    } else if (
        // If block is at/under floor height, no gravity, and isn't being
        // pushed/pulled anymore. Prevents blocks from getting stuck in
        // IS_INACTIVE if retriggered.
        item->pos.y >= under_block_height && !item->gravity
        && !M_IsPushPull(item) && !M_IsForcedMoving(item)) {
        item->status = IS_INACTIVE;
        Item_RemoveActive(item_num);
    }

    // Don't update room number if on a walkable because room number can fall
    // through to a pit room (e.g. trapdoors).
    if (update_room_num) {
        room_num = item->room_num;
        Room_GetSectorOnWalkable(
            (XYZ_32) { item->pos.x, item->pos.y - WALL_L, item->pos.z },
            &room_num);
        Item_UpdateRoom(item_num, room_num);
    }

    if (item->status == IS_DEACTIVATED) {
        const GAME_VECTOR target = {
            .pos = item->pos,
            .room_num = item->room_num,
        };
        Walkable_Reposition(item_num, M_GetLinked(item), target);
        M_SetLinked(item);
        item->status = IS_INACTIVE;
        Item_RemoveActive(item_num);
        M_UpdateStoppers(item, false);
        MovableBlock_UpdateBox(item, true);
        Room_TestTriggers(item);
    }
}

static int16_t M_GetFloorHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (item->status == IS_INVISIBLE || item->gravity) {
        return height;
    }

    // TODO OG bug: camera and shadow behave like OG during push/pull.
    if (M_IsPushPull(item)) {
        return height;
    }

    if (!M_IsItemOnTop(item, x, z)) {
        return height;
    }

    if (M_IsAgainstFloor(item) && M_IsAgainstCeiling(item)) {
        return NO_HEIGHT;
    }

    // If partially embedded from below e.g. jumping up into an overhead block.
    if (y <= item->pos.y && y > item->pos.y - WALL_L
        && M_IsAgainstCeiling(item)) {
        const SECTOR *const sector = Room_GetWorldSector(
            Room_Get(item->room_num), item->pos.x, item->pos.z);
        if (item->pos.y < sector->floor.height) {
            // If partially embedded from below e.g. jumping up into an overhead
            // block.
            return height;
        } else if (M_IsAgainstFloor(item)) {
            // Clamped between floor and ceiling. Match up with similar case in
            // M_GetCeilingHeight to return same sentinel value;
            return item->pos.y - WALL_L;
        }
    }

    // If under the bottom of the block.
    if (y > item->pos.y) {
        return height;
    }

    // If the the top of the block is under the floor height.
    if (item->pos.y - WALL_L >= height) {
        return height;
    }

    return item->pos.y - WALL_L;
}

static int16_t M_GetCeilingHeight(
    const ITEM *const item, const int32_t x, const int32_t y, const int32_t z,
    const int16_t height)
{
    if (item->status == IS_INVISIBLE || item->gravity) {
        return height;
    }

    // TODO OG bug: camera and shadow behave like OG during push/pull.
    if (M_IsPushPull(item)) {
        return height;
    }

    // Only care if we are inside the block footprint.
    if (!M_IsItemOnTop(item, x, z)) {
        return height;
    }

    if (M_IsAgainstFloor(item) && M_IsAgainstCeiling(item)) {
        return NO_HEIGHT;
    }

    if (y <= item->pos.y && y > item->pos.y - WALL_L && !item->gravity) {
        if (M_IsAgainstCeiling(item)) {
            // If clamped betwee floor and ceiling return same sentinel value as
            // M_GetFloorHeight.
            return M_IsAgainstFloor(item) ? item->pos.y - WALL_L : item->pos.y;
        }
        return height;
    }

    // If above the top of the block.
    if (y <= item->pos.y - WALL_L) {
        return height;
    }

    // If the the bottom of the block is above the ceiling height.
    if (item->pos.y <= height) {
        return height;
    }

    return item->pos.y;
}

static void M_AddWalkable(const int16_t item_num)
{
    const ITEM *const item = Item_Get(item_num);
    Walkable_Add(item_num, item->pos);
}

static void M_GetStack(
    VECTOR *const stack, const XYZ_32 stack_pos, int32_t stack_height,
    const int32_t step_y, const int16_t room_num)
{
    int16_t sector_room_num = room_num;
    SECTOR *sector = Room_GetSector(stack_pos, &sector_room_num);
    sector = Room_GetPitSector(sector, stack_pos.x, stack_pos.z);

    for (WALKABLE *w = sector->walkable; w != nullptr; w = w->next) {
        const ITEM *item = Item_Get(w->item_num);
        if (!Object_IsType(item->object_id, g_MovableBlockObjects)) {
            continue;
        }
        if (w->pos.x == stack_pos.x && w->pos.y == stack_height
            && w->pos.z == stack_pos.z) {
            Vector_Add(stack, (void *)&w->item_num);
            stack_height += step_y;
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->bounds_func = M_Bounds;
    obj->draw_func = M_Draw;
    obj->initialise_func = M_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->collision_func = M_Collision;
    obj->control_func = M_Control;
    obj->floor_height_func = M_GetFloorHeight;
    obj->ceiling_height_func = M_GetCeilingHeight;
    obj->add_walkable_func = M_AddWalkable;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->base_rot.y = true;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->save_position = true;
}

void MovableBlock_UpdateBox(const ITEM *const item, const bool blocked)
{
    if (blocked
        && (item->status == IS_ACTIVE || item->status == IS_INVISIBLE
            || (item->flags & IF_KILLED) != 0)) {
        return;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    if (sector->floor.height == item->pos.y && sector->box != NO_BOX) {
        BOX_INFO *const box = Box_GetBox(sector->box);
        if (box != nullptr && (box->overlap_index & BOX_BLOCKABLE) != 0) {
            TOGGLE_BIT(box->overlap_index, BOX_BLOCKED, blocked);
        }
    }
}

void MovableBlock_DropStack(const XYZ_32 drop_pos, int16_t room_num)
{
    VECTOR *stack = Vector_Create(sizeof(int16_t));
    M_GetStack(stack, drop_pos, drop_pos.y, -WALL_L, room_num);

    for (int16_t i = stack->count - 1; i >= 0; i--) {
        const int16_t item_num = *(const int16_t *)Vector_Get(stack, i);
        ITEM *const item = Item_Get(item_num);
        M_SetGravityFrames(item, i);
        item->status = IS_ACTIVE;
        Item_AddActive(item_num);
        Item_Animate(item);
    }

    Vector_Free(stack);
}

void MovableBlock_ShiftStackY(
    int32_t stack_height, const XYZ_32 old_pos, const int32_t new_y,
    const int16_t room_num, const bool reposition)
{
    VECTOR *stack = Vector_Create(sizeof(int16_t));
    M_GetStack(stack, old_pos, stack_height, -WALL_L, room_num);

    for (int16_t i = 0; i < stack->count; i++) {
        const int16_t item_num = *(const int16_t *)Vector_Get(stack, i);
        ITEM *const item = Item_Get(item_num);
        item->status = IS_ACTIVE;
        M_SetForcedMoving(item, true);
        item->pos.y = new_y;
        int16_t sector_room_num = room_num;
        SECTOR *sector = Room_GetSector(
            (XYZ_32) { item->pos.x, item->pos.y - STEP_L, item->pos.z },
            &sector_room_num);
        Item_UpdateRoom(item_num, sector_room_num);
        if (reposition) {
            const GAME_VECTOR target = {
                .pos = item->pos,
                .room_num = item->room_num,
            };
            Walkable_Reposition(item_num, M_GetLinked(item), target);
            M_SetLinked(item);
            item->status = IS_INACTIVE;
            M_SetForcedMoving(item, false);
        }
    }

    Vector_Free(stack);
}

void MovableBlock_SlideStack(
    int32_t stack_height, const XYZ_32 old_pos, const ITEM *const dest_item,
    const bool reposition)
{
    VECTOR *stack = Vector_Create(sizeof(int16_t));
    M_GetStack(stack, old_pos, stack_height, -WALL_L, dest_item->room_num);

    for (int16_t i = 0; i < stack->count; i++) {
        const int16_t item_num = *(const int16_t *)Vector_Get(stack, i);
        ITEM *const item = Item_Get(item_num);
        item->status = IS_ACTIVE;
        M_SetForcedMoving(item, true);
        item->pos.x = dest_item->pos.x;
        item->pos.z = dest_item->pos.z;
        int16_t sector_room_num = dest_item->room_num;
        Room_GetSector(
            (XYZ_32) { item->pos.x, item->pos.y - STEP_L, item->pos.z },
            &sector_room_num);
        Item_UpdateRoom(item_num, sector_room_num);
        if (reposition) {
            const GAME_VECTOR target = {
                .pos = item->pos,
                .room_num = item->room_num,
            };
            Walkable_Reposition(item_num, M_GetLinked(item), target);
            M_SetLinked(item);
            item->status = IS_INACTIVE;
            M_SetForcedMoving(item, false);
        }
    }

    Vector_Free(stack);
}

REGISTER_OBJECT(O_MOVABLE_BLOCK_1, M_Setup)
REGISTER_OBJECT(O_MOVABLE_BLOCK_2, M_Setup)
REGISTER_OBJECT(O_MOVABLE_BLOCK_3, M_Setup)
REGISTER_OBJECT(O_MOVABLE_BLOCK_4, M_Setup)

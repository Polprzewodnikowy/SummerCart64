#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/const.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/items.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/names.h>
#include <trx/game/objects/vars.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>

static GAME_VECTOR M_GetTargetPos(
    const ITEM *const lara_item, const int16_t angle_add)
{
    const int16_t angle = lara_item->rot.y + angle_add;
    const int32_t s = Math_Sin(angle);
    const int32_t c = Math_Cos(angle);
    const int32_t dist = WALL_L;
    return (GAME_VECTOR) {
        .x = lara_item->pos.x + ((s * dist) >> W2V_SHIFT),
        .y = lara_item->pos.y,
        .z = lara_item->pos.z + ((c * dist) >> W2V_SHIFT),
        .room_num = lara_item->room_num,
    };
}

static bool M_FindValidTargetPos(
    const ITEM *const lara_item, GAME_VECTOR *const out_pos)
{
    for (int32_t angle = -DEG_45; angle <= DEG_45; angle += DEG_45) {
        GAME_VECTOR pos = M_GetTargetPos(lara_item, angle);
        if (Room_FindValidPos(&pos.pos, &pos.room_num)) {
            *out_pos = pos;
            return true;
        }
    }
    return false;
}

static bool M_CanSpawnObject(const OBJECT_ID object_id)
{
    return !Object_IsType(object_id, g_NullObjects)
        && !Object_IsType(object_id, g_AnimObjects)
        && !Object_IsType(object_id, g_InvObjects)
        && Object_Get(object_id)->loaded;
}

static bool M_ShouldEnableBaddieAI(const OBJECT_ID object_id)
{
    if (Object_IsType(object_id, g_CreatureObjects)) {
        return true;
    }

    if (Object_IsType(object_id, g_LoyalObjects)) {
        return true;
    }

    return Object_Get(object_id)->intelligent;
}

static bool M_SpawnItem(
    const OBJECT_ID object_id, const GAME_VECTOR target_pos,
    const XYZ_16 target_rot)
{
    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return false;
    }

    ITEM *const new_item = Item_Get(item_num);
    new_item->object_id = object_id;
    new_item->room_num = target_pos.room_num;
    new_item->pos = target_pos.pos;
    new_item->rot = target_rot;
    new_item->shade.value_1 = -1;
    Item_Initialise(item_num);
    Item_AddActive(item_num);

    if (M_ShouldEnableBaddieAI(object_id)) {
        new_item->status = IS_ACTIVE;
        LOT_EnableBaddieAI(item_num, true);
    }

    return true;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item->hit_points <= 0) {
        return CR_UNAVAILABLE;
    }

    GAME_VECTOR target_pos;
    if (!M_FindValidTargetPos(lara_item, &target_pos)) {
        Console_LogError(GS("console/cmd/spawn/fail"));
        return CR_FAILURE;
    }

    const char *const args = ctx->args;
    int32_t match_count = 0;
    OBJECT_NAME_MATCH *matches = nullptr;
    if (String_IsEmpty(args)) {
        return CR_BAD_INVOCATION;
    } else {
        matches = Object_IdsFromName(args, &match_count, M_CanSpawnObject);
    }

    if (match_count <= 0) {
        Console_LogError(GS("general/osd/invalid_item"), args);
        Memory_FreePointer(&matches);
        return CR_FAILURE;
    }

    const int32_t dx = lara_item->pos.x - target_pos.x;
    const int32_t dz = lara_item->pos.z - target_pos.z;
    const XYZ_16 target_rot = { .y = Math_Atan(dz, dx) };

    bool spawned = false;
    for (int32_t i = 0; i < match_count; i++) {
        if (M_SpawnItem(matches[i].object_id, target_pos, target_rot)) {
            spawned = true;
            break;
        }
    }
    Memory_FreePointer(&matches);

    if (!spawned) {
        return CR_FAILURE;
    }

    Console_Log(GS("console/cmd/spawn/success"));
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("spawn", M_Entrypoint, nullptr)

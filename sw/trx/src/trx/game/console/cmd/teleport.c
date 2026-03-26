#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/const.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/items.h>
#include <trx/game/lara/cheat.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/names.h>
#include <trx/game/objects/vars.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static int16_t m_LastTeleportedItemNum = NO_ITEM;

static bool M_ObjectCanBePickedUp(const OBJECT_ID obj_id)
{
    if (!Object_IsType(obj_id, g_PickupObjects)) {
        return true;
    }
    for (int32_t item_num = 0; item_num < Item_GetTotalCount(); item_num++) {
        const ITEM *const item = Item_Get(item_num);
        if (item->object_id == obj_id && item->status != IS_INVISIBLE) {
            return true;
        }
    }
    return false;
}

static bool M_CanTargetObject(const OBJECT_ID obj_id)
{
    return !Object_IsType(obj_id, g_NullObjects)
        && !Object_IsType(obj_id, g_AnimObjects)
        && !Object_IsType(obj_id, g_InvObjects) && Object_Get(obj_id)->loaded
        && M_ObjectCanBePickedUp(obj_id);
}

static bool M_CanTargetItem(
    const ITEM *const item, const OBJECT_NAME_MATCH *const matches,
    const int32_t match_count)
{
    // Collected pickups
    if (Object_IsType(item->object_id, g_PickupObjects)
        && (item->status == IS_INVISIBLE || item->status == IS_DEACTIVATED
            || item->room_num == NO_ROOM)) {
        return false;
    }

    // Killed enemies and removed items
    if (item->flags & IF_KILLED) {
        return false;
    }

    // Out of bounds items
    int16_t room_num = item->room_num;
    if (room_num != NO_ROOM) {
        const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
        if (Room_GetHeight(sector, item->pos) == NO_HEIGHT) {
            return false;
        }
    }

    // Non-matches to user input
    bool is_matched = false;
    for (int32_t j = 0; j < match_count; j++) {
        if (matches[j].object_id == item->object_id) {
            is_matched = true;
            break;
        }
    }
    if (!is_matched) {
        return false;
    }

    return true;
}

static bool M_CanTargetEnemyItem(const ITEM *const item)
{
    if (!Creature_IsHostile(item) || item->room_num == NO_ROOM
        || item->hit_points <= 0 || (item->flags & IF_KILLED) != 0) {
        return false;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    return Room_GetHeight(sector, item->pos) != NO_HEIGHT;
}

static const ITEM *M_GetItemToTeleporTo(const char *const user_input)
{
    int32_t match_count = 0;
    OBJECT_NAME_MATCH *matches =
        Object_IdsFromName(user_input, &match_count, M_CanTargetObject);

    const ITEM *const lara_item = Lara_GetItem();

    int16_t best_item_num = NO_ITEM;

    // Choose the matching item closest to Lara.
    const int32_t near_distance = WALL_L;
    int16_t closest_item_num = NO_ITEM;
    int32_t closest_distance = INT32_MAX;
    for (int32_t item_num = 0; item_num < Item_GetTotalCount(); item_num++) {
        const ITEM *const item = Item_Get(item_num);
        if (!M_CanTargetItem(item, matches, match_count)) {
            continue;
        }

        const int32_t distance = Item_GetDistance(item, lara_item->pos);
        if (distance < closest_distance) {
            closest_distance = distance;
            closest_item_num = item_num;
        }
    }

    if (closest_distance > near_distance) {
        best_item_num = closest_item_num;
    } else {
        // If Lara's already very close to a matching item, choose the next
        // matching item in a round-robin fashion.
        const int16_t start_idx = (closest_item_num + 1) % Item_GetTotalCount();
        for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
            int16_t item_num = (start_idx + i) % Item_GetTotalCount();
            if (item_num == closest_item_num) {
                continue;
            }

            const ITEM *const item = Item_Get(item_num);
            if (!M_CanTargetItem(item, matches, match_count)) {
                continue;
            }

            const int32_t distance = Item_GetDistance(item, lara_item->pos);
            if (distance < near_distance) {
                continue;
            }

            best_item_num = item_num;
            break;
        }
    }

    Memory_FreePointer(&matches);
    return Item_Get(best_item_num);
}

static const ITEM *M_GetHostileEnemyToTeleportTo(void)
{
    const int32_t item_count = Item_GetTotalCount();
    if (item_count <= 0) {
        return nullptr;
    }

    int16_t start_idx = 0;
    if (m_LastTeleportedItemNum >= 0 && m_LastTeleportedItemNum < item_count) {
        start_idx = (m_LastTeleportedItemNum + 1) % item_count;
    }

    for (int32_t i = 0; i < item_count; i++) {
        const int16_t item_num = (start_idx + i) % item_count;
        const ITEM *const item = Item_Get(item_num);
        if (!M_CanTargetEnemyItem(item)) {
            continue;
        }
        m_LastTeleportedItemNum = item_num;
        return item;
    }

    return nullptr;
}

static inline bool M_IsFloatRound(const float num)
{
    return fabsf(num - roundf(num)) < 0.0001f;
}

static void M_AlignLaraToItem(const ITEM *const item)
{
    typedef enum {
        L_DIR_NONE = -1,
        L_DIR_SAME = 0,
        L_DIR_OPPOSITE = 1,
    } L_DIR;

    L_DIR dir = L_DIR_NONE;
    if (Object_IsType(item->object_id, g_PickupObjects)) {
        dir = L_DIR_OPPOSITE;
    } else if (Object_IsType(item->object_id, g_SwitchObjects)) {
        dir = L_DIR_SAME;
    } else if (Object_IsType(item->object_id, g_ReceptacleObjects)) {
        dir = L_DIR_SAME;
    } else if (Object_IsType(item->object_id, g_DoorObjects)) {
        dir = L_DIR_OPPOSITE;
    } else if (item->object_id == O_ZIPLINE_HANDLE) {
        dir = L_DIR_SAME;
    }
    if (dir != L_DIR_NONE) {
        ITEM *const lara_item = Lara_GetItem();
        lara_item->rot.x = 0;
        lara_item->rot.y = item->rot.y;
        lara_item->rot.z = 0;
        if (dir == L_DIR_OPPOSITE) {
            lara_item->rot.y += DEG_180;
        }
    }
}

static COMMAND_RESULT M_TeleportToXYZ(
    float x, const float y, float z, const bool precise_coords)
{
    if (!precise_coords) {
        if (M_IsFloatRound(x)) {
            x += 0.5f;
        }
        if (M_IsFloatRound(z)) {
            z += 0.5f;
        }
    }

    const XYZ_32 pos = {
        .x = precise_coords ? x : x * WALL_L,
        .y = precise_coords ? y : y * WALL_L,
        .z = precise_coords ? z : z * WALL_L,
    };
    if (!Lara_Cheat_Teleport(pos, NO_ROOM)) {
        Console_LogError(GS("console/cmd/teleport/pos_fail"), x, y, z);
        return CR_FAILURE;
    }

    Console_Log(GS("console/cmd/teleport/pos"), x, y, z);
    return CR_SUCCESS;
}

static COMMAND_RESULT M_TeleportToItemNum(const int16_t item_num)
{
    if (item_num < 0 || item_num >= Item_GetTotalCount()) {
        Console_LogError(GS("console/cmd/teleport/item_fail"), item_num);
        return CR_FAILURE;
    }

    const ITEM *const item = Item_Get(item_num);
    if (item == nullptr || item->room_num == NO_ROOM) {
        Console_LogError(GS("console/cmd/teleport/item_fail"), item_num);
        return CR_FAILURE;
    }

    if ((item->flags & IF_KILLED) != 0) {
        Console_LogError(GS("console/cmd/teleport/item_fail"), item_num);
        return CR_FAILURE;
    }

    const XYZ_32 pos = {
        .x = item->pos.x,
        .y = item->pos.y - STEP_L / 4,
        .z = item->pos.z,
    };
    if (Lara_Cheat_Teleport(pos, item->room_num)) {
        M_AlignLaraToItem(item);
        Console_Log(GS("console/cmd/teleport/item"), item_num);
        return CR_SUCCESS;
    }

    Console_LogError(GS("console/cmd/teleport/item_fail"), item_num);
    return CR_FAILURE;
}

static COMMAND_RESULT M_TeleportToRoom(const int16_t room_num)
{
    if (room_num < 0 || room_num >= Room_GetCount()) {
        Console_LogWarning(
            GS("general/osd/invalid_room"), room_num, Room_GetCount() - 1);
        return CR_FAILURE;
    }

    const ROOM *const room = Room_Get(room_num);
    const int32_t x1 = room->pos.x + WALL_L;
    const int32_t x2 = room->pos.x + (room->size.x << WALL_SHIFT) - WALL_L;
    const int32_t y1 = room->min_floor;
    const int32_t y2 = room->max_ceiling;
    const int32_t z1 = room->pos.z + WALL_L;
    const int32_t z2 = room->pos.z + (room->size.z << WALL_SHIFT) - WALL_L;

    bool success = false;
    for (int32_t i = 0; i < 100; i++) {
        const XYZ_32 pos = {
            .x = x1 + Random_GetControl() * (x2 - x1) / 0x7FFF,
            .y = y1,
            .z = z1 + Random_GetControl() * (z2 - z1) / 0x7FFF,
        };
        if (Lara_Cheat_Teleport(pos, room_num)) {
            success = true;
            break;
        }
    }

    if (!success) {
        Console_LogError(GS("console/cmd/teleport/room_fail"), room_num);
        return CR_FAILURE;
    }

    Console_Log(GS("console/cmd/teleport/room"), room_num);
    return CR_SUCCESS;
}

static COMMAND_RESULT M_TeleportToObject(const char *const user_input)
{
    // Nearest item of this name
    if (String_Equivalent(user_input, "")) {
        return CR_BAD_INVOCATION;
    }

    const ITEM *const best_item = M_GetItemToTeleporTo(user_input);
    if (best_item == nullptr) {
        Console_LogError(GS("console/cmd/teleport/object_fail"), user_input);
        return CR_FAILURE;
    }

    // Determine which alias matched via Object_IdsFromName
    int32_t match_count = 0;
    OBJECT_NAME_MATCH *matches =
        Object_IdsFromName(user_input, &match_count, M_CanTargetObject);
    const char *reported_name = user_input;
    for (int32_t i = 0; i < match_count; i++) {
        if (matches[i].object_id == best_item->object_id) {
            if (matches[i].matched_name != nullptr) {
                reported_name = matches[i].matched_name;
            }
            break;
        }
    }
    Memory_FreePointer(&matches);

    const XYZ_32 pos = {
        .x = best_item->pos.x,
        .y = best_item->pos.y - STEP_L / 4,
        .z = best_item->pos.z,
    };
    if (Lara_Cheat_Teleport(pos, best_item->room_num)) {
        M_AlignLaraToItem(best_item);
        Console_Log(GS("console/cmd/teleport/object"), reported_name);
    } else {
        Console_LogError(GS("console/cmd/teleport/object_fail"), reported_name);
    }
    return CR_SUCCESS;
}

static COMMAND_RESULT M_TeleportToEnemy(void)
{
    ITEM *const enemy_item = (ITEM *)M_GetHostileEnemyToTeleportTo();
    if (enemy_item == nullptr) {
        Console_LogError(GS("console/cmd/teleport/object_fail"), "enemy");
        return CR_FAILURE;
    }

    const XYZ_32 pos = {
        .x = enemy_item->pos.x,
        .y = enemy_item->pos.y - STEP_L / 4,
        .z = enemy_item->pos.z,
    };
    if (!Lara_Cheat_Teleport(pos, enemy_item->room_num)) {
        Console_LogError(GS("console/cmd/teleport/object_fail"), "enemy");
        return CR_FAILURE;
    }

    M_AlignLaraToItem(enemy_item);
    Console_Log(GS("console/cmd/teleport/object"), "enemy");
    return CR_SUCCESS;
}

static bool M_TryParseTagNumber(
    const char *const args, const char tag, int16_t *const out_num)
{
    if (args == nullptr) {
        return false;
    }
    if (args[0] != tag) {
        return false;
    }

    int32_t num32 = 0;
    if (!String_ParseInteger(args + 1, &num32)) {
        return false;
    }
    if (num32 < INT16_MIN || num32 > INT16_MAX) {
        return false;
    }
    *out_num = (int16_t)num32;
    return true;
}

static bool M_TryParseKeywordNumber(
    const char *const args, const char *const keyword, int16_t *const out_num)
{
    if (args == nullptr || keyword == nullptr) {
        return false;
    }

    const size_t keyword_len = strlen(keyword);
    if (strncmp(args, keyword, keyword_len) != 0) {
        return false;
    }

    int32_t num32 = 0;
    if (!String_ParseInteger(args + keyword_len, &num32)) {
        return false;
    }
    if (num32 < INT16_MIN || num32 > INT16_MAX) {
        return false;
    }
    *out_num = (int16_t)num32;
    return true;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    const ITEM *const lara_item = Lara_GetItem();
    if (!lara_item->hit_points) {
        return CR_UNAVAILABLE;
    }

    float x, y, z;
    if (sscanf(ctx->args, "precise %f %f %f", &x, &y, &z) == 3) {
        return M_TeleportToXYZ(x, y, z, true);
    }

    if (sscanf(ctx->args, "%f %f %f", &x, &y, &z) == 3) {
        return M_TeleportToXYZ(x, y, z, false);
    }

    int16_t num = 0;
    if (M_TryParseKeywordNumber(ctx->args, "item ", &num)) {
        return M_TeleportToItemNum(num);
    }
    if (M_TryParseTagNumber(ctx->args, 'i', &num)) {
        return M_TeleportToItemNum(num);
    }
    if (M_TryParseKeywordNumber(ctx->args, "room ", &num)) {
        return M_TeleportToRoom(num);
    }
    if (M_TryParseTagNumber(ctx->args, 'r', &num)) {
        return M_TeleportToRoom(num);
    }

    int16_t room_num = -1;
    if (sscanf(ctx->args, "%hd", &room_num) == 1) { // legacy
        return M_TeleportToRoom(room_num);
    }

    if (String_Equivalent(ctx->args, "enemy")) {
        return M_TeleportToEnemy();
    }

    return M_TeleportToObject(ctx->args);
}

REGISTER_CONSOLE_COMMAND("tp", M_Entrypoint, GS_ID("console/cmd/tp/help"))

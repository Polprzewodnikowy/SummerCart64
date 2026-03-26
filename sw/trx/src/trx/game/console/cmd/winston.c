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
#include <trx/game/music.h>
#include <trx/game/objects/vars.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>

static bool M_TrySummon(const GAME_VECTOR target_pos, const OBJECT_ID object_id)
{
    const ITEM *const lara_item = Lara_GetItem();
    for (int16_t item_num = 0; item_num < Item_GetTotalCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (item->object_id == object_id) {
            if (item->status == IS_INVISIBLE || item->status == IS_INACTIVE) {
                item->status = IS_ACTIVE;
                Item_AddActive(item_num);
                LOT_EnableBaddieAI(item_num, true);
            } else if ((item->flags & IF_KILLED) != 0) {
                Music_Stop();
                Console_Log(GS("console/cmd/winston/dead"));
                return true;
            }
            item->pos.x = target_pos.x;
            item->pos.y = target_pos.y;
            item->pos.z = target_pos.z;
            item->rot.y = lara_item->rot.y;
            Item_UpdateRoom(item_num, target_pos.room_num);
            return true;
        }
    }
    return false;
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

    const OBJECT *const obj = Object_Get(O_WINSTON);
    if (!obj->loaded) {
        return CR_UNAVAILABLE;
    }

    GAME_VECTOR target_pos = {
        .x = lara_item->pos.x + STEP_L,
        .y = lara_item->pos.y - WALL_L,
        .z = lara_item->pos.z + STEP_L,
        .room_num = lara_item->room_num,
    };
    if (!Room_FindValidPos(&target_pos.pos, &target_pos.room_num)) {
        Console_LogError(GS("console/cmd/winston/spawn_failed"));
        return CR_FAILURE;
    }

    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->killed_loyal_item) {
        Music_Stop();
        Console_Log(GS("console/cmd/winston/dead"));
        return CR_FAILURE;
    }

    if (M_TrySummon(target_pos, O_WINSTON_ARMY)
        || M_TrySummon(target_pos, O_WINSTON)) {
        Console_Log(GS("console/cmd/winston/teleported"));
        return CR_SUCCESS;
    }

    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return CR_FAILURE;
    }
    ITEM *const new_item = Item_Get(item_num);
    new_item->object_id = O_WINSTON;
    new_item->room_num = target_pos.room_num;
    new_item->pos.x = target_pos.x;
    new_item->pos.y = target_pos.y;
    new_item->pos.z = target_pos.z;
    new_item->rot.y = lara_item->rot.y;
    new_item->shade.value_1 = -1;
    Item_Initialise(item_num);
    Item_AddActive(item_num);
    new_item->status = IS_ACTIVE;
    LOT_EnableBaddieAI(item_num, true);

    Console_Log(GS("console/cmd/winston/spawned"));
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("teatime", M_Entrypoint, nullptr)

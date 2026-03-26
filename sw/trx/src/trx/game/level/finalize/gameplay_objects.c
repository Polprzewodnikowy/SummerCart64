#include <trx/game/game_flow/util.h>
#include <trx/game/items.h>
#include <trx/game/items/walkable.h>
#include <trx/game/lara.h>
#include <trx/game/level/finalize.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

static uint8_t M_GetAIBit(const OBJECT_ID object_id)
{
    switch (object_id) {
        // clang-format off
    case O_AI_GUARD:    return AI_GUARD;
    case O_AI_AMBUSH:   return AI_AMBUSH;
    case O_AI_PATROL_1: return AI_PATROL_1;
    case O_AI_MODIFY:   return AI_MODIFY;
    case O_AI_FOLLOW:   return AI_FOLLOW;
    // clang-format on
    default:
        return 0;
    }
}

static void M_AssignAIBits(void)
{
    const int32_t item_count = Item_GetLevelCount();
    for (int32_t i = 0; i < item_count; i++) {
        ITEM *const item = Item_Get(i);
        const OBJECT *const obj = Object_Get(item->object_id);
        if (!obj->intelligent || item->room_num == NO_ROOM) {
            continue;
        }

        ROOM *const room = Room_Get(item->room_num);
        int16_t ai_item_num = room->item_num;
        while (ai_item_num != NO_ITEM) {
            ITEM *const ai_item = Item_Get(ai_item_num);
            const int16_t next_num = ai_item->next_item;
            const uint8_t ai_bit = M_GetAIBit(ai_item->object_id);
            if (ai_bit != 0 && ai_item->pos.x == item->pos.x
                && ai_item->pos.z == item->pos.z) {
                item->ai_bits |= ai_bit;
                item->ai_tag = ai_item->rot.y;
                if (!(ai_item->object_id == O_AI_PATROL_1
                      && (GF_BadGetLevelNum() == 15
                          || GF_BadGetLevelNum() == 14))) {
                    Item_Kill(ai_item_num);
                    ai_item->room_num = NO_ROOM;
                }
            }
            ai_item_num = next_num;
        }
    }
}

void Level_Finalize_LoadObjectsAndItems(LEVEL_CONTEXT *const ctx)
{
    // Object and item setup/initialisation must take place after injections
    // have been processed. A cached item count must be used as individual
    // initialisations may increment the total item count.
    Object_SetupAllObjects();
    Walkable_ResetLevel();

    const int32_t item_count = Item_GetLevelCount();
    for (int32_t i = 0; i < item_count; i++) {
        Item_Initialise(i);
    }

    // Must take place after item initialization.
    Level_Finalize_LoadWalkables(ctx);

    M_AssignAIBits();
    Lara_State_Initialise();
}

void Level_Finalize_LoadWalkables(LEVEL_CONTEXT *const ctx)
{
    for (int32_t item_num = 0; item_num < Item_GetLevelCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        const OBJECT *const obj = Object_Get(item->object_id);
        if (obj->add_walkable_func != nullptr) {
            obj->add_walkable_func(item_num);
        }
    }
}

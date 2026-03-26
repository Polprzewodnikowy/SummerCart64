#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/items.h>
#include <trx/game/objects.h>
#include <trx/game/objects/names.h>
#include <trx/game/pathing.h>

typedef enum {
    M_TARGET_OK = 0,
    M_TARGET_INVALID_ITEM,
    M_TARGET_NO_MATCH,
    M_TARGET_NOT_FOUND,
} M_TARGET_RESULT;

static bool M_CanTargetObject(const OBJECT_ID object_id)
{
    return Object_Get(object_id)->loaded;
}

static void M_SortItemNums(int16_t *const item_nums, const int32_t count)
{
    for (int32_t i = 0; i < count - 1; i++) {
        for (int32_t j = i + 1; j < count; j++) {
            if (item_nums[i] > item_nums[j]) {
                SWAP(item_nums[i], item_nums[j]);
            }
        }
    }
}

static char *M_BuildCollapsedItemNums(
    const int16_t *const item_nums, const int32_t count)
{
    if (count <= 0) {
        return Memory_DupStr("");
    }

    char *result = nullptr;
    for (int32_t i = 0; i < count; i++) {
        const int16_t start = item_nums[i];
        int16_t end = start;
        while (i + 1 < count && item_nums[i + 1] == end + 1) {
            i++;
            end = item_nums[i];
        }

        char *segment = nullptr;
        if (end == start) {
            segment = String_Format("%d", start);
        } else {
            segment = String_Format("%d-%d", start, end);
        }

        if (result == nullptr) {
            result = segment;
        } else {
            char *const joined = String_Format("%s, %s", result, segment);
            Memory_FreePointer(&result);
            Memory_FreePointer(&segment);
            result = joined;
        }
    }

    if (result == nullptr) {
        return Memory_DupStr("");
    }
    return result;
}

static void M_ApplyTrigger(const int16_t item_num, const bool enable)
{
    ITEM *const item = Item_Get(item_num);
    if (item == nullptr) {
        return;
    }
    const OBJECT *const obj = Object_Get(item->object_id);

    if (obj->trigger_func != nullptr) {
        const bool use_default_handling = obj->trigger_func(item, nullptr);
        if (!use_default_handling) {
            return;
        }
    }

    if (enable) {
        item->flags |= IF_CODE_BITS;
        item->timer = 0;
        Item_AddActive(item_num);
        if (obj->intelligent) {
            item->status = IS_ACTIVE;
            LOT_EnableBaddieAI(item_num, true);
        }
    } else {
        item->flags &= ~IF_CODE_BITS;
        Item_RemoveActive(item_num);
        if (obj->intelligent) {
            item->status = IS_ACTIVE;
            LOT_DisableBaddieAI(item_num);
        }
    }
}

static M_TARGET_RESULT M_TargetItemsFromItemNum(
    VECTOR *const target_item_nums, const int32_t item_num_arg)
{
    if (item_num_arg < 0 || item_num_arg >= Item_GetTotalCount()) {
        return M_TARGET_INVALID_ITEM;
    }

    const int16_t item_num = (int16_t)item_num_arg;
    const ITEM *const item = Item_Get(item_num);
    if (item == nullptr || item->object_id == O_LARA) {
        return M_TARGET_INVALID_ITEM;
    }

    Vector_Add(target_item_nums, &item_num);
    return M_TARGET_OK;
}

static M_TARGET_RESULT M_TargetItemsFromItemName(
    VECTOR *const target_item_nums, const char *const item_name)
{
    const ITEM *const item = Item_GetByName(item_name);
    if (item == nullptr) {
        return M_TARGET_NO_MATCH;
    }

    const int16_t item_num = Item_GetIndex(item);
    Vector_Add(target_item_nums, &item_num);
    return M_TARGET_OK;
}

static M_TARGET_RESULT M_TargetItemsFromObjectName(
    VECTOR *const target_item_nums, const char *const object_name)
{
    int32_t match_count = 0;
    OBJECT_NAME_MATCH *matches =
        Object_IdsFromName(object_name, &match_count, M_CanTargetObject);
    if (match_count <= 0) {
        Memory_FreePointer(&matches);
        return M_TARGET_NO_MATCH;
    }

    const int32_t total_count = Item_GetTotalCount();
    for (int16_t item_num = 0; item_num < total_count; item_num++) {
        const ITEM *const item = Item_Get(item_num);
        if (item == nullptr || item->object_id == O_LARA) {
            continue;
        }
        if ((item->flags & IF_KILLED) != 0) {
            continue;
        }

        bool is_matched = false;
        for (int32_t i = 0; i < match_count; i++) {
            if (matches[i].object_id == item->object_id) {
                is_matched = true;
                break;
            }
        }
        if (!is_matched) {
            continue;
        }

        Vector_Add(target_item_nums, &item_num);
    }

    Memory_FreePointer(&matches);
    if (target_item_nums->count <= 0) {
        return M_TARGET_NOT_FOUND;
    }
    return M_TARGET_OK;
}

static M_TARGET_RESULT M_GetTargetItems(
    VECTOR *const target_item_nums, const char *const user_input)
{
    int32_t item_num_arg = 0;
    if (String_ParseInteger(user_input, &item_num_arg)) {
        return M_TargetItemsFromItemNum(target_item_nums, item_num_arg);
    }

    const M_TARGET_RESULT named_result =
        M_TargetItemsFromItemName(target_item_nums, user_input);
    if (named_result == M_TARGET_OK) {
        return M_TARGET_OK;
    }

    return M_TargetItemsFromObjectName(target_item_nums, user_input);
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsLoaded()) {
        return CR_UNAVAILABLE;
    }

    const bool is_trigger = String_Equivalent(ctx->prefix, "trigger");
    const bool is_untrigger = String_Equivalent(ctx->prefix, "untrigger");
    if (!is_trigger && !is_untrigger) {
        return CR_BAD_INVOCATION;
    }
    const bool enable = is_trigger;

    if (String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    VECTOR *const target_item_nums = Vector_Create(sizeof(int16_t));
    const M_TARGET_RESULT target_result =
        M_GetTargetItems(target_item_nums, ctx->args);
    if (target_result != M_TARGET_OK) {
        switch (target_result) {
        case M_TARGET_INVALID_ITEM:
            Console_LogError(GS("console/cmd/trigger/invalid_item"), ctx->args);
            break;
        case M_TARGET_NO_MATCH:
            Console_LogError(GS("console/cmd/trigger/no_match"), ctx->args);
            break;
        case M_TARGET_NOT_FOUND:
            Console_LogError(GS("console/cmd/trigger/not_found"), ctx->args);
            break;
        case M_TARGET_OK:
            break;
        }
        Vector_Free(target_item_nums);
        return CR_FAILURE;
    }

    int16_t *const item_nums = Vector_GetData(target_item_nums);
    M_SortItemNums(item_nums, target_item_nums->count);
    for (int32_t i = 0; i < target_item_nums->count; i++) {
        M_ApplyTrigger(item_nums[i], enable);
    }

    char *collapsed =
        M_BuildCollapsedItemNums(item_nums, target_item_nums->count);
    Console_Log(
        enable ? GS("console/cmd/trigger/triggered")
               : GS("console/cmd/trigger/untriggered"),
        collapsed);
    Memory_FreePointer(&collapsed);
    Vector_Free(target_item_nums);

    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "trigger", M_Entrypoint, GS_ID("console/cmd/trigger/help"))
REGISTER_CONSOLE_COMMAND(
    "untrigger", M_Entrypoint, GS_ID("console/cmd/trigger/help"))

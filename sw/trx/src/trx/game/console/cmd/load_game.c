#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/savegame.h>

#include <stdio.h>

typedef struct {
    SAVEGAME_SLOT_POOL pool;
    int32_t slot_num;
    bool has_slot_num;
} M_SLOT_REQUEST;

static bool M_IsBlank(const char *const str)
{
    if (str == nullptr) {
        return true;
    }
    for (const char *c = str; *c != '\0'; c++) {
        if (*c != ' ' && *c != '\t') {
            return false;
        }
    }
    return true;
}

static bool M_TryParseQuickArg(
    const char *const args, int32_t *const slot_num, bool *const has_slot_num)
{
    *has_slot_num = false;
    if (M_IsBlank(args)) {
        return true;
    }

    char tail = '\0';
    if (String_ParseInteger(args, slot_num)
        || sscanf(args, " quick %d %c", slot_num, &tail) == 1
        || sscanf(args, " q%d %c", slot_num, &tail) == 1) {
        *has_slot_num = true;
        return true;
    }

    return false;
}

static bool M_TryParseLoadArg(
    const char *const args, M_SLOT_REQUEST *const request)
{
    int32_t slot_num = -1;
    if (String_ParseInteger(args, &slot_num)) {
        request->pool = SAVEGAME_SLOT_POOL_NORMAL;
        request->slot_num = slot_num;
        request->has_slot_num = true;
        return true;
    }

    char tail = '\0';
    if (sscanf(args, " quick %d %c", &slot_num, &tail) == 1
        || sscanf(args, " q%d %c", &slot_num, &tail) == 1) {
        request->pool = SAVEGAME_SLOT_POOL_QUICK;
        request->slot_num = slot_num;
        request->has_slot_num = true;
        return true;
    }

    if (sscanf(args, " quick %c", &tail) == 0
        || sscanf(args, " q %c", &tail) == 0
        || sscanf(args, " q%c", &tail) == 0) {
        request->pool = SAVEGAME_SLOT_POOL_QUICK;
        request->slot_num = -1;
        request->has_slot_num = false;
        return true;
    }

    return false;
}

static COMMAND_RESULT M_ExecuteLoad(const M_SLOT_REQUEST request)
{
    SAVEGAME_SLOT_REF slot = Savegame_InvalidSlot();
    int32_t shown_slot_num = request.slot_num;
    if (request.pool == SAVEGAME_SLOT_POOL_QUICK) {
        const int32_t visual_count = Savegame_GetQuickVisualCount();
        const int32_t visual_slot = request.has_slot_num ? request.slot_num : 1;
        if (visual_slot < 1 || visual_slot > visual_count) {
            Console_LogError(
                GS("general/osd/load_game_fail_invalid_slot"), visual_slot);
            return CR_FAILURE;
        }
        slot = Savegame_QuickFromVisualIndex(visual_slot - 1);
        shown_slot_num = visual_slot;
    } else {
        const int32_t slot_idx =
            request.slot_num - 1; // convert 1-indexing to 0-indexing
        if (slot_idx < 0
            || slot_idx >= Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_NORMAL)) {
            Console_LogError(
                GS("general/osd/load_game_fail_invalid_slot"),
                request.slot_num);
            return CR_FAILURE;
        }
        slot = Savegame_NormalSlot(slot_idx);
    }

    if (Savegame_IsSlotFree(slot)) {
        Console_LogError(
            GS("general/osd/load_game_fail_unavailable_slot"), shown_slot_num);
        return CR_FAILURE;
    }

    GF_OverrideCommand((GF_COMMAND) {
        .action = GF_START_SAVED_GAME,
        .param = Savegame_SlotToParam(slot),
    });
    if (request.pool == SAVEGAME_SLOT_POOL_QUICK) {
        Console_Log(GS("general/osd/quick_load"), shown_slot_num);
    } else {
        Console_Log(GS("general/osd/load_game"), shown_slot_num);
    }
    return CR_SUCCESS;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    M_SLOT_REQUEST request;
    if (!M_TryParseLoadArg(ctx->args, &request)) {
        return CR_BAD_INVOCATION;
    }
    return M_ExecuteLoad(request);
}

static COMMAND_RESULT M_EntrypointQL(const COMMAND_CONTEXT *const ctx)
{
    M_SLOT_REQUEST request = {
        .pool = SAVEGAME_SLOT_POOL_QUICK,
        .slot_num = -1,
        .has_slot_num = false,
    };
    if (!M_TryParseQuickArg(
            ctx->args, &request.slot_num, &request.has_slot_num)) {
        return CR_BAD_INVOCATION;
    }
    return M_ExecuteLoad(request);
}

REGISTER_CONSOLE_COMMAND("load", M_Entrypoint, GS_ID("console/cmd/load/help"))
REGISTER_CONSOLE_COMMAND(
    "quickload", M_EntrypointQL, GS_ID("console/cmd/load/help"))
REGISTER_CONSOLE_COMMAND("ql", M_EntrypointQL, GS_ID("console/cmd/load/help"))

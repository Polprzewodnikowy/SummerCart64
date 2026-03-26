#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/savegame.h>

#include <stdio.h>

static bool M_TryParseQuickKeyword(const char *const args)
{
    char tail = '\0';
    return sscanf(args, " quick %c", &tail) == 0
        || sscanf(args, " q %c", &tail) == 0;
}

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

static COMMAND_RESULT M_HandleQuickSave(void)
{
    const SAVEGAME_SLOT_REF slot = Savegame_GetNextQuickSlot();
    if (!Savegame_IsValidSlotRef(slot)) {
        Console_LogError(GS("general/osd/quick_save_fail_no_slots"));
        return CR_FAILURE;
    }

    Savegame_Save(slot);
    Console_Log(GS("general/osd/quick_save"));
    return CR_SUCCESS;
}

static COMMAND_RESULT M_EntrypointQS(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    if (!M_IsBlank(ctx->args)) {
        return CR_BAD_INVOCATION;
    }
    return M_HandleQuickSave();
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    int32_t slot_num;
    if (String_ParseInteger(ctx->args, &slot_num)) {
        const int32_t slot_idx =
            slot_num - 1; // convert 1-indexing to 0-indexing
        if (slot_idx < 0
            || slot_idx >= Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_NORMAL)) {
            Console_LogError(
                GS("general/osd/save_game_fail_invalid_slot"), slot_num);
            return CR_BAD_INVOCATION;
        }

        Savegame_Save(Savegame_NormalSlot(slot_idx));
        Console_Log(GS("general/osd/save_game"), slot_num);
        return CR_SUCCESS;
    }

    if (M_TryParseQuickKeyword(ctx->args)) {
        return M_HandleQuickSave();
    }
    return CR_BAD_INVOCATION;
}

REGISTER_CONSOLE_COMMAND("save", M_Entrypoint, GS_ID("console/cmd/save/help"))
REGISTER_CONSOLE_COMMAND(
    "quicksave", M_EntrypointQS, GS_ID("console/cmd/save/help"))
REGISTER_CONSOLE_COMMAND("qs", M_EntrypointQS, GS_ID("console/cmd/save/help"))

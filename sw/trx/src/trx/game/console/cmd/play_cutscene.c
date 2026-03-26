#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/entries.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    int32_t cutscene_to_load = -1;
    if (String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    } else if (String_ParseInteger(ctx->args, &cutscene_to_load)) {
        cutscene_to_load--;
        const GF_LEVEL_TABLE *const level_table =
            GF_GetLevelTable(GFLT_CUTSCENES);
        if (cutscene_to_load < 0 || cutscene_to_load >= level_table->count) {
            Console_LogError(GS("general/osd/invalid_cutscene"));
            return CR_FAILURE;
        }
        const GF_LEVEL *const level = &level_table->levels[cutscene_to_load];
        GF_OverrideCommand((GF_COMMAND) {
            .action = GF_START_CINE,
            .param = cutscene_to_load,
        });
        Console_Log(GS("general/osd/play_cutscene"), level->num + 1);
        return CR_SUCCESS;
    } else {
        return CR_BAD_INVOCATION;
    }
}

REGISTER_CONSOLE_COMMAND(
    "cut", M_Entrypoint, GS_ID("console/cmd/play_cutscene/help"))
REGISTER_CONSOLE_COMMAND(
    "cutscene", M_Entrypoint, GS_ID("console/cmd/play_cutscene/help"))

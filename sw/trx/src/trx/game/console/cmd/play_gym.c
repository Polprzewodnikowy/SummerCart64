#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/game_strings/entries.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (String_IsEmpty(ctx->args)) {
        const GF_LEVEL *const level = GF_GetGymLevel();
        if (level == nullptr) {
            Console_LogError(GS("general/osd/invalid_level"));
            return CR_FAILURE;
        }
        GF_OverrideCommand((GF_COMMAND) {
            .action = GF_SELECT_GAME,
            .param = level->num,
        });
        Console_Log(GS("general/osd/play_level"), level->title);
        return CR_SUCCESS;
    } else {
        return CR_BAD_INVOCATION;
    }
}

REGISTER_CONSOLE_COMMAND(
    "gym", M_Entrypoint, GS_ID("console/cmd/play_gym/help"))
REGISTER_CONSOLE_COMMAND(
    "home", M_Entrypoint, GS_ID("console/cmd/play_gym/help"))

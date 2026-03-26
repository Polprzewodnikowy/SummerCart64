#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    bool enable;
    if (String_ParseBool(ctx->args, &enable)) {
        g_Config.debug.enable_invulnerability = enable;
        Config_Update();
    } else if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    } else {
        g_Config.debug.enable_invulnerability =
            !g_Config.debug.enable_invulnerability;
        Config_Update();
    }

    Console_Log(
        g_Config.debug.enable_invulnerability ? GS("console/cmd/immune/on")
                                              : GS("console/cmd/immune/off"));
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "immune", M_Entrypoint, GS_ID("console/cmd/immune/help"))
REGISTER_CONSOLE_COMMAND(
    "immunity", M_Entrypoint, GS_ID("console/cmd/immune/help"))

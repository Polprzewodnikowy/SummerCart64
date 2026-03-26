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
        g_Config.debug.enable_endless_sprint = enable;
        Config_Update();
    } else if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    } else {
        g_Config.debug.enable_endless_sprint =
            !g_Config.debug.enable_endless_sprint;
        Config_Update();
    }

    Console_Log(
        g_Config.debug.enable_endless_sprint
            ? GS("console/cmd/inf_sprint/on")
            : GS("console/cmd/inf_sprint/off"));
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "restless", M_Entrypoint, GS_ID("console/cmd/inf_sprint/help"))

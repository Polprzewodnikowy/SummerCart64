#include <trx/core/strings.h>
#include <trx/game/console/registry.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/lara/cheat.h>
#include <trx/game/lara/common.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!Game_IsPlayable()) {
        return CR_UNAVAILABLE;
    }

    bool enable;
    if (String_ParseBool(ctx->args, &enable)) {
        if (enable) {
            Lara_Cheat_EnterFlyMode();
        } else {
            Lara_Cheat_ExitFlyMode();
        }
        return CR_SUCCESS;
    }

    if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->water_status == LWS_CHEAT) {
        Lara_Cheat_ExitFlyMode();
    } else {
        Lara_Cheat_EnterFlyMode();
    }
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("fly", M_Entrypoint, GS_ID("console/cmd/fly/help"))

#include <trx/core/strings.h>
#include <trx/game/clock.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (String_Equivalent(ctx->args, "")) {
        Console_Log(GS("general/osd/speed_get"), Clock_GetTurboSpeed());
        return CR_SUCCESS;
    }

    int32_t num = -1;
    if (String_ParseInteger(ctx->args, &num)) {
        Clock_SetTurboSpeed(num);
        return CR_SUCCESS;
    }

    return CR_BAD_INVOCATION;
}

REGISTER_CONSOLE_COMMAND("speed", M_Entrypoint, GS_ID("console/cmd/speed/help"))

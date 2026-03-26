#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    Console_Clear();
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "cls|clear", M_Entrypoint, GS_ID("console/cmd/clear/help"))

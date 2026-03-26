#include <trx/core/strings.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_flow/common.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (!String_IsEmpty(ctx->args)) {
        return CR_BAD_INVOCATION;
    }

    GF_OverrideCommand((GF_COMMAND) { .action = GF_EXIT_TO_TITLE });
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("title", M_Entrypoint, GS_ID("console/cmd/title/help"))

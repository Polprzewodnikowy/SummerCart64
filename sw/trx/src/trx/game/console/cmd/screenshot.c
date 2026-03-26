#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/output.h>
#include <trx/game/screenshot.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *ctx)
{
    const char *const arg = ctx->args;
    if (arg == nullptr || String_IsEmpty(arg)) {
        Screenshot_Make(g_Config.rendering.screenshot_format);
    } else {
        Screenshot_MakeToPath(arg);
    }
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND(
    "screenshot", M_Entrypoint, GS_ID("console/cmd/screenshot/help"))

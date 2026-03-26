#include <trx/core/strings.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/shell/common.h>
#include <trx/game/shell/mod.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    if (String_IsEmpty(ctx->args)) {
        const SHELL_ARGS *const args = Shell_GetArgs();
        Console_Log("Currently loaded mod: %s", args->mod->name);
        return CR_SUCCESS;
    }

    const SHELL_MOD *const mod = Shell_GetModByName(ctx->args);
    if (!Shell_CanSwitchToMod(mod)) {
        Console_LogError("Invalid mod: %s", ctx->args);
        return CR_FAILURE;
    }

    Shell_RequestModSwitch(mod->name);
    GF_OverrideCommand((GF_COMMAND) { .action = GF_SWITCH_MOD });
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("mod", M_Entrypoint, GS_ID("console/cmd/mod/help"))

#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/console/cmd/config.h>
#include <trx/game/console/registry.h>

typedef struct {
    const char *prefix;
    void *target;
} COMMAND_TO_OPTION_MAP;

static COMMAND_TO_OPTION_MAP m_CommandToOptionMap[] = {
    { "braid", &g_Config.visuals.enable_braid },
    { "cheats", &g_Config.gameplay.enable_cheats },
    { "vsync", &g_Config.rendering.enable_vsync },
    { "wireframe", &g_Config.rendering.enable_wireframe },
    { "fps", &g_Config.rendering.fps },
    { "lighting", &g_Config.rendering.enable_lighting },
    { "textures", &g_Config.rendering.enable_textures },
    { nullptr, nullptr },
};

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    COMMAND_TO_OPTION_MAP *match = m_CommandToOptionMap;
    while (match->target != nullptr) {
        if (String_Equivalent(match->prefix, ctx->prefix)) {
            return Console_Cmd_Config_Helper(
                Console_Cmd_Config_GetOptionFromTarget(match->target),
                ctx->args);
        }
        match++;
    }

    return CR_FAILURE;
}

REGISTER_CONSOLE_COMMAND("braid", M_Entrypoint, GS_ID("console/cmd/braid/help"))
REGISTER_CONSOLE_COMMAND(
    "cheats", M_Entrypoint, GS_ID("console/cmd/cheats/help"))
REGISTER_CONSOLE_COMMAND("vsync", M_Entrypoint, GS_ID("console/cmd/vsync/help"))
REGISTER_CONSOLE_COMMAND(
    "wireframe", M_Entrypoint, GS_ID("console/cmd/wireframe/help"))
REGISTER_CONSOLE_COMMAND("fps", M_Entrypoint, GS_ID("console/cmd/fps/help"))
REGISTER_CONSOLE_COMMAND(
    "lighting", M_Entrypoint, GS_ID("console/cmd/lighting/help"))
REGISTER_CONSOLE_COMMAND(
    "textures", M_Entrypoint, GS_ID("console/cmd/textures/help"))

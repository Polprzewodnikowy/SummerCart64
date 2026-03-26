#include <trx/core/memory.h>
#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>

#include <string.h>

static COMMAND_RESULT M_ListAllCommands(void)
{
    VECTOR *const vec = Console_Registry_GetAll();
    const CONSOLE_COMMAND **list =
        (const CONSOLE_COMMAND **)Vector_GetData(vec);

    // compute buffer size
    int32_t total = 0;
    for (int32_t i = 0; i < vec->count; i++) {
        if (list[i]->help_id == nullptr) {
            continue;
        }
        total += strlen(list[i]->prefix) + (i + 1 < vec->count ? 2 : 0);
    }
    char *buf = Memory_Alloc(total + 1);
    for (int32_t i = 0; i < vec->count; i++) {
        if (list[i]->help_id == nullptr) {
            continue;
        }
        strcat(buf, list[i]->prefix);
        if (i + 1 < vec->count) {
            strcat(buf, ", ");
        }
    }
    Console_Log("%s", GS("console/cmd/help/list"));
    Console_Log("%s", buf);
    Memory_Free(buf);
    Vector_Free(vec);
    return CR_SUCCESS;
}

static COMMAND_RESULT M_ShowSpecificCommand(const char *const cmd_name)
{
    const CONSOLE_COMMAND *cmd = Console_Registry_Get(cmd_name);
    if (cmd == nullptr || cmd->help_id == nullptr) {
        Console_LogError(GS("general/osd/unknown_command"), cmd_name);
        return CR_FAILURE;
    }
    const char *const help = GameString_Get(cmd->help_id);
    Console_Log("%s", help);
    return CR_SUCCESS;
}

static COMMAND_RESULT M_Help(const COMMAND_CONTEXT *const ctx)
{
    if (ctx->args != nullptr && *ctx->args != '\0') {
        return M_ShowSpecificCommand(ctx->args);
    }
    return M_ListAllCommands();
}

REGISTER_CONSOLE_COMMAND("help", M_Help, GS_ID("console/cmd/help/help"))

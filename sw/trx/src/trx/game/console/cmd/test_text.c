#include <trx/game/console/common.h>
#include <trx/game/console/registry.h>

#include <stdio.h>

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    char buf[500] = {};
    char *ptr = buf;
    for (int32_t y = 0; y < 3; y++) {
        for (int32_t x = 0; x < 4; x++) {
            const int32_t i = y * 4 + x;
            ptr += sprintf(ptr, "\\{color %d}Color %d\\{/color}   ", i, i);
        }
        ptr += sprintf(ptr, "\n");
    }
    ptr += sprintf(ptr, "\\{dim}Dim\\{/dim}\n");
    ptr += sprintf(ptr, "Secrets: \\{secret 1}\\{secret 2}\\{secret 3}");
    Console_Log("%s", buf);
    return CR_SUCCESS;
}

REGISTER_CONSOLE_COMMAND("test-text", M_Entrypoint, nullptr)

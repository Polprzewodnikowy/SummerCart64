#include <trx/game/shell/session.h>

#include <trx/core/memory.h>

SHELL_SESSION *ShellSession_Create(void)
{
    return Memory_Alloc(sizeof(SHELL_SESSION));
}

void ShellSession_Free(SHELL_SESSION *const session)
{
    if (session != nullptr) {
        if (session->args != nullptr) {
            Shell_FreeArgs((SHELL_ARGS *)session->args);
        }
        Memory_Free(session);
    }
}

void ShellSession_UseArgs(
    SHELL_SESSION *const session, const SHELL_ARGS *const args)
{
    if (session->args != nullptr) {
        Shell_FreeArgs((SHELL_ARGS *)session->args);
    }
    session->args = args;
}

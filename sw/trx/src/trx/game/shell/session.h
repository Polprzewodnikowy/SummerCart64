#pragma once

#include <trx/game/shell/args.h>

typedef struct {
    const SHELL_ARGS *args;
} SHELL_SESSION;

SHELL_SESSION *ShellSession_Create(void);
// Frees session and currently attached args (if any).
void ShellSession_Free(SHELL_SESSION *session);

// Replaces session args ownership.
// The session takes ownership of `args` and frees previously attached args.
void ShellSession_UseArgs(SHELL_SESSION *session, const SHELL_ARGS *args);

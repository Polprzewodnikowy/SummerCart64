#pragma once
#include <trx/game/console/enum.h>
#include <trx/game/game_strings/entries.h>

typedef struct {
    const struct CONSOLE_COMMAND *cmd;
    const char *prefix;
    const char *args;
} COMMAND_CONTEXT;

typedef struct CONSOLE_COMMAND {
    const char *prefix;
    COMMAND_RESULT (*proc)(const COMMAND_CONTEXT *ctx);
    GAME_STRING_ID help_id;
} CONSOLE_COMMAND;

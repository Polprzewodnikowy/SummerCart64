#pragma once

#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/game/console/types.h>

#include <stddef.h>

void Console_Registry_Add(CONSOLE_COMMAND cmd);

const CONSOLE_COMMAND *Console_Registry_Get(const char *cmdline);

// Retrieve a vector containing pointers to all registered console commands.
// The returned vector must be freed via Vector_Free().
VECTOR *Console_Registry_GetAll(void);

#define REGISTER_CONSOLE_COMMAND(prefix_, proc_, help_)                        \
    __attribute__((__constructor__)) static void CONCAT(                       \
        M_Register_, __LINE__)(void)                                           \
    {                                                                          \
        Console_Registry_Add((CONSOLE_COMMAND) {                               \
            .prefix = prefix_, .proc = proc_, .help_id = help_ });             \
    }

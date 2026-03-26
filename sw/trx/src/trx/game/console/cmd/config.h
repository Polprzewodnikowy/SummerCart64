#pragma once

#include <trx/config/option.h>
#include <trx/core/vector.h>
#include <trx/game/console/common.h>

#include <stddef.h>

char *Console_Cmd_Config_NormalizeKey(const char *key);
bool Console_Cmd_Config_SetCurrentValue(
    const CONFIG_OPTION *option, const char *new_value);

// Returns a vector of STRING_FUZZY_MATCH entries for the given key.
// Caller must free the result with Vector_Free().
VECTOR *Console_Cmd_Config_GetOptionsFromKey(const char *key);

// Returns a pointer to a CONFIG_OPTION from a pointer into g_Config.
const CONFIG_OPTION *Console_Cmd_Config_GetOptionFromTarget(const void *target);

COMMAND_RESULT Console_Cmd_Config_Helper(
    const CONFIG_OPTION *option, const char *new_value);

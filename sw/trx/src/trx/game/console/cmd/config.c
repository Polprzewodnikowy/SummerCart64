#include <trx/game/console/cmd/config.h>

#include <trx/config.h>
#include <trx/config/dynamic_enum.h>
#include <trx/core/colors.h>
#include <trx/core/enum_map.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/console/registry.h>
#include <trx/game/game_strings/entries.h>

#include <stdio.h>
#include <string.h>

static char *M_NormalizeValue(const char *const value)
{
    if (value == nullptr) {
        return nullptr;
    }

    char *const result = Memory_DupStr(value);
    for (uint32_t i = 0; i < strlen(result); i++) {
        if (result[i] == '_') {
            result[i] = '-';
        }
    }
    return result;
}

static const char *M_Resolve(const char *const option_name)
{
    const char *dot = strrchr(option_name, '.');
    if (dot) {
        return dot + 1;
    }
    return option_name;
}

static const CONFIG_OPTION *M_GetOptionFromKey(const char *const key)
{
    VECTOR *source = Vector_Create(sizeof(STRING_FUZZY_SOURCE));

    for (const CONFIG_OPTION *option = Config_GetOptionMap();
         option->name != nullptr; option++) {
        STRING_FUZZY_SOURCE source_item = {
            .key = (const char *)Console_Cmd_Config_NormalizeKey(option->name),
            .value = (void *)option,
            .weight = 1,
        };
        Vector_Add(source, &source_item);
    }

    VECTOR *matches = String_FuzzyMatch(key, source);
    const CONFIG_OPTION *result = nullptr;
    if (matches->count == 0) {
        Console_LogError(GS("general/osd/config_option_unknown_option"), key);
    } else if (matches->count == 1) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, 0);
        result = match->value;
    } else if (matches->count == 2) {
        const STRING_FUZZY_MATCH *const match1 = Vector_Get(matches, 0);
        const STRING_FUZZY_MATCH *const match2 = Vector_Get(matches, 1);
        Console_LogError(
            GS("general/osd/ambiguous_input_2"), match1->key, match2->key);
    } else if (matches->count >= 3) {
        const STRING_FUZZY_MATCH *const match1 = Vector_Get(matches, 0);
        const STRING_FUZZY_MATCH *const match2 = Vector_Get(matches, 1);
        Console_LogError(
            GS("general/osd/ambiguous_input_3"), match1->key, match2->key);
    }

    for (int32_t i = 0; i < source->count; i++) {
        const STRING_FUZZY_SOURCE *const source_item = Vector_Get(source, i);
        Memory_Free((char *)source_item->key);
    }

    Vector_Free(matches);
    Vector_Free(source);
    return result;
}

static char *M_FormatValuesList(const VECTOR *const values)
{
    if (values == nullptr || values->count == 0) {
        return nullptr;
    }

    char *result = nullptr;
    for (int32_t i = 0; i < values->count; i++) {
        const char *const value = *(char **)Vector_Get(values, i);
        if (value == nullptr) {
            continue;
        }

        char *normalized = M_NormalizeValue(value);
        if (normalized == nullptr) {
            continue;
        }

        if (result == nullptr) {
            result = normalized;
        } else {
            char *const joined = String_Format("%s, %s", result, normalized);
            Memory_FreePointer(&result);
            Memory_FreePointer(&normalized);
            result = joined;
        }
    }

    return result;
}

static char *M_FormatDynamicEnumDefaults(const CONFIG_OPTION *const option)
{
    VECTOR *const values = Vector_Create(sizeof(char *));
    const char *const default_value = "-";
    Vector_Add(values, &default_value);

    const int32_t value_count = Config_DynamicEnum_GetValueCount(option);
    for (int32_t i = 0; i < value_count; i++) {
        const char *const value = Config_DynamicEnum_GetValueAt(option, i);
        if (value != nullptr) {
            Vector_Add(values, &value);
        }
    }

    char *const result = M_FormatValuesList(values);
    Vector_Free(values);
    return result;
}

static char *M_GetValueForConsole(const CONFIG_OPTION *const option)
{
    if ((option->type == COT_STRING || option->type == COT_DYNAMIC_ENUM)
        && *(char **)option->target == nullptr) {
        return Memory_DupStr("(null)");
    }

    const char *const value = Config_GetOptionValueAsString(option, false);
    if (value == nullptr) {
        return nullptr;
    }

    if (option->type == COT_ENUM || option->type == COT_DYNAMIC_ENUM) {
        return M_NormalizeValue(value);
    }
    return Memory_DupStr(value);
}

static bool M_TryApplyOptionValue(
    const CONFIG_OPTION *const option, const char *const new_value)
{
    if (strcmp(new_value, "-") == 0) {
        return Config_RestoreOptionDefault(option->target);
    }
    if (Config_SetOptionValueFromString(option, new_value)) {
        return true;
    }

    if (option->type != COT_ENUM && option->type != COT_DYNAMIC_ENUM) {
        return false;
    }

    char *normalized = M_NormalizeValue(new_value);
    if (normalized != nullptr) {
        const bool different = strcmp(normalized, new_value) != 0;
        if (different && Config_SetOptionValueFromString(option, normalized)) {
            Memory_FreePointer(&normalized);
            return true;
        }
        Memory_FreePointer(&normalized);
    }

    char *underscore = Memory_DupStr(new_value);
    bool different = false;
    for (uint32_t i = 0; i < strlen(underscore); i++) {
        if (underscore[i] == '-') {
            underscore[i] = '_';
            different = true;
        }
    }
    if (different && Config_SetOptionValueFromString(option, underscore)) {
        Memory_FreePointer(&underscore);
        return true;
    }
    Memory_FreePointer(&underscore);
    return false;
}

// Builds a source list of all options and returns fuzzy-match results.
// Caller must free the result vector with Vector_Free().
static VECTOR *M_GetOptionsFuzzy(const char *const key)
{
    VECTOR *source = Vector_Create(sizeof(STRING_FUZZY_SOURCE));

    for (const CONFIG_OPTION *option = Config_GetOptionMap();
         option->name != nullptr; option++) {
        STRING_FUZZY_SOURCE source_item = {
            .key = (const char *)Console_Cmd_Config_NormalizeKey(option->name),
            .value = (void *)option,
            .weight = 1,
        };
        Vector_Add(source, &source_item);
    }

    VECTOR *matches = String_FuzzyMatch(key, source);

    for (int32_t i = 0; i < source->count; i++) {
        const STRING_FUZZY_SOURCE *const source_item = Vector_Get(source, i);
        Memory_Free((char *)source_item->key);
    }

    Vector_Free(source);
    return matches;
}

static COMMAND_RESULT M_Entrypoint(const COMMAND_CONTEXT *const ctx)
{
    COMMAND_RESULT result = CR_BAD_INVOCATION;

    char *key = Memory_DupStr(ctx->args);
    char *const space = strchr(key, ' ');
    const char *new_value = nullptr;
    if (space != nullptr) {
        new_value = space + 1;
        space[0] = '\0'; // nullptr-terminate the key
    }

    const CONFIG_OPTION *const option = M_GetOptionFromKey(key);
    if (option == nullptr) {
        result = CR_FAILURE;
    } else {
        result = Console_Cmd_Config_Helper(option, new_value);
    }

cleanup:
    Memory_FreePointer(&key);
    return result;
}

// Return a comma-delimited list of valid values for the option.
// Caller must free the result with Memory_Free*().
static char *M_GetAvailableOptions(const CONFIG_OPTION *const option)
{
    if (option == nullptr) {
        return nullptr;
    }

    switch (option->type) {
    case COT_BOOL:
        return Memory_DupStr(GS("general/osd/command_bool"));

    case COT_INT32:
        return Memory_DupStr(GS("general/osd/command_integer"));

    case COT_DOUBLE:
    case COT_FLOAT:
        return Memory_DupStr(GS("general/osd/command_decimal"));

    case COT_FLOAT_PERCENT:
        return Memory_DupStr(GS("general/osd/command_percent"));

    case COT_ENUM: {
        const char *enum_name = (const char *)option->param;
        VECTOR *const values = EnumMap_ListValues(enum_name);
        if (values == nullptr) {
            return nullptr;
        }
        char *const result = M_FormatValuesList(values);
        Vector_Free(values);
        return result;
    }

    case COT_DYNAMIC_ENUM:
        return M_FormatDynamicEnumDefaults(option);

    case COT_STRING:
        return nullptr;

    default:
        return nullptr;
    }
}

char *Console_Cmd_Config_NormalizeKey(const char *key)
{
    // TODO: Once we support arbitrary glyphs, this conversion should
    // no longer be necessary.
    char *result = Memory_DupStr(key);
    for (uint32_t i = 0; i < strlen(result); i++) {
        if (result[i] == '_') {
            result[i] = '-';
        }
    }
    return result;
}

const CONFIG_OPTION *Console_Cmd_Config_GetOptionFromTarget(
    const void *const target)
{
    for (const CONFIG_OPTION *option = Config_GetOptionMap();
         option->name != nullptr; option++) {
        if (option->target == target) {
            return option;
        }
    }

    return nullptr;
}

COMMAND_RESULT Console_Cmd_Config_Helper(
    const CONFIG_OPTION *const option, const char *const new_value)
{
    ASSERT(option != nullptr);

    char *normalized_name = Console_Cmd_Config_NormalizeKey(option->name);
    COMMAND_RESULT result = CR_FAILURE;

    if (new_value == nullptr || String_IsEmpty(new_value)) {
        char *value_str = M_GetValueForConsole(option);
        if (value_str == nullptr) {
            result = CR_FAILURE;
            goto cleanup;
        }
        Console_Log(
            GS("general/osd/config_option_get"), normalized_name, value_str);
        Memory_FreePointer(&value_str);
        result = CR_SUCCESS;
        goto cleanup;
    }

    if (M_TryApplyOptionValue(option, new_value)) {
        Config_Update();
        char *value_str = M_GetValueForConsole(option);
        ASSERT(value_str != nullptr);
        Console_Log(
            GS("general/osd/config_option_set"), normalized_name, value_str);
        Memory_FreePointer(&value_str);
        result = CR_SUCCESS;
    } else {
        // Report bad invocation on the provided new value
        Console_LogError(GS("general/osd/command_bad_invocation"), new_value);
        char *available_options = M_GetAvailableOptions(option);
        if (available_options != nullptr) {
            Console_Log(
                GS("general/osd/command_valid_values"), available_options);
            Memory_FreePointer(&available_options);
        }
        result = CR_FAILURE;
    }

cleanup:
    Memory_FreePointer(&normalized_name);
    return result;
}

VECTOR *Console_Cmd_Config_GetOptionsFromKey(const char *const key)
{
    return M_GetOptionsFuzzy(key);
}

REGISTER_CONSOLE_COMMAND("set", M_Entrypoint, GS_ID("console/cmd/set/help"))

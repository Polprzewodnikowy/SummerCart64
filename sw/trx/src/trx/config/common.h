#pragma once

#include <trx/config/option.h>
#include <trx/core/event_manager.h>
#include <trx/core/json.h>

#include <stdint.h>

void Config_ApplyDefaultSettings(void);
bool Config_Read(const char *default_path, const char *enforced_path);
bool Config_Write(void);
bool Config_Update(void);

const CONFIG_OPTION *Config_GetOptionMap(void);

int32_t Config_SubscribeChanges(EVENT_LISTENER listener, void *user_data);
void Config_UnsubscribeChanges(int32_t listener_id);

// Retrieves CONFIG_OPTION related to the target setting (a pointer into a
// g_Config property).
const CONFIG_OPTION *Config_GetOption(const void *target);

// Returns true if a given setting was enforced by the game flow file.
bool Config_IsOptionEnforced(const void *target);

// Returns true if a given setting should be hidden in settings dialogs.
bool Config_IsOptionHidden(const void *target);

// Returns whether the given setting's current value is the same as its default.
bool Config_IsOptionAtDefault(const void *target);

// Restores the given setting's default value.
bool Config_RestoreOptionDefault(const void *target);

// Get a flat string name of an option.
const char *Config_ResolveOptionName(const char *option_name);

// Retrieve an option given a string path.
const CONFIG_OPTION *Config_GetOptionByPath(const char *path);

// Returns translated title for a config option.
const char *Config_GetOptionTitle(const CONFIG_OPTION *opt);

// Returns translated description for a config option.
const char *Config_GetOptionDescription(const CONFIG_OPTION *opt);

// Formats the current value of a config option as a static string.
// The string must not be freed and is short lived.
const char *Config_GetOptionValueAsString(
    const CONFIG_OPTION *option, bool human_readable);

// Normalizes an option value string using the same parser/formatter as config
// runtime values.
// Returns an allocated string that must be freed by the caller.
char *Config_NormalizeOptionValueString(
    const CONFIG_OPTION *option, const char *value, bool human_readable);

// Updates the given setting's value from string.
bool Config_SetOptionValueFromString(
    const CONFIG_OPTION *option, const char *new_value);

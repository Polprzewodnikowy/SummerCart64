#pragma once

#include <stdint.h>

// A config preset represents a named set of settings to apply all at once.
// Presets are loaded from cfg/presets/*.json5 at startup.

typedef struct {
    char *name_gs;
    // Flat arrays of config key paths and their string values.
    char **keys;
    char **values;
    int32_t setting_count;
} CONFIG_PRESET;

// Load all presets from the game's cfg/presets/ directory.
void Config_Presets_ScanFiles(void);

// Number of loaded presets.
int32_t Config_Presets_GetCount(void);

// Returns the preset at the given index, or nullptr if out of range.
const CONFIG_PRESET *Config_Presets_Get(int32_t idx);

// Apply all settings from preset[idx] and write config to disk.
void Config_Presets_Apply(int32_t idx);

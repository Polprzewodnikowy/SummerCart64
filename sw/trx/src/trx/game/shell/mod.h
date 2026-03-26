#pragma once

#include <stdint.h>

typedef enum {
    MOD_BASE_GAME,
    MOD_EXPANSION_PACK,
    MOD_MISC,
    MOD_DIRECT_LEVEL,
    MOD_CUSTOM,
} SHELL_MOD_TYPE;

typedef struct {
    char *name;
    char *title;
    SHELL_MOD_TYPE mod_type;
    int32_t engine_version;
    char *base_mod;
    bool is_available;
    bool is_valid;
} SHELL_MOD;

void Shell_ScanAvailableMods(void);
void Shell_ValidateMods(void);
int32_t Shell_GetModCount(void);
const SHELL_MOD *Shell_GetMod(int32_t index);
const SHELL_MOD *Shell_GetModByName(const char *name);
const SHELL_MOD *Shell_SelectStartupMod(int32_t engine_version);
const SHELL_MOD *Shell_GetModByType(
    SHELL_MOD_TYPE mod_type, int32_t engine_version);
bool Shell_CanSwitchToMod(const SHELL_MOD *mod);
bool Shell_IsCurrentMod(const char *name);

const char *Shell_GetCommonStringsPath(void);
const char *Shell_GetBaseGameStringsPath(const SHELL_MOD *mod);
const char *Shell_GetGameStringsPath(const SHELL_MOD *mod);
const char *Shell_GetGameFlowPath(const SHELL_MOD *mod);

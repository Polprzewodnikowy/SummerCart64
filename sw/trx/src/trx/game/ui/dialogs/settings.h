#pragma once

#include <trx/config/option.h>
#include <trx/config/types.h>
#include <trx/core/utils.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/settings_tabs.h>
#include <trx/game/ui/dialogs/text.h>
#include <trx/game/ui/elements/tab_switch.h>
#include <trx/game/ui/scrollable.h>

typedef struct {
    const char *(*format_value)(const struct UI_SETTINGS_OPTION *option);
    bool (*can_change_value)(
        const struct UI_SETTINGS_OPTION *option, int32_t dir);
    bool (*request_change_value)(
        const struct UI_SETTINGS_OPTION *option, int32_t dir);
    bool (*is_available)(const struct UI_SETTINGS_OPTION *option);
    bool (*is_visible)(const struct UI_SETTINGS_OPTION *option);
    bool (*is_enum_value_available)(
        const struct UI_SETTINGS_OPTION *option, int32_t value);
} UI_SETTINGS_CUSTOM_OPITON_HANDLER;

typedef struct UI_SETTINGS_OPTION {
    // A custom handler that must have all the function pointers filled,
    UI_SETTINGS_CUSTOM_OPITON_HANDLER custom_handler;

    // ...or a convenience default handler options
    struct {
        void *target;
        int32_t min_value;
        int32_t max_value;
        int32_t delta_slow;
        int32_t delta_fast;
        const void *misc;
    };
} UI_SETTINGS_OPTION;

#define X_UI_CFG(TARGET_, ...) { .target = &g_Config.TARGET_, ##__VA_ARGS__ },

#define X_UI_CFG_DYN_ENUM(TARGET_, ...)                                        \
    X_UI_CFG(TARGET_, .delta_slow = 1, .delta_fast = 1, ##__VA_ARGS__)

#define X_UI_CFG_ENUM(TARGET_, ...)                                            \
    X_UI_CFG(TARGET_, .delta_slow = 1, .delta_fast = 1, ##__VA_ARGS__)

#define X_UI_CFG_RGB888(TARGET_, ...)                                          \
    X_UI_CFG(TARGET_, .min_value = 0, .max_value = 255, ##__VA_ARGS__)

typedef struct UI_SETTINGS_DIALOG_STATE UI_SETTINGS_DIALOG_STATE;

UI_SETTINGS_DIALOG_STATE *UI_SettingsDialog_Init(
    GAME_STRING_ID title, int32_t tab_count, const UI_SETTINGS_TAB *tabs);
void UI_SettingsDialog_Free(UI_SETTINGS_DIALOG_STATE *s);
bool UI_SettingsDialog_Control(UI_SETTINGS_DIALOG_STATE *s);

void UI_SettingsDialog(UI_SETTINGS_DIALOG_STATE *s);

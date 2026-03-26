#pragma once

// settings tab contracts and tab factory helpers

#include <trx/core/utils.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/ui/scrollable.h>

typedef struct UI_SETTINGS_OPTION UI_SETTINGS_OPTION;

typedef enum {
    UI_SETTINGS_PHASE_NAVIGATE_TABS,
    UI_SETTINGS_PHASE_EDIT_SETTINGS,
} UI_SETTINGS_PHASE;

typedef bool (*UI_SETTINGS_TAB_CONTROL_FUNC)(
    void *user_data, UI_SETTINGS_PHASE *phase);
typedef void (*UI_SETTINGS_TAB_DRAW_FUNC)(
    void *user_data, UI_SETTINGS_PHASE phase, float row_width);
typedef void (*UI_SETTINGS_TAB_DRAW_FOOTER_FUNC)(
    void *user_data, UI_SETTINGS_PHASE phase);
typedef void (*UI_SETTINGS_TAB_FREE_FUNC)(void *user_data);
typedef void (*UI_SETTINGS_TAB_DRAW_OVERLAY_FUNC)(void *user_data);
typedef UI_SCROLLABLE *(*UI_SETTINGS_TAB_GET_SCROLLABLE_FUNC)(void *user_data);
typedef void (*UI_SETTINGS_TAB_RECOMPUTE_FUNC)(
    void *user_data, int32_t visible_rows);
typedef float (*UI_SETTINGS_TAB_GET_CONTENT_WIDTH_FUNC)(void *user_data);
typedef float (*UI_SETTINGS_TAB_GET_CONTENT_HEIGHT_FUNC)(void *user_data);
typedef int32_t (*UI_SETTINGS_TAB_GET_ITEM_COUNT_FUNC)(void *user_data);

typedef struct UI_SETTINGS_TAB_OPS {
    UI_SETTINGS_TAB_CONTROL_FUNC control;
    UI_SETTINGS_TAB_DRAW_FUNC draw;
    UI_SETTINGS_TAB_DRAW_FOOTER_FUNC draw_footer;
    UI_SETTINGS_TAB_DRAW_OVERLAY_FUNC draw_overlay;
    UI_SETTINGS_TAB_FREE_FUNC free;
    UI_SETTINGS_TAB_GET_SCROLLABLE_FUNC get_scrollable;
    UI_SETTINGS_TAB_RECOMPUTE_FUNC recompute;
    UI_SETTINGS_TAB_GET_CONTENT_WIDTH_FUNC get_content_width;
    UI_SETTINGS_TAB_GET_CONTENT_HEIGHT_FUNC get_content_height;
    UI_SETTINGS_TAB_GET_ITEM_COUNT_FUNC get_item_count;
} UI_SETTINGS_TAB_OPS;

typedef struct UI_SETTINGS_TAB {
    GAME_STRING_ID header_gs;
    const UI_SETTINGS_TAB_OPS *ops;
    void *user_data;
} UI_SETTINGS_TAB;

UI_SETTINGS_TAB UI_SettingsTab_MakeEditor(
    GAME_STRING_ID header_gs, const UI_SETTINGS_OPTION *options);
UI_SETTINGS_TAB UI_SettingsTab_MakePresets(GAME_STRING_ID header_gs);

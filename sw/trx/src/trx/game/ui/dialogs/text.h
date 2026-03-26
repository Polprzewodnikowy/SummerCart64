#pragma once

#include <trx/core/vector.h>
#include <trx/game/ui/common.h>

// A widget to cycle through several pages of a text content.

typedef struct UI_TEXT_DIALOG_STATE UI_TEXT_DIALOG_STATE;
typedef void (*UI_TEXT_DIALOG_FOOTER_FUNC)(void *user_data);

typedef struct {
    const char *title_raw;
    const char *text_raw;
    UI_TEXT_DIALOG_FOOTER_FUNC footer_func;
    void *footer_user_data;
} UI_TEXT_DIALOG_SETTINGS;

// state functions
UI_TEXT_DIALOG_STATE *UI_TextDialog_Init(
    size_t wrap_width, size_t wrap_max_lines, bool is_heavy);

// Handle page-left/right input.  Call before UI_TextDialog().
void UI_TextDialog_Control(UI_TEXT_DIALOG_STATE *state);

// Free any allocated buffers.  Call when dialog is dismissed.
void UI_TextDialog_Free(UI_TEXT_DIALOG_STATE *state);

// draw functions

// Draw and manage a text dialog in one call.  Rewraps/recapitalizes only
// when title_raw/text_raw differ (compared via strcmp).  Call every frame.
void UI_TextDialog(
    UI_TEXT_DIALOG_STATE *state, const char *title_raw, const char *text_raw);

// Same as UI_TextDialog(), with optional settings.
void UI_TextDialogEx(
    UI_TEXT_DIALOG_STATE *state, UI_TEXT_DIALOG_SETTINGS settings);

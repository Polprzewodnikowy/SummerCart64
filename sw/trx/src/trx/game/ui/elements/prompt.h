#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/flash.h>

#include <stdint.h>

// A text edit widget that collects text input from the player.
// Needs to be in focus to work, otherwise is inactive.

typedef struct {
    bool is_focused;
    int32_t caret_pos;
    int32_t current_text_capacity;
    char *current_text;

    int32_t listener1;
    int32_t listener2;

    UI_FLASH_STATE flash;
} UI_PROMPT_STATE;

// state functions
void UI_Prompt_Init(UI_PROMPT_STATE *s);
void UI_Prompt_Free(UI_PROMPT_STATE *s);
void UI_Prompt_Control(UI_PROMPT_STATE *s);
void UI_Prompt_Clear(UI_PROMPT_STATE *s);
void UI_Prompt_SetFocus(UI_PROMPT_STATE *s, bool is_focused);
void UI_Prompt_ChangeText(UI_PROMPT_STATE *s, const char *new_text);

// draw functions
void UI_Prompt(UI_PROMPT_STATE *s);

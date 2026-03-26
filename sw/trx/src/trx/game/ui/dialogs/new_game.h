#pragma once

// A new game mode selector dialog.

#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/requester.h>

typedef enum {
    UI_NEW_GAME_CHOICE_NG,
    UI_NEW_GAME_CHOICE_NGPLUS,
    UI_NEW_GAME_CHOICE_JP_NG,
    UI_NEW_GAME_CHOICE_JP_NGPLUS,
    UI_NEW_GAME_CHOICE_SWITCH_MOD,
    UI_NEW_GAME_CHOICE_PLAY_PREV_LEVELS,
    UI_NEW_GAME_CHOICE_STORY_SO_FAR,
} UI_NEW_GAME_CHOICE;

typedef struct UI_NEW_GAME_STATE UI_NEW_GAME_STATE;

// state functions
bool UI_NewGame_HasModChoices(void);
UI_NEW_GAME_STATE *UI_NewGame_Init(bool show_play_prev_levels);
int32_t UI_NewGame_Control(UI_NEW_GAME_STATE *s);
void UI_NewGame_Free(UI_NEW_GAME_STATE *s);

// draw functions
void UI_NewGame(UI_NEW_GAME_STATE *s);

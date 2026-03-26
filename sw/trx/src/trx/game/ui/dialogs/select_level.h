// UI dialog for selecting a level within a save slot

#pragma once

#include <trx/game/savegame/types.h>
#include <trx/game/ui/common.h>

#include <stdint.h>

#define UI_SELECT_LEVEL_CHOICE_NOOP -1
#define UI_SELECT_LEVEL_CHOICE_CANCEL -2
#define UI_SELECT_LEVEL_CHOICE_PLAY_STORY_SO_FAR -3

typedef struct UI_SELECT_LEVEL_DIALOG_STATE UI_SELECT_LEVEL_DIALOG_STATE;

struct UI_SELECT_LEVEL_DIALOG_STATE *UI_SelectLevelDialog_Init(
    SAVEGAME_SLOT_REF save_slot);

void UI_SelectLevelDialog_Free(struct UI_SELECT_LEVEL_DIALOG_STATE *s);

int32_t UI_SelectLevelDialog_Control(struct UI_SELECT_LEVEL_DIALOG_STATE *s);

void UI_SelectLevelDialog(struct UI_SELECT_LEVEL_DIALOG_STATE *s);

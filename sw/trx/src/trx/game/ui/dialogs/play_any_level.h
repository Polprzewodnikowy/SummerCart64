// UI dialog for selecting a level within a save slot

#pragma once

#include <trx/game/ui/common.h>

#include <stdint.h>

#define UI_PLAY_ANY_LEVEL_CHOICE_NO_CHOICE UI_REQUESTER_NO_CHOICE
#define UI_PLAY_ANY_LEVEL_CHOICE_CANCEL UI_REQUESTER_CANCEL

typedef struct UI_PLAY_ANY_LEVEL_DIALOG_STATE UI_PLAY_ANY_LEVEL_DIALOG_STATE;

struct UI_PLAY_ANY_LEVEL_DIALOG_STATE *UI_PlayAnyLevelDialog_Init(void);

void UI_PlayAnyLevelDialog_Free(struct UI_PLAY_ANY_LEVEL_DIALOG_STATE *s);

int32_t UI_PlayAnyLevelDialog_Control(struct UI_PLAY_ANY_LEVEL_DIALOG_STATE *s);

void UI_PlayAnyLevelDialog(struct UI_PLAY_ANY_LEVEL_DIALOG_STATE *s);

// UI dialog for selecting a save slot (load or save game)

#pragma once

#include <trx/game/savegame/types.h>
#include <trx/game/ui/common.h>

typedef enum {
    UI_SAVE_SLOT_DIALOG_LOAD_GAME,
    UI_SAVE_SLOT_DIALOG_SAVE_GAME,
    UI_SAVE_SLOT_DIALOG_GENERIC,
} UI_SAVE_SLOT_DIALOG_TYPE;

typedef enum {
    UI_SAVE_SLOT_DIALOG_NO_CHOICE,
    UI_SAVE_SLOT_DIALOG_CANCEL,
    UI_SAVE_SLOT_DIALOG_CONFIRM,
} UI_SAVE_SLOT_DIALOG_ACTION;

typedef struct {
    UI_SAVE_SLOT_DIALOG_ACTION action;
    SAVEGAME_SLOT_REF slot;
} UI_SAVE_SLOT_DIALOG_CHOICE;

typedef struct UI_SAVE_SLOT_DIALOG_STATE UI_SAVE_SLOT_DIALOG_STATE;

// state functions
struct UI_SAVE_SLOT_DIALOG_STATE *UI_SaveSlotDialog_Init(
    UI_SAVE_SLOT_DIALOG_TYPE type, SAVEGAME_SLOT_REF initial_slot);
void UI_SaveSlotDialog_Free(struct UI_SAVE_SLOT_DIALOG_STATE *s);
UI_SAVE_SLOT_DIALOG_CHOICE UI_SaveSlotDialog_Control(
    struct UI_SAVE_SLOT_DIALOG_STATE *s);

// draw functions
void UI_SaveSlotDialog(const struct UI_SAVE_SLOT_DIALOG_STATE *s);

#pragma once

#include <trx/game/inventory_ring/types.h>
#include <trx/game/savegame/types.h>

typedef enum {
    PASSPORT_ACTION_LOAD_GAME,
    PASSPORT_ACTION_SELECT_LEVEL,
    PASSPORT_ACTION_GLOBE_SELECT,
    PASSPORT_ACTION_STORY_SO_FAR,
    PASSPORT_ACTION_SAVE_GAME,
    PASSPORT_ACTION_NEW_GAME,
    PASSPORT_ACTION_SWITCH_MOD,
    PASSPORT_ACTION_RESTART,
    PASSPORT_ACTION_EXIT_TO_TITLE,
    PASSPORT_ACTION_EXIT_GAME,
} PASSPORT_ACTION;

typedef struct {
    PASSPORT_ACTION select_action;
    union {
        int32_t select_level;
        SAVEGAME_SLOT_REF select_save_slot;
    };
    bool ask_for_save;
} PASSPORT;

extern PASSPORT g_Passport; // TODO: meh

void Option_Passport_Control(INVENTORY_ITEM *inv_item, bool is_busy);
void Option_Passport_Draw(INVENTORY_ITEM *inv_item);
void Option_Passport_Close(void);

#pragma once

#include <trx/game/game_flow/types.h>
#include <trx/game/savegame/types.h>

// Loading a saved game is divided into two phases. First, the game reads the
// savegame file contents to look for the level number. The rest of the save
// data is stored in a special buffer. Then the engine continues to execute the
// normal game flow and loads the specified level. Second phase occurs after
// everything finishes loading, e.g. items, creatures, triggers etc., and is
// what actually sets Lara's health, creatures status, triggers, inventory etc.

void Savegame_Init(void);
void Savegame_Shutdown(void);
bool Savegame_IsInitialised(void);
void Savegame_ScanSavedGames(void);

SAVEGAME_VERSION Savegame_GetInitialVersion(void);
void Savegame_SetInitialVersion(SAVEGAME_VERSION version);
int32_t Savegame_GetCounter(void);
int32_t Savegame_GetTotalCount(void);
int32_t Savegame_GetLevelNumber(SAVEGAME_SLOT_REF slot);
bool Savegame_IsSlotFree(SAVEGAME_SLOT_REF slot);
bool Savegame_RestartAvailable(SAVEGAME_SLOT_REF slot);

// Remembers the slot used when the player starts a loaded game.
// Persists across level reloads.
void Savegame_BindSlot(SAVEGAME_SLOT_REF slot);

// Removes the binding of the current slot. Used when the player exits to
// title, issues a command like `/play` etc.
void Savegame_UnbindSlot(void);

// Returns the currently bound slot. If there is none, returns an invalid slot.
SAVEGAME_SLOT_REF Savegame_GetBoundSlot(void);

// Returns the most recently created save slot number. If there is none,
// returns an invalid slot.
SAVEGAME_SLOT_REF Savegame_GetMostRecentlyCreatedSlot(void);

// Returns the most recently used slot save number. If there is none, returns
// an invalid slot.
SAVEGAME_SLOT_REF Savegame_GetMostRecentlyUsedSlot(void);

void Savegame_ProcessItemsBeforeLoad(void);
void Savegame_ProcessItemsBeforeSave(void);
bool Savegame_Load(SAVEGAME_SLOT_REF slot);
bool Savegame_Save(SAVEGAME_SLOT_REF slot);
bool Savegame_UpdateDeathCounters(SAVEGAME_SLOT_REF slot, int32_t death_count);
bool Savegame_LoadOnlyResumeInfo(SAVEGAME_SLOT_REF slot);

void Savegame_InitCurrentInfo(void);
void Savegame_SetCurrentInfo(int32_t current_slot, int32_t src_slot);
RESUME_INFO *Savegame_GetCurrentInfo(const GF_LEVEL *level);
const SAVEGAME_INFO *Savegame_GetSavegameInfo(SAVEGAME_SLOT_REF slot);
void Savegame_ResetCurrentInfo(const GF_LEVEL *level);
void Savegame_CarryCurrentInfoToNextLevel(
    const GF_LEVEL *src_level, const GF_LEVEL *dst_level);
void Savegame_PersistGameToCurrentInfo(const GF_LEVEL *level);
void Savegame_ApplyLogicToCurrentInfo(const GF_LEVEL *level);

int32_t Savegame_GetSlotCount(SAVEGAME_SLOT_POOL pool);
SAVEGAME_SLOT_REF Savegame_GetNextQuickSlot(void);
int32_t Savegame_GetQuickVisualCount(void);
SAVEGAME_SLOT_REF Savegame_QuickFromVisualIndex(int32_t visual_index);
int32_t Savegame_QuickToVisualIndex(SAVEGAME_SLOT_REF slot);
bool Savegame_IsValidSlotRef(SAVEGAME_SLOT_REF slot);
SAVEGAME_SLOT_REF Savegame_NormalSlot(int32_t index);
SAVEGAME_SLOT_REF Savegame_QuickSlot(int32_t index);
SAVEGAME_SLOT_REF Savegame_InvalidSlot(void);
int32_t Savegame_SlotToParam(SAVEGAME_SLOT_REF slot);
SAVEGAME_SLOT_REF Savegame_SlotFromParam(int32_t param);

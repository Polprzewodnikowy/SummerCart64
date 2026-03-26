#pragma once

#include <trx/game/game_flow/types.h>
#include <trx/game/savegame/types.h>

void GF_Init(void);
void GF_Shutdown(void);

void GF_OverrideCommand(GF_COMMAND action);
GF_COMMAND GF_GetOverrideCommand(void);

GF_LEVEL_TABLE_TYPE GF_GetLevelTableType(const GF_LEVEL_TYPE level_type);
const GF_LEVEL_TABLE *GF_GetLevelTable(GF_LEVEL_TABLE_TYPE level_type);
int32_t GF_GetLevelCount(GF_LEVEL_TABLE_TYPE level_table_type);

const GF_LEVEL *GF_GetCurrentLevel(void);
const GF_LEVEL *GF_GetTitleLevel(void);
const GF_LEVEL *GF_GetGymLevel(void);
const GF_LEVEL *GF_GetFirstLevel(void);
const GF_LEVEL *GF_GetLastLevel(void);
const GF_LEVEL *GF_GetLevel(GF_LEVEL_TABLE_TYPE level_table_type, int32_t num);
const GF_LEVEL *GF_GetLevelAfter(const GF_LEVEL *level);
const GF_LEVEL *GF_GetLevelBefore(const GF_LEVEL *level);

// Get human-readable number (as opposed to index), starting with 1.
// Returns 0 for Gym levels and -1 for unknown levels.
int32_t GF_GetLevelOrdinalNumber(
    GF_LEVEL_TABLE_TYPE level_table_type, const GF_LEVEL *level);

// Get the level based on the human-readable number - opposite of
// GF_GetLevelOrdinalNumber().
GF_LEVEL *GF_GetLevelByOrdinalNumber(
    GF_LEVEL_TABLE_TYPE level_table_type, int32_t level_num);

void GF_SetCurrentLevel(const GF_LEVEL *level);
void GF_SetLevelTitle(GF_LEVEL *level, const char *title);

// Returns true if any story cutscenes or FMVs occur before gameplay in any
// main level up to the level in the specified save slot.
bool GF_HasAvailableStory(SAVEGAME_SLOT_REF slot);

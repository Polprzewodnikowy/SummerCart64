#pragma once

#include <trx/game/game/enum.h>
#include <trx/game/game_flow.h>

extern int32_t g_OverlayFlag;

// Sets the game's playing state, which in turn toggles random draw lock, and
// certain overlay displays/animations, such as bars and pickups.
void Game_SetIsPlaying(bool is_playing);

// Returns true if the game is in a playing state - i.e. not suspended, such as
// during pause, photo mode, or while the inventory is open.
bool Game_IsPlaying(void);

const GF_LEVEL *Game_GetCurrentLevel(void);
void Game_SetCurrentLevel(const GF_LEVEL *level);

bool Game_IsInGym(void);

// Returns true if an FMV is not playing and the current level is not the title.
bool Game_IsLoaded(void);

// Returns true if an FMV is not playing, if the level type is neither the
// title, a demo or a cutscene, and if Lara is loaded and controllable.
bool Game_IsPlayable(void);

GAME_BONUS_FLAG Game_GetBonusFlag(void);
void Game_SetBonusFlag(GAME_BONUS_FLAG flag);
bool Game_IsBonusFlagSet(GAME_BONUS_FLAG flag);

void Game_SetIsLevelComplete(bool is_complete);
bool Game_IsLevelComplete(void);

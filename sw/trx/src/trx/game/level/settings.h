#pragma once

#include <trx/core/colors.h>
#include <trx/game/game_flow/types.h>

RGB_888 Level_GetWaterColor(void);
RGBA_8888 Level_GetFogColor(void);
float Level_GetFogStart(void);
float Level_GetFogEnd(void);
bool Level_HasColdWater(void);
GF_DEATH_TILE Level_GetDeathTile(void);

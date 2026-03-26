#pragma once

#include <trx/game/level/context.h>

void Level_Finalize_LoadRooms(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadTextures(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadTexturePages(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadPalettes(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadAnimCommands(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadAnimFrames(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadObjectsAndItems(LEVEL_CONTEXT *ctx);
void Level_Finalize_LoadWalkables(LEVEL_CONTEXT *ctx);

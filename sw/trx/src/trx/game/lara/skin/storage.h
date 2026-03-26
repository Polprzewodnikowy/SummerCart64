#pragma once

#include <trx/game/lara/skin/types.h>

void Lara_Skin_LoadFromFile(const char *path);
void Lara_Skin_Shutdown(void);
int32_t Lara_Skin_GetOutfitCount(void);
bool Lara_Skin_IsOutfitAvailable(LARA_SKIN_TYPE skin_type);
const LARA_SKIN_OUTFIT *Lara_Skin_GetOutfit(LARA_SKIN_TYPE skin_type);
const char *Lara_Skin_GetOutfitName(LARA_SKIN_TYPE skin_type);
LARA_SKIN_TYPE Lara_Skin_FindOutfitByName(const char *name);
LARA_SKIN_TYPE Lara_Skin_GetDefaultType(void);
int32_t Lara_Skin_GetExtraMeshOffset(LARA_SKIN_EXTRA_MESH mesh);

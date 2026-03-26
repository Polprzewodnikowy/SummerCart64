#pragma once

#include <trx/game/lara/enum.h>
#include <trx/game/lara/skin/types.h>
#include <trx/game/sound/ids.h>

void Lara_Skin_Initialise(void);
void Lara_Skin_ApplyOutfitFromConfig(void);
void Lara_Skin_CycleOutfit(int32_t dir);
LARA_SKIN_TYPE Lara_Skin_GetType(void);
bool Lara_Skin_IsDefaultType(void);
void Lara_Skin_SetType(LARA_SKIN_TYPE skin_type);
void Lara_Skin_ApplyOutfit(void);

void Lara_Skin_SetCombatFace(bool enabled);
void Lara_Skin_SwapAllExtra(LARA_EXTRA_STATE state);
void Lara_Skin_SwapSingleExtra(LARA_MESH mesh, LARA_EXTRA_STATE state);
const ANIM_BONE *Lara_Skin_GetBoneBase(void);

bool Lara_Skin_IsBraidSupported(void);
XYZ_32 Lara_Skin_GetBraidOffset(void);
int32_t Lara_Skin_GetBraidMeshIdx(void);
const ANIM_BONE *Lara_Skin_GetBraidBoneBase(void);

bool Lara_Skin_AreHolstersVisible(void);
void Lara_Skin_SetHolstersVisible(bool visible);
void Lara_Skin_ClearEquipment(LARA_MESH mesh);
void Lara_Skin_SetGunEquipment(LARA_MESH mesh, LARA_GUN_TYPE gun_type);
void Lara_Skin_SetExtraEquipment(
    LARA_MESH mesh, LARA_SKIN_EXTRA_MESH extra_mesh);
const LARA_SKIN_EQUIPMENT *Lara_Skin_GetEquipment(LARA_MESH mesh);

SAMPLE_ID Lara_Skin_GetAnimSFX(SAMPLE_ID sample_id);

void Lara_Skin_ExtractLegacyEquipment(const OBJECT_MESH **meshes);

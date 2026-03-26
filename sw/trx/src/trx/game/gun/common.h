#pragma once

#include <trx/game/lara/types.h>

void Gun_InitialiseNewWeapon(void);

void Gun_SetLaraBackMesh(LARA_GUN_TYPE weapon_type);
void Gun_SetLaraHandLMesh(LARA_GUN_TYPE weapon_type);
void Gun_SetLaraHandRMesh(LARA_GUN_TYPE weapon_type);
void Gun_SetLaraHolsterLMesh(LARA_GUN_TYPE weapon_type);
void Gun_SetLaraHolsterRMesh(LARA_GUN_TYPE weapon_type);

// TODO: make this a struct
OBJECT_ID Gun_GetLaraAnim(LARA_GUN_TYPE gun_type);
OBJECT_ID Gun_GetWeaponAnim(LARA_GUN_TYPE gun_type);
LARA_GUN_TYPE Gun_GetType(OBJECT_ID obj_id);
OBJECT_ID Gun_GetGunObject(LARA_GUN_TYPE gun_type);
OBJECT_ID Gun_GetAmmoObject(LARA_GUN_TYPE gun_type);
int32_t Gun_GetAmmoQuantity(LARA_GUN_TYPE gun_type);
int32_t Gun_GetAmmoShellCount(LARA_GUN_TYPE gun_type);
int32_t Gun_GetAmmoClipCount(LARA_GUN_TYPE gun_type);
AMMO_INFO *Gun_GetAmmoInfo(LARA_GUN_TYPE gun_type);
bool Gun_IsRifleType(LARA_GUN_TYPE gun_type);

void Gun_AddDynamicLight(void);

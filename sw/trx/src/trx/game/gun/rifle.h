#pragma once

#include <trx/game/lara/types.h>

void Gun_Rifle_Control(LARA_GUN_TYPE weapon_type);
void Gun_Rifle_Draw(LARA_GUN_TYPE weapon_type);
void Gun_Rifle_Undraw(LARA_GUN_TYPE weapon_type);

void Gun_Rifle_DrawMeshes(LARA_GUN_TYPE weapon_type);
void Gun_Rifle_UndrawMeshes(LARA_GUN_TYPE weapon_type);
void Gun_Rifle_EnsureReady(LARA_GUN_TYPE weapon_type);

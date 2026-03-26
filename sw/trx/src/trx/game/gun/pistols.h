#pragma once

#include <trx/game/lara/types.h>

void Gun_Pistols_Control(LARA_GUN_TYPE weapon_type);

void Gun_Pistols_Draw(LARA_GUN_TYPE weapon_type);
void Gun_Pistols_Undraw(LARA_GUN_TYPE weapon_type);
void Gun_Pistols_Ready(LARA_GUN_TYPE weapon_type);

void Gun_Pistols_DrawMeshes(LARA_GUN_TYPE weapon_type);
void Gun_Pistols_UndrawMeshLeft(LARA_GUN_TYPE weapon_type);
void Gun_Pistols_UndrawMeshRight(LARA_GUN_TYPE weapon_type);

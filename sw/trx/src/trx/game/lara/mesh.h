#pragma once

#include <trx/core/colors.h>
#include <trx/game/game_flow.h>
#include <trx/game/lara/enum.h>
#include <trx/game/objects.h>

void Lara_Mesh_Initialise(const GF_LEVEL *level);
void Lara_Mesh_SwapSingle(LARA_MESH mesh, OBJECT_ID obj_id);
void Lara_Mesh_SwapAll(OBJECT_ID obj_id);
void Lara_Mesh_Set(LARA_MESH mesh, OBJECT_MESH *mesh_ptr);
OBJECT_MESH *Lara_Mesh_Get(LARA_MESH mesh);
RGB_F Lara_GetMeshTint(GAME_VECTOR pos);
int32_t Lara_GetMeshIndex(const ITEM *item, int32_t mesh_idx);

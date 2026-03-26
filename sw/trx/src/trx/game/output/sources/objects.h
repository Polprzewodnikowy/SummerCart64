#pragma once

#include <trx/game/objects/types.h>
#include <trx/game/output/mesh_batcher/batcher.h>
#include <trx/game/output/scene_source.h>

void OutputSource_Objects_Init(MESH_BATCHER *batcher);
void OutputSource_Objects_Shutdown(void);
void OutputSource_Objects_ObserveLevelLoad(void);
void OutputSource_Objects_ObserveLevelUnload(void);
void OutputSource_Objects_ObserveObjectMeshSwap(
    int32_t mesh_idx_1, int32_t mesh_idx_2);
void OutputSource_Objects_ObserveObjectMeshUpdate(int32_t mesh_idx);

void OutputSource_Objects_StageSkyboxMesh(
    const OBJECT_MESH *mesh, int16_t shade);
void OutputSource_Objects_StageObjectMesh(const OBJECT_MESH *mesh);

const SCENE_SOURCE *OutputSource_Objects_GetSource(void);

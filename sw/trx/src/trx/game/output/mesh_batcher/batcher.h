#pragma once

#include <trx/game/output/mesh_batcher/mesh.h>
#include <trx/game/output/scene_source.h>
#include <trx/game/output/uniforms.h>
#include <trx/game/rooms/types.h>
#include <trx/game/viewport.h>

#include <stdint.h>

typedef struct MESH_INSTANCE {
    OUTPUT_MESH *mesh;

    // TODO: use gl_InstanceID some day for this
    // and glMultiDrawArraysIndirect
    MATRIX cwmatrix;
    MATRIX wmatrix;
    const ROOM *room;
    RGB_F tint;
    bool wibble;
    int32_t water_effect;

    OUTPUT_LIGHT_INFO light_info;

    bool enable_scissor;
    bool disable_z_writes;
    float depth_adjust;
    VIEWPORT_RECT scissor;
} MESH_INSTANCE;

typedef struct MESH_BATCHER MESH_BATCHER;

MESH_BATCHER *MeshBatcher_Create(void);
void MeshBatcher_Destroy(struct MESH_BATCHER *batcher);
void MeshBatcher_AddMesh(struct MESH_BATCHER *batcher, OUTPUT_MESH *mesh);
void MeshBatcher_RemoveMesh(struct MESH_BATCHER *batcher, OUTPUT_MESH *mesh);
void MeshBatcher_Seal(MESH_BATCHER *batcher);

const SCENE_SOURCE *MeshBatcher_AsSource(const struct MESH_BATCHER *batcher);

void MeshBatcher_Stage(
    struct MESH_BATCHER *batcher, const MESH_INSTANCE *inst, SCENE_PASS pass);
void MeshBatcher_UpdateMeshGeometry(
    const struct MESH_BATCHER *batcher, const OUTPUT_MESH *mesh);

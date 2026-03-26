#pragma once

#include <trx/game/output/mesh_batcher/batcher.h>

void OutputSource_Shadows_Init(MESH_BATCHER *batcher);
void OutputSource_Shadows_Shutdown(void);

void OutputSource_Shadows_StageShadow(void);

#pragma once

#include <trx/game/objects/types.h>
#include <trx/game/output/shaders/mesh.h>
#include <trx/game/output/shaders/ui.h>
#include <trx/game/output/uniforms.h>
#include <trx/game/rooms.h>
#include <trx/game/viewport.h>

void Output_Init(void);
void Output_Shutdown(void);
bool Output_IsHeadless(void);

const OUTPUT_UNIFORMS *Output_GetUniforms(void);
OUTPUT_MESH_SHADER *Output_GetMeshShader(void);
OUTPUT_UI_SHADER *Output_GetUIShader(void);

void Output_BeginScene(void);
void Output_EndScene(void);
void Output_Flush(void);
void Output_FlipScreen(void);

void Output_SwitchViewport(VIEWPORT_SPACE space);

void Output_ApplyRenderSettings(void);
void Output_ApplyLevelSettings(void);

void Output_DispatchLevelLoad(void);
void Output_DispatchLevelUnload(void);
void Output_DispatchRoomFlip(const ROOM *room);
void Output_DispatchObjectMeshUpdate(int32_t mesh_idx);
void Output_DispatchObjectMeshSwap(int32_t mesh_idx_0, int32_t mesh_idx_1);

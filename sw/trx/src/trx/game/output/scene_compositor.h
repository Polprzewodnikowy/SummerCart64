#pragma once

#include <trx/game/output/scene_source.h>

void SceneCompositor_Init(void);
void SceneCompositor_Shutdown(void);
void SceneCompositor_AddSource(const SCENE_SOURCE *source);
void SceneCompositor_BeginScene(void);
void SceneCompositor_EndScene(void);
void SceneCompositor_Flush(void);
void SceneCompositor_AnimateTextures(void);

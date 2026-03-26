#include <trx/game/output/common.h>

#include <trx/config.h>
#include <trx/game/level.h>
#include <trx/game/output/func.h>
#include <trx/game/output/lights.h>
#include <trx/game/output/mesh_batcher/batcher.h>
#include <trx/game/output/scene_compositor.h>
#include <trx/game/output/sources/lightnings.h>
#include <trx/game/output/sources/misc.h>
#include <trx/game/output/sources/objects.h>
#include <trx/game/output/sources/overlay.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/output/sources/rooms.h>
#include <trx/game/output/sources/rooms_debug.h>
#include <trx/game/output/sources/shadows.h>
#include <trx/game/output/sources/sprites.h>
#include <trx/game/output/sources/ui.h>
#include <trx/game/output/state.h>
#include <trx/game/output/textures.h>
#include <trx/game/shell.h>
#include <trx/gl/context.h>
#include <trx/gl/track.h>

static MESH_BATCHER *m_Batcher = nullptr;
static OUTPUT_UNIFORMS *m_Uniforms = nullptr;
static OUTPUT_MESH_SHADER *m_ShaderWorld = nullptr;
static OUTPUT_UI_SHADER *m_ShaderUI = nullptr;

void Output_Init(void)
{
    SceneCompositor_Init();
    Output_Textures_Init();

    m_Uniforms = Output_Uniforms_Create();
    m_ShaderWorld = Output_MeshShader_Create();
    m_ShaderUI = Output_UIShader_Create();
    m_Batcher = MeshBatcher_Create();
    SceneCompositor_AddSource(MeshBatcher_AsSource(m_Batcher));
    OutputSource_Rooms_Init(m_Batcher);
    OutputSource_RoomsDebug_Init();
    OutputSource_Objects_Init(m_Batcher);
    OutputSource_Sprites_Init(m_Batcher);
    OutputSource_Lightnings_Init();
    OutputSource_PolyFX_Init();
    OutputSource_Shadows_Init(m_Batcher);
    OutputSource_Misc_Init();
    OutputSource_Overlay_Init();

    Output_Lights_Init();
    OutputSource_UI_Init();

    Output_ApplyRenderSettings();
}

void Output_Shutdown(void)
{
    SceneCompositor_Shutdown();
    OutputSource_Rooms_Shutdown();
    OutputSource_RoomsDebug_Shutdown();
    OutputSource_Objects_Shutdown();
    OutputSource_Sprites_Shutdown();
    OutputSource_Lightnings_Shutdown();
    OutputSource_PolyFX_Shutdown();
    OutputSource_Shadows_Shutdown();
    OutputSource_Misc_Shutdown();
    OutputSource_Overlay_Shutdown();
    OutputSource_UI_Shutdown();

    if (m_ShaderWorld != nullptr) {
        Output_MeshShader_Free(m_ShaderWorld);
        m_ShaderWorld = nullptr;
    }
    if (m_ShaderUI != nullptr) {
        Output_UIShader_Free(m_ShaderUI);
        m_ShaderUI = nullptr;
    }
    if (m_Uniforms != nullptr) {
        Output_Uniforms_Free(m_Uniforms);
        m_Uniforms = nullptr;
    }
    if (m_Batcher != nullptr) {
        MeshBatcher_Destroy(m_Batcher);
        m_Batcher = nullptr;
    }

    Output_Textures_Shutdown();
    Output_Lights_Shutdown();
}

bool Output_IsHeadless(void)
{
    return Shell_GetArgs()->headless;
}

const OUTPUT_UNIFORMS *Output_GetUniforms(void)
{
    return m_Uniforms;
}

OUTPUT_MESH_SHADER *Output_GetMeshShader(void)
{
    return m_ShaderWorld;
}

OUTPUT_UI_SHADER *Output_GetUIShader(void)
{
    return m_ShaderUI;
}

void Output_BeginScene(void)
{
    Output_ApplyFOV();
    TRX_GL_Context_Clear();
    TRX_GL_Track_Reset();
    TRX_GL_Context_SetWireframeMode(g_Config.rendering.enable_wireframe);
    SceneCompositor_BeginScene();
}

void Output_EndScene(void)
{
    SceneCompositor_EndScene();
}

void Output_Flush(void)
{
    SceneCompositor_Flush();
}

void Output_FlipScreen(void)
{
    TRX_GL_Context_SwapBuffers();
}

void Output_SwitchViewport(const VIEWPORT_SPACE space)
{
    if (space == VIEWPORT_GAME) {
        TRX_GL_Renderer_BindGeometryFbo();
    } else if (space == VIEWPORT_UI) {
        TRX_GL_Renderer_BindUiFbo();
    }
    TRX_GL_Context_SwitchToViewport(space);
    TRX_GL_Context_Clear();
    glClear(GL_DEPTH_BUFFER_BIT);
}

void Output_ApplyRenderSettings(void)
{
    Output_Textures_ApplyRenderSettings();
    Output_ApplyLevelSettings();

    if (m_ShaderWorld == nullptr) {
        return;
    }

    TRX_GL_Context_SetVSync(g_Config.rendering.enable_vsync);
    TRX_GL_Context_SetDisplayFilter(g_Config.rendering.upscaling_filter);
    TRX_GL_Context_SetWireframeMode(g_Config.rendering.enable_wireframe);
    TRX_GL_Context_SetLineWidth(g_Config.rendering.wireframe_width);
}

void Output_ApplyLevelSettings(void)
{
    Output_SetWaterColor(Level_GetWaterColor());
    Output_SetFogColor(Level_GetFogColor());
    Output_SetFogStart(Level_GetFogStart() * WALL_L);
    Output_SetFogEnd(Level_GetFogEnd() * WALL_L);
}

void Output_DispatchLevelLoad(void)
{
    Output_Textures_ObserveLevelLoad();
    Output_Lights_ObserveLevelLoad();

    OutputSource_Objects_ObserveLevelLoad();
    OutputSource_Rooms_ObserveLevelLoad();
    OutputSource_RoomsDebug_ObserveLevelLoad();
    OutputSource_Sprites_ObserveLevelLoad();

    MeshBatcher_Seal(m_Batcher);

    Output_ApplyLevelSettings();
}

void Output_DispatchLevelUnload(void)
{
    OutputSource_Objects_ObserveLevelUnload();
    OutputSource_Rooms_ObserveLevelUnload();
    OutputSource_RoomsDebug_ObserveLevelUnload();
    OutputSource_Sprites_ObserveLevelUnload();
}

void Output_DispatchRoomFlip(const ROOM *room)
{
    OutputSource_Rooms_ObserveRoomFlip(room);
    OutputSource_RoomsDebug_ObserveRoomFlip(room);
}

void Output_DispatchObjectMeshSwap(
    const int32_t mesh_idx_1, const int32_t mesh_idx_2)
{
    OutputSource_Objects_ObserveObjectMeshSwap(mesh_idx_1, mesh_idx_2);
}

void Output_DispatchObjectMeshUpdate(const int32_t mesh_idx)
{
    OutputSource_Objects_ObserveObjectMeshUpdate(mesh_idx);
}

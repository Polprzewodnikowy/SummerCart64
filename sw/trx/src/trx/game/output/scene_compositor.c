#include <trx/game/output/scene_compositor.h>

#include <trx/config.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/output.h>
#include <trx/game/output/shaders/mesh.h>
#include <trx/game/output/shaders/ui.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/uniforms.h>
#include <trx/game/shell.h>
#include <trx/gl/context.h>
#include <trx/gl/utils.h>

#define M_PROCESS_SOURCES(p, func, ...)                                        \
    do {                                                                       \
        for (int32_t i = 0; i < p->sources->count; i++) {                      \
            const SCENE_SOURCE *const source =                                 \
                *(SCENE_SOURCE **)Vector_Get(p->sources, i);                   \
            if (source->func != nullptr) {                                     \
                source->func(source, ##__VA_ARGS__);                           \
            }                                                                  \
        }                                                                      \
    } while (0)

typedef struct {
    VECTOR *sources;
    GLuint sampler_id;
} M_PRIV;

static M_PRIV m_Priv = {};

static void M_SetSamplerFilter(
    const GLuint sampler, const TEXTURE_FILTER filter)
{
    const GLenum gl_filter =
        filter == TEXTURE_FILTER_BILINEAR ? GL_LINEAR : GL_NEAREST;
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, gl_filter);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, gl_filter);
}

static void M_BindTextures(const M_PRIV *const p)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Output_Textures_GetAtlasTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, Output_Textures_GetEnvMapTexture());
}

static void M_SetupScene(const M_PRIV *const p)
{
    Output_MeshShader_Bind(Output_GetMeshShader());
    Output_Uniforms_UploadViewMatrix(Output_GetUniforms(), &g_ViewMatrix);
    glEnable(GL_BLEND);
    glBlendFunc(
        GL_ONE,
        g_Config.rendering.enable_wireframe ? GL_ZERO : GL_ONE_MINUS_SRC_ALPHA);
    M_SetSamplerFilter(p->sampler_id, g_Config.rendering.texture_filter);
}

static void M_SetupUI(const M_PRIV *const p)
{
    Output_UIShader_Bind(Output_GetUIShader());
    Output_Uniforms_UploadOrthoMatrix(Output_GetUniforms());
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    M_SetSamplerFilter(p->sampler_id, g_Config.rendering.ui_filter);
    glClear(GL_DEPTH_BUFFER_BIT);
}

static void M_RenderSourcePass(const M_PRIV *const p, const SCENE_PASS pass)
{
    for (int32_t i = 0; i < p->sources->count; i++) {
        const SCENE_SOURCE *const source =
            *(SCENE_SOURCE **)Vector_Get(p->sources, i);
        if (source->is_dirty != nullptr && source->is_dirty(source, pass)) {
            ASSERT(source->render_pass != nullptr);
            source->render_pass(source, pass);
        }
    }
}

static bool M_IsSourceDirty(const M_PRIV *const p, const SCENE_PASS pass)
{
    for (int32_t i = 0; i < p->sources->count; i++) {
        const SCENE_SOURCE *const source =
            *(SCENE_SOURCE **)Vector_Get(p->sources, i);
        if (source->is_dirty != nullptr && source->is_dirty(source, pass)) {
            return true;
        }
    }
    return false;
}

static bool M_IsAnySourceDirty(const M_PRIV *const p)
{
    for (SCENE_PASS pass = 0; pass < SCENE_PASS_COUNT; pass++) {
        if (M_IsSourceDirty(p, pass)) {
            return true;
        }
    }
    return false;
}

static void M_PrepareScene(const M_PRIV *const p)
{
#ifndef __APPLE__
    glLineWidth(g_Config.rendering.wireframe_width);
    TRX_GL_CheckError();
#endif

    glBindSampler(0, p->sampler_id);
    glSamplerParameterf(
        p->sampler_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
        g_Config.rendering.anisotropy_filter);

    Output_Uniforms_UploadGeneral(Output_GetUniforms());
    Output_Uniforms_UploadRoomLights(Output_GetUniforms(), nullptr);
    Output_SetCurrentRoom(nullptr);
}

static void M_RenderScenePasses(const M_PRIV *const p)
{
    if (!M_IsAnySourceDirty(p)) {
        return;
    }

    M_BindTextures(p);
    M_SetupScene(p);

    glDisable(GL_DEPTH_TEST);
    if (M_IsSourceDirty(p, SCENE_PASS_BACKGROUND)) {
        M_RenderSourcePass(p, SCENE_PASS_BACKGROUND);
    }

    OUTPUT_MESH_SHADER *const shader = Output_GetMeshShader();
    Output_MeshShader_Bind(shader);

    glPolygonMode(
        GL_FRONT_AND_BACK,
        g_Config.rendering.enable_wireframe ? GL_LINE : GL_FILL);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);

    if (M_IsSourceDirty(p, SCENE_PASS_OPAQUE)
        || M_IsSourceDirty(p, SCENE_PASS_TRANSPARENT)
        || M_IsSourceDirty(p, SCENE_PASS_BLEND_SUB)
        || M_IsSourceDirty(p, SCENE_PASS_BLEND_ADD)) {
        glEnable(GL_CULL_FACE);
        Output_MeshShader_UploadAlphaDiscard(shader, true);
        M_RenderSourcePass(p, SCENE_PASS_OPAQUE);
        Output_MeshShader_UploadAlphaDiscard(shader, false);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        M_RenderSourcePass(p, SCENE_PASS_TRANSPARENT);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        M_RenderSourcePass(p, SCENE_PASS_BLEND_SUB);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        M_RenderSourcePass(p, SCENE_PASS_BLEND_ADD);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
    }

    if (M_IsSourceDirty(p, SCENE_PASS_OVERLAY_PRE_UI)) {
        M_RenderSourcePass(p, SCENE_PASS_OVERLAY_PRE_UI);
    }

    if (M_IsSourceDirty(p, SCENE_PASS_UI)) {
        M_SetupUI(p);
        M_RenderSourcePass(p, SCENE_PASS_UI);
    }

    if (M_IsSourceDirty(p, SCENE_PASS_OVERLAY_POST_UI)) {
        M_RenderSourcePass(p, SCENE_PASS_OVERLAY_POST_UI);
    }
}

void SceneCompositor_Init(void)
{
    M_PRIV *const p = &m_Priv;
    p->sources = Vector_Create(sizeof(SCENE_SOURCE *));
    glGenSamplers(1, &p->sampler_id);
    glSamplerParameteri(p->sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(p->sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(p->sampler_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(p->sampler_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    TRX_GL_CheckError();
}

void SceneCompositor_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->sources != nullptr) {
        Vector_Free(p->sources);
        p->sources = nullptr;
    }
    if (p->sampler_id != 0) {
        glDeleteSamplers(1, &p->sampler_id);
        p->sampler_id = 0;
    }
}

bool M_IsActive(void)
{
    return !Output_IsHeadless() || Shell_GetArgs()->debug_render_performance
        || TRX_GL_Context_GetScheduledScreenshotPath() != nullptr;
}

void SceneCompositor_BeginScene(void)
{
    M_PRIV *const p = &m_Priv;
    if (!M_IsActive()) {
        return;
    }
    M_PrepareScene(p);
    M_PROCESS_SOURCES(p, render_begin);
}

void SceneCompositor_Flush(void)
{
    M_PRIV *const p = &m_Priv;
    if (!M_IsActive()) {
        return;
    }
    M_RenderScenePasses(p);
    M_PROCESS_SOURCES(p, render_end);
    M_PROCESS_SOURCES(p, render_begin);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void SceneCompositor_EndScene(void)
{
    M_PRIV *const p = &m_Priv;
    if (!M_IsActive()) {
        return;
    }
    M_RenderScenePasses(p);
    M_PROCESS_SOURCES(p, render_end);
}

void SceneCompositor_AnimateTextures(void)
{
    M_PRIV *const p = &m_Priv;
    M_PROCESS_SOURCES(p, animate_textures);
}

void SceneCompositor_AddSource(const SCENE_SOURCE *const source)
{
    M_PRIV *const p = &m_Priv;
    Vector_Add(p->sources, &source);
}

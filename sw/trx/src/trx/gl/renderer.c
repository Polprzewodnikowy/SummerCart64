#include <trx/gl/renderer.h>

#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/gl/buffer.h>
#include <trx/gl/context.h>
#include <trx/gl/enum.h>
#include <trx/gl/fbo.h>
#include <trx/gl/program.h>
#include <trx/gl/sampler.h>
#include <trx/gl/screenshot.h>
#include <trx/gl/texture.h>
#include <trx/gl/utils.h>
#include <trx/gl/vertex_array.h>

#include <GL/glew.h>
#include <SDL2/SDL_video.h>
#include <stdint.h>

typedef struct {
    const TRX_GL_CONFIG *config;

    TRX_GL_FBO geometry_fbo;
    TRX_GL_FBO ui_fbo;

    // Full-screen quad resources for blitting FBOs to default framebuffer.
    TRX_GL_VERTEX_ARRAY vertex_array;
    TRX_GL_BUFFER buffer;
    TRX_GL_SAMPLER sampler;
    TRX_GL_PROGRAM program;
} M_CONTEXT;

static void M_Blit(const M_CONTEXT *const p, const TRX_GL_FBO *const fbo)
{
    TRX_GL_Texture_Bind(&fbo->texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    TRX_GL_CheckError();
}

static void M_UpdateFBOSizes(TRX_GL_RENDERER *renderer)
{
    M_CONTEXT *const p = renderer->priv;
    const TRX_GL_CONFIG *const config = p->config;

    VIEWPORT_RECT rect;
    rect = Viewport_GetRect(VIEWPORT_GAME);
    TRX_GL_FBO_ResizeIfNeeded(&p->geometry_fbo, rect.width, rect.height);
    rect = Viewport_GetRect(VIEWPORT_UI);
    TRX_GL_FBO_ResizeIfNeeded(&p->ui_fbo, rect.width, rect.height);
}

static void M_Render(TRX_GL_RENDERER *renderer)
{
    ASSERT(renderer != nullptr);
    M_CONTEXT *const p = renderer->priv;
    ASSERT(p != nullptr);

    const GLuint filter = p->config->display_filter == TEXTURE_FILTER_BILINEAR
        ? GL_LINEAR
        : GL_NEAREST;

    TRX_GL_FBO_Unbind();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    TRX_GL_CheckError();

    TRX_GL_Program_Bind(&p->program);
    TRX_GL_Buffer_Bind(&p->buffer);
    TRX_GL_VertexArray_Bind(&p->vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);

    TRX_GL_Sampler_Bind(&p->sampler, 0);
    TRX_GL_Sampler_Parameteri(&p->sampler, GL_TEXTURE_MAG_FILTER, filter);
    TRX_GL_Sampler_Parameteri(&p->sampler, GL_TEXTURE_MIN_FILTER, filter);

    VIEWPORT_RECT rect = Viewport_GetRect(VIEWPORT_TARGET);
    glViewport(rect.x, rect.y, rect.width, rect.height);
    TRX_GL_CheckError();

    // Composite geometry FBO (opaque)
    glDisable(GL_BLEND);
    M_Blit(p, &p->geometry_fbo);

    // Composite UI FBO (with premultiplied alpha blending)
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    M_Blit(p, &p->ui_fbo);
    glDisable(GL_BLEND);

    if (TRX_GL_Context_GetScheduledScreenshotPath() != nullptr) {
        TRX_GL_Context_SwitchToViewport(VIEWPORT_TARGET);
        TRX_GL_Screenshot_CaptureToFile(
            TRX_GL_Context_GetScheduledScreenshotPath());
        TRX_GL_Context_ClearScheduledScreenshotPath();
    }
}

static void M_SwapBuffers(TRX_GL_RENDERER *const renderer)
{
    M_CONTEXT *const p = renderer->priv;

    M_Render(renderer);
    SDL_GL_SwapWindow(TRX_GL_Context_GetWindowHandle());
    M_UpdateFBOSizes(renderer);

    TRX_GL_Context_SwitchToViewport(VIEWPORT_WINDOW);
    TRX_GL_Context_Clear();

    // Rebind geometry FBO for the next frame
    TRX_GL_Renderer_BindGeometryFbo();
    TRX_GL_Context_SwitchToViewport(VIEWPORT_GAME);
    TRX_GL_Context_Clear();
}

static void M_Init(
    TRX_GL_RENDERER *const renderer, const TRX_GL_CONFIG *const config)
{
    ASSERT(renderer != nullptr);
    renderer->priv = (M_CONTEXT *)Memory_Alloc(sizeof(M_CONTEXT));
    M_CONTEXT *const p = renderer->priv;
    ASSERT(p != nullptr);

    p->config = config;

    TRX_GL_Buffer_Init(&p->buffer, GL_ARRAY_BUFFER);
    TRX_GL_Buffer_Bind(&p->buffer);
    const GLfloat verts[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    };
    TRX_GL_Buffer_Data(&p->buffer, sizeof(verts), verts, GL_STATIC_DRAW);

    TRX_GL_VertexArray_Init(&p->vertex_array);
    TRX_GL_VertexArray_Bind(&p->vertex_array);
    TRX_GL_VertexArray_Attribute(
        &p->vertex_array, 0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    TRX_GL_Sampler_Init(&p->sampler);
    TRX_GL_Sampler_Bind(&p->sampler, 0);
    TRX_GL_Sampler_Parameteri(&p->sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    TRX_GL_Sampler_Parameteri(&p->sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    TRX_GL_Sampler_Parameteri(&p->sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    TRX_GL_Sampler_Parameteri(&p->sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    TRX_GL_Program_Init(&p->program);
    TRX_GL_Program_AttachShader(&p->program, GL_VERTEX_SHADER, "fbo.glsl");
    TRX_GL_Program_AttachShader(&p->program, GL_FRAGMENT_SHADER, "fbo.glsl");
    TRX_GL_Program_FragmentData(&p->program, "outColor");
    TRX_GL_Program_Link(&p->program);
    TRX_GL_Program_Bind(&p->program);
    TRX_GL_Program_Uniform1i(
        &p->program, TRX_GL_Program_UniformLocation(&p->program, "uTex0"), 0);

    VIEWPORT_RECT rect;
    rect = Viewport_GetRect(VIEWPORT_GAME);
    TRX_GL_FBO_Init(
        &p->geometry_fbo, rect.width, rect.height, GL_RGBA8, GL_RGBA, true);

    rect = Viewport_GetRect(VIEWPORT_UI);
    TRX_GL_FBO_Init(
        &p->ui_fbo, rect.width, rect.height, GL_RGBA8, GL_RGBA, false);
}

static void M_Shutdown(TRX_GL_RENDERER *renderer)
{
    LOG_INFO("");

    ASSERT(renderer != nullptr);
    M_CONTEXT *const p = renderer->priv;
    ASSERT(p != nullptr);

    TRX_GL_FBO_Close(&p->geometry_fbo);
    TRX_GL_FBO_Close(&p->ui_fbo);
    TRX_GL_Program_Close(&p->program);
    TRX_GL_Sampler_Close(&p->sampler);
    TRX_GL_Buffer_Close(&p->buffer);
    TRX_GL_VertexArray_Close(&p->vertex_array);

    Memory_FreePointer(&renderer->priv);
}

TRX_GL_RENDERER g_TRX_GL_Renderer = {
    .swap_buffers = &M_SwapBuffers,
    .init = &M_Init,
    .shutdown = &M_Shutdown,
};

void TRX_GL_Renderer_BindGeometryFbo(void)
{
    M_CONTEXT *const p = (M_CONTEXT *)g_TRX_GL_Renderer.priv;
    TRX_GL_FBO_Bind(&p->geometry_fbo);
}

void TRX_GL_Renderer_BindUiFbo(void)
{
    M_CONTEXT *const p = (M_CONTEXT *)g_TRX_GL_Renderer.priv;
    TRX_GL_FBO_Bind(&p->ui_fbo);
}

/*
 * gl_renderer.h — Host-side OpenGL renderer for T3D HLE
 *
 * Creates a headless EGL/OSMesa context and renders T3D geometry
 * using modern OpenGL (3.2 core profile).
 *
 * Used in Tier 2 (shadow) and Tier 3 (replace) modes.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "t3d_hle.h"

/* N64 framebuffer dimensions */
#define T3D_GL_WIDTH  320
#define T3D_GL_HEIGHT 240

/* Renderer state (opaque to callers) */
typedef struct {
    /* EGL/OSMesa context handles */
    void *egl_display;
    void *egl_context;
    void *egl_surface;
    void *osmesa_ctx;
    uint8_t *osmesa_buffer;

    /* GL objects */
    uint32_t vao;
    uint32_t vbo;
    uint32_t ebo;
    uint32_t fbo;
    uint32_t color_tex;
    uint32_t depth_rb;
    uint32_t shader_program;

    /* Shader uniforms */
    int32_t u_mvp;
    int32_t u_use_texture;
    int32_t u_fog_enabled;
    int32_t u_fog_color;
    int32_t u_fog_near;
    int32_t u_fog_far;

    /* State */
    bool initialized;
    bool context_valid;

    /* Read-back buffer (RGBA8888, 320x240) */
    uint8_t framebuffer[T3D_GL_WIDTH * T3D_GL_HEIGHT * 4];

    /* Stats */
    uint32_t draw_calls;
    uint32_t triangles;
} T3DGLRenderer;

/* Initialize the GL renderer. Returns true on success. */
bool t3d_gl_init(T3DGLRenderer *r);

/* Shutdown and free GL resources. */
void t3d_gl_shutdown(T3DGLRenderer *r);

/* Begin a new frame (clear buffers). */
void t3d_gl_frame_begin(T3DGLRenderer *r);

/* Upload vertices to the GL vertex buffer. */
void t3d_gl_upload_vertices(T3DGLRenderer *r,
                            const T3DHLEVertex *verts, int count);

/* Set the model-view-projection matrix (column-major float[16]). */
void t3d_gl_set_mvp(T3DGLRenderer *r, const float *mvp);

/* Set draw flags (depth test, backface cull, etc.) */
void t3d_gl_set_draw_state(T3DGLRenderer *r,
                           bool depth_test, bool depth_write,
                           int cull_mode); /* 0=none, 1=front, 2=back */

/* Draw triangles with vertex indices. */
void t3d_gl_draw_triangles(T3DGLRenderer *r,
                           const uint16_t *indices, int tri_count);

/* End frame, read back pixels into r->framebuffer. */
void t3d_gl_frame_end(T3DGLRenderer *r);

/* Convert framebuffer to N64 RGBA5551 format and write to RDRAM. */
void t3d_gl_composite_to_rdram(T3DGLRenderer *r,
                               uint8_t *rdram, uint32_t fb_addr,
                               int width, int height);

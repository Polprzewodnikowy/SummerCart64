/*
 * gl_renderer.c — Host OpenGL renderer for T3D HLE
 *
 * Uses OSMesa (off-screen Mesa) for headless rendering.
 * OSMesa works with llvmpipe on CPU-only servers and is much
 * faster than running parallel-rdp through lavapipe Vulkan.
 *
 * Vertex format: position(3f) + color(4ub) + uv(2f) = 28 bytes
 */

#include "gl_renderer.h"

/* Conditional compilation: only build GL renderer when we have GL headers.
 * When built standalone (unit tests), this is a stub. */
#ifdef T3D_HLE_GL_ENABLED

#define GL_GLEXT_PROTOTYPES 1
#include <GL/osmesa.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "t3d_hle.h" /* T3DHLEVertex struct */

/* ----------------------------------------------------------------
 * Shaders
 * ---------------------------------------------------------------- */

static const char *vertex_shader_src =
    "#version 120\n"
    "attribute vec3 a_position;\n"
    "attribute vec4 a_color;\n"
    "attribute vec2 a_uv;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "varying float v_fog_depth;\n"
    "void main() {\n"
    "    gl_Position = u_mvp * vec4(a_position, 1.0);\n"
    "    v_color = a_color;\n"
    "    v_uv = a_uv;\n"
    "    v_fog_depth = gl_Position.z / gl_Position.w;\n"
    "}\n";

static const char *fragment_shader_src =
    "#version 120\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "varying float v_fog_depth;\n"
    "uniform bool u_fog_enabled;\n"
    "uniform vec4 u_fog_color;\n"
    "uniform float u_fog_near;\n"
    "uniform float u_fog_far;\n"
    "void main() {\n"
    "    vec4 color = v_color;\n"
    "    if (u_fog_enabled) {\n"
    "        float fog = clamp((v_fog_depth - u_fog_near) / (u_fog_far - u_fog_near), 0.0, 1.0);\n"
    "        color = mix(color, u_fog_color, fog);\n"
    "    }\n"
    "    gl_FragColor = color;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "[T3D-GL] Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint create_program(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_color");
    glBindAttribLocation(prog, 2, "a_uv");
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "[T3D-GL] Program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

/* ----------------------------------------------------------------
 * Init / Shutdown
 * ---------------------------------------------------------------- */

bool t3d_gl_init(T3DGLRenderer *r) {
    memset(r, 0, sizeof(*r));

    /* Create OSMesa context */
    r->osmesa_ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!r->osmesa_ctx) {
        fprintf(stderr, "[T3D-GL] OSMesa context creation failed\n");
        return false;
    }

    r->osmesa_buffer = calloc(T3D_GL_WIDTH * T3D_GL_HEIGHT, 4);
    if (!OSMesaMakeCurrent(r->osmesa_ctx, r->osmesa_buffer,
                           GL_UNSIGNED_BYTE, T3D_GL_WIDTH, T3D_GL_HEIGHT)) {
        fprintf(stderr, "[T3D-GL] OSMesaMakeCurrent failed\n");
        OSMesaDestroyContext(r->osmesa_ctx);
        free(r->osmesa_buffer);
        return false;
    }

    fprintf(stderr, "[T3D-GL] OSMesa context created (%dx%d)\n",
            T3D_GL_WIDTH, T3D_GL_HEIGHT);
    fprintf(stderr, "[T3D-GL] GL: %s / %s\n",
            glGetString(GL_RENDERER), glGetString(GL_VERSION));

    /* Create shader program */
    r->shader_program = create_program();
    if (!r->shader_program) {
        fprintf(stderr, "[T3D-GL] Shader creation failed\n");
        OSMesaDestroyContext(r->osmesa_ctx);
        free(r->osmesa_buffer);
        return false;
    }

    glUseProgram(r->shader_program);
    r->u_mvp = glGetUniformLocation(r->shader_program, "u_mvp");
    r->u_fog_enabled = glGetUniformLocation(r->shader_program, "u_fog_enabled");
    r->u_fog_color = glGetUniformLocation(r->shader_program, "u_fog_color");
    r->u_fog_near = glGetUniformLocation(r->shader_program, "u_fog_near");
    r->u_fog_far = glGetUniformLocation(r->shader_program, "u_fog_far");

    /* Set default MVP to identity */
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(r->u_mvp, 1, GL_FALSE, identity);

    /* Enable depth test */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glViewport(0, 0, T3D_GL_WIDTH, T3D_GL_HEIGHT);

    r->initialized = true;
    r->context_valid = true;
    return true;
}

void t3d_gl_shutdown(T3DGLRenderer *r) {
    if (!r->initialized) return;
    if (r->shader_program) glDeleteProgram(r->shader_program);
    if (r->osmesa_ctx) OSMesaDestroyContext(r->osmesa_ctx);
    if (r->osmesa_buffer) free(r->osmesa_buffer);
    memset(r, 0, sizeof(*r));
}

/* ----------------------------------------------------------------
 * Frame rendering
 * ---------------------------------------------------------------- */

void t3d_gl_frame_begin(T3DGLRenderer *r) {
    if (!r->context_valid) return;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r->draw_calls = 0;
    r->triangles = 0;
}

void t3d_gl_upload_vertices(T3DGLRenderer *r,
                            const T3DHLEVertex *verts, int count) {
    if (!r->context_valid || count <= 0) return;

    /* Interleaved vertex data: pos(3f) + rgba(4ub, normalized) + uv(2f) */
    typedef struct {
        float pos[3];
        uint8_t rgba[4];
        float uv[2];
    } GLVert;

    GLVert *buf = (GLVert *)malloc(count * sizeof(GLVert));
    for (int i = 0; i < count; i++) {
        buf[i].pos[0] = verts[i].pos[0];
        buf[i].pos[1] = verts[i].pos[1];
        buf[i].pos[2] = verts[i].pos[2];
        buf[i].rgba[0] = verts[i].rgba[0];
        buf[i].rgba[1] = verts[i].rgba[1];
        buf[i].rgba[2] = verts[i].rgba[2];
        buf[i].rgba[3] = verts[i].rgba[3];
        buf[i].uv[0] = verts[i].uv[0];
        buf[i].uv[1] = verts[i].uv[1];
    }

    /* Use immediate-mode style VBO (recreated each upload) */
    if (!r->vbo) glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(GLVert), buf, GL_DYNAMIC_DRAW);

    /* Set vertex attribute pointers */
    glEnableVertexAttribArray(0); /* position */
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVert),
                          (void*)offsetof(GLVert, pos));
    glEnableVertexAttribArray(1); /* color */
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLVert),
                          (void*)offsetof(GLVert, rgba));
    glEnableVertexAttribArray(2); /* uv */
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLVert),
                          (void*)offsetof(GLVert, uv));

    free(buf);
}

void t3d_gl_set_mvp(T3DGLRenderer *r, const float *mvp) {
    if (!r->context_valid) return;
    glUseProgram(r->shader_program);
    glUniformMatrix4fv(r->u_mvp, 1, GL_FALSE, mvp);
}

void t3d_gl_set_draw_state(T3DGLRenderer *r,
                           bool depth_test, bool depth_write,
                           int cull_mode) {
    if (!r->context_valid) return;
    if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(depth_write ? GL_TRUE : GL_FALSE);
    if (cull_mode == 0) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(cull_mode == 1 ? GL_FRONT : GL_BACK);
    }
}

void t3d_gl_draw_triangles(T3DGLRenderer *r,
                           const uint16_t *indices, int tri_count) {
    if (!r->context_valid || tri_count <= 0) return;
    glDrawElements(GL_TRIANGLES, tri_count * 3, GL_UNSIGNED_SHORT, indices);
    r->draw_calls++;
    r->triangles += tri_count;
}

void t3d_gl_frame_end(T3DGLRenderer *r) {
    if (!r->context_valid) return;
    glFinish();
    /* OSMesa renders directly to osmesa_buffer — copy to our framebuffer */
    memcpy(r->framebuffer, r->osmesa_buffer,
           T3D_GL_WIDTH * T3D_GL_HEIGHT * 4);
}

/* ----------------------------------------------------------------
 * Framebuffer composite — write GL output to N64 RDRAM
 * ---------------------------------------------------------------- */

void t3d_gl_composite_to_rdram(T3DGLRenderer *r,
                               uint8_t *rdram, uint32_t fb_addr,
                               int width, int height) {
    if (!r->context_valid) return;
    fb_addr &= 0x1FFFFFFF; /* physical mask */

    for (int y = 0; y < height && y < T3D_GL_HEIGHT; y++) {
        for (int x = 0; x < width && x < T3D_GL_WIDTH; x++) {
            /* GL is bottom-up, N64 is top-down */
            int gl_y = T3D_GL_HEIGHT - 1 - y;
            uint8_t *px = &r->framebuffer[(gl_y * T3D_GL_WIDTH + x) * 4];
            uint8_t red = px[0], grn = px[1], blu = px[2], alp = px[3];

            /* Convert to RGBA5551 (big-endian in RDRAM) */
            uint16_t n64px = ((red >> 3) << 11) |
                             ((grn >> 3) <<  6) |
                             ((blu >> 3) <<  1) |
                             ((alp >> 7));

            uint32_t addr = fb_addr + (y * width + x) * 2;
            rdram[addr + 0] = (n64px >> 8) & 0xFF;
            rdram[addr + 1] = n64px & 0xFF;
        }
    }
}

#else /* !T3D_HLE_GL_ENABLED */

/* Stub implementations when GL is not available */
bool t3d_gl_init(T3DGLRenderer *r) {
    (void)r;
    fprintf(stderr, "[T3D-GL] GL not available (built without T3D_HLE_GL_ENABLED)\n");
    return false;
}
void t3d_gl_shutdown(T3DGLRenderer *r) { (void)r; }
void t3d_gl_frame_begin(T3DGLRenderer *r) { (void)r; }
void t3d_gl_upload_vertices(T3DGLRenderer *r, const T3DHLEVertex *v, int c) { (void)r; (void)v; (void)c; }
void t3d_gl_set_mvp(T3DGLRenderer *r, const float *m) { (void)r; (void)m; }
void t3d_gl_set_draw_state(T3DGLRenderer *r, bool d, bool w, int c) { (void)r; (void)d; (void)w; (void)c; }
void t3d_gl_draw_triangles(T3DGLRenderer *r, const uint16_t *i, int c) { (void)r; (void)i; (void)c; }
void t3d_gl_frame_end(T3DGLRenderer *r) { (void)r; }
void t3d_gl_composite_to_rdram(T3DGLRenderer *r, uint8_t *rd, uint32_t a, int w, int h) { (void)r; (void)rd; (void)a; (void)w; (void)h; }

#endif /* T3D_HLE_GL_ENABLED */

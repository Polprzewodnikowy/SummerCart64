#include <trx/game/output/quad.h>

#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/output/shaders/generic.h>
#include <trx/gl/utils.h>

#include <GL/glew.h>
#include <stddef.h>
#include <string.h>

typedef enum {
    M_UNIFORM_TEXTURE_MAIN,
    M_UNIFORM_TEXTURE_SIZE,
    M_UNIFORM_EFFECT,
    M_UNIFORM_OPACITY,
    M_UNIFORM_BRIGHTNESS_SCALE,
    M_UNIFORM_FIT_MODE,
    M_UNIFORM_SRC_ASPECT,
    M_UNIFORM_NUMBER_OF,
} M_UNIFORM;

typedef struct {
    struct {
        GLfloat x;
        GLfloat y;
    } pos;
    struct {
        GLfloat u;
        GLfloat v;
    } uv;
} M_VERTEX;

struct OUTPUT_QUAD {
    GLuint vao;
    GLuint vbo;
    GLuint texture;
    OUTPUT_SHADER *shader;

    M_VERTEX *vertices;
    int32_t vertex_count;

    bool ready;
    OUTPUT_QUAD_SURFACE_DESC desc;
    struct {
        int32_t x;
        int32_t y;
    } repeat;

    OUTPUT_QUAD_EFFECT effect;

    float opacity;
    float brightness_scale;

    OUTPUT_QUAD_FIT_MODE fit_mode;
    float src_aspect;

    bool use_external_texture;
    GLuint external_texture_id;

    GLint loc[M_UNIFORM_NUMBER_OF];
};

static const M_VERTEX m_Vertices[] = {
    { .pos = { .x = 0.0, .y = 0.0 }, .uv = { .u = 0.0, .v = 0.0 } },
    { .pos = { .x = 1.0, .y = 0.0 }, .uv = { .u = 1.0, .v = 0.0 } },
    { .pos = { .x = 0.0, .y = 1.0 }, .uv = { .u = 0.0, .v = 1.0 } },
    { .pos = { .x = 0.0, .y = 1.0 }, .uv = { .u = 0.0, .v = 1.0 } },
    { .pos = { .x = 1.0, .y = 0.0 }, .uv = { .u = 1.0, .v = 0.0 } },
    { .pos = { .x = 1.0, .y = 1.0 }, .uv = { .u = 1.0, .v = 1.0 } },
};

static const OUTPUT_QUAD_SURFACE_UV m_DefaultUV[] = {
    { .u = 0.0f, .v = 0.0f },
    { .u = 1.0f, .v = 0.0f },
    { .u = 1.0f, .v = 1.0f },
    { .u = 0.0f, .v = 1.0f },
};

static bool M_AllUVsZero(const OUTPUT_QUAD_SURFACE_DESC *const desc)
{
    for (int32_t i = 0; i < 4; i++) {
        if (desc->uv[i].u != 0.0f || desc->uv[i].v != 0.0f) {
            return false;
        }
    }
    return true;
}

static OUTPUT_QUAD_SURFACE_DESC M_NormalizeDesc(
    const OUTPUT_QUAD_SURFACE_DESC *const desc)
{
    OUTPUT_QUAD_SURFACE_DESC out = *desc;
    if (M_AllUVsZero(desc)) {
        memcpy(out.uv, m_DefaultUV, sizeof(m_DefaultUV));
    }
    return out;
}

static void M_BindProgram(const OUTPUT_QUAD *const r)
{
    Output_Shader_Bind(r->shader);
}

static void M_UploadVertices(OUTPUT_QUAD *const r)
{
    if (!r->ready) {
        return;
    }

    const int32_t mapping[] = { 0, 1, 3, 3, 1, 2 };
    r->vertex_count = r->repeat.x * r->repeat.y * 6;
    r->vertices = Memory_Realloc(
        r->vertices, r->repeat.x * r->repeat.y * 6 * sizeof(M_VERTEX));

    M_VERTEX *ptr = r->vertices;
    for (int32_t y = 0; y < r->repeat.y; y++) {
        for (int32_t x = 0; x < r->repeat.x; x++) {
            for (int32_t i = 0; i < 6; i++) {
                const float x_factor = (float)x / (float)r->repeat.x;
                const float y_factor = (float)y / (float)r->repeat.y;
                const float x_offset = 1.0f / (float)r->repeat.x;
                const float y_offset = 1.0f / (float)r->repeat.y;

                ptr->pos.x = m_Vertices[i].pos.x * x_offset + x_factor;
                ptr->pos.y = m_Vertices[i].pos.y * y_offset + y_factor;
                ptr->uv.u = r->desc.uv[mapping[i]].u;
                ptr->uv.v = r->desc.uv[mapping[i]].v;

                ptr++;
            }
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(M_VERTEX) * 6 * r->repeat.x * r->repeat.y,
        r->vertices, GL_STATIC_DRAW);
    TRX_GL_CheckError();
}

OUTPUT_QUAD *Output_Quad_Create(void)
{
    OUTPUT_QUAD *const r = Memory_Alloc(sizeof(OUTPUT_QUAD));

    r->effect = OUTPUT_QUAD_EFFECT_NONE;
    r->opacity = 1.0f;
    r->brightness_scale = 1.0f;
    r->repeat.x = 1;
    r->repeat.y = 1;

    r->fit_mode = OUTPUT_QUAD_FIT_STRETCH;
    r->src_aspect = 1.0f;

    r->vertices = nullptr;
    r->vertex_count = 6;
    r->use_external_texture = false;
    r->external_texture_id = 0;

    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(m_Vertices), m_Vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &r->vao);
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)offsetof(M_VERTEX, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)offsetof(M_VERTEX, uv));
    TRX_GL_CheckError();

    glGenTextures(1, &r->texture);
    TRX_GL_CheckError();

    r->shader = Output_Shader_Create("2d.glsl");

    r->loc[M_UNIFORM_TEXTURE_MAIN] =
        Output_Shader_LookupUniform(r->shader, "uTexMain");
    r->loc[M_UNIFORM_TEXTURE_SIZE] =
        Output_Shader_LookupUniform(r->shader, "uTexSize");
    r->loc[M_UNIFORM_EFFECT] =
        Output_Shader_LookupUniform(r->shader, "uEffect");
    r->loc[M_UNIFORM_OPACITY] =
        Output_Shader_LookupUniform(r->shader, "uOpacity");
    r->loc[M_UNIFORM_BRIGHTNESS_SCALE] =
        Output_Shader_LookupUniform(r->shader, "uBrightnessScale");
    r->loc[M_UNIFORM_FIT_MODE] =
        Output_Shader_LookupUniform(r->shader, "uFitMode");
    r->loc[M_UNIFORM_SRC_ASPECT] =
        Output_Shader_LookupUniform(r->shader, "uSrcAspect");

    M_BindProgram(r);
    glUniform1i(r->loc[M_UNIFORM_TEXTURE_MAIN], 0);
    glUniform4f(r->loc[M_UNIFORM_TEXTURE_SIZE], 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform1i(r->loc[M_UNIFORM_EFFECT], r->effect);
    glUniform1f(r->loc[M_UNIFORM_OPACITY], r->opacity);
    glUniform1f(r->loc[M_UNIFORM_BRIGHTNESS_SCALE], r->brightness_scale);
    glUniform1i(r->loc[M_UNIFORM_FIT_MODE], (int32_t)r->fit_mode);
    glUniform1f(r->loc[M_UNIFORM_SRC_ASPECT], r->src_aspect);
    TRX_GL_CheckError();

    return r;
}

void Output_Quad_Destroy(OUTPUT_QUAD *const r)
{
    ASSERT(r != nullptr);

    if (r->vao != 0) {
        glDeleteVertexArrays(1, &r->vao);
    }
    if (r->vbo != 0) {
        glDeleteBuffers(1, &r->vbo);
    }
    if (r->texture != 0) {
        glDeleteTextures(1, &r->texture);
    }
    TRX_GL_CheckError();

    if (r->shader != nullptr) {
        Output_Shader_Free(r->shader);
    }
    Memory_FreePointer(&r->vertices);
    Memory_Free(r);
}

void Output_Quad_Upload(
    OUTPUT_QUAD *const r, const OUTPUT_QUAD_SURFACE_DESC *const desc,
    const uint8_t *const data)
{
    ASSERT(r != nullptr);

    const OUTPUT_QUAD_SURFACE_DESC normalized_desc = M_NormalizeDesc(desc);

    bool reupload_vert = false;
    if (memcmp(r->desc.uv, normalized_desc.uv, sizeof(normalized_desc.uv))
        != 0) {
        reupload_vert = true;
    }
    if (!r->ready) {
        reupload_vert = true;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->texture);

    if (r->desc.width != normalized_desc.width
        || r->desc.height != normalized_desc.height
        || r->desc.tex_format != normalized_desc.tex_format
        || r->desc.tex_type != normalized_desc.tex_type) {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        TRX_GL_CheckError();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        TRX_GL_CheckError();
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, normalized_desc.width,
            normalized_desc.height, 0, normalized_desc.tex_format,
            normalized_desc.tex_type, data);
        TRX_GL_CheckError();
    } else {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        TRX_GL_CheckError();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        TRX_GL_CheckError();
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0, normalized_desc.width,
            normalized_desc.height, normalized_desc.tex_format,
            normalized_desc.tex_type, data);
        TRX_GL_CheckError();
    }

    r->ready = true;
    r->desc = normalized_desc;
    r->use_external_texture = false;
    r->external_texture_id = 0;
    if (reupload_vert) {
        M_UploadVertices(r);
    }
}

void Output_Quad_SetExternalTexture(
    OUTPUT_QUAD *const r, const GLuint texture_id, const int32_t width,
    const int32_t height, const bool flip_y)
{
    ASSERT(r != nullptr);
    r->use_external_texture = true;
    r->external_texture_id = texture_id;

    const float v0 = flip_y ? 1.0f : 0.0f;
    const float v1 = flip_y ? 0.0f : 1.0f;
    const OUTPUT_QUAD_SURFACE_DESC desc = {
        .width = width,
        .height = height,
        .bit_count = 32,
        .tex_format = GL_RGBA,
        .tex_type = GL_UNSIGNED_INT_8_8_8_8_REV,
        .uv = {
            { .u = 0.0f, .v = v0 },
            { .u = 1.0f, .v = v0 },
            { .u = 1.0f, .v = v1 },
            { .u = 0.0f, .v = v1 },
        },
        .pitch = width * 4,
    };

    const bool reupload_vert =
        memcmp(r->desc.uv, desc.uv, sizeof(desc.uv)) != 0 || !r->ready;
    r->ready = true;
    r->desc = desc;
    if (reupload_vert) {
        M_UploadVertices(r);
    }
}

void Output_Quad_ClearExternalTexture(OUTPUT_QUAD *const r)
{
    ASSERT(r != nullptr);
    r->use_external_texture = false;
    r->external_texture_id = 0;
}

void Output_Quad_SetTextureSize(
    OUTPUT_QUAD *const r, const OUTPUT_QUAD_TEXTURE_SIZE *const size)
{
    ASSERT(r != nullptr);
    M_BindProgram(r);
    if (size == nullptr) {
        glUniform4f(r->loc[M_UNIFORM_TEXTURE_SIZE], 0.0f, 0.0f, 1.0f, 1.0f);
    } else {
        glUniform4f(
            r->loc[M_UNIFORM_TEXTURE_SIZE], size->x0, size->y0, size->x1,
            size->y1);
    }
    TRX_GL_CheckError();
}

void Output_Quad_SetRepeat(
    OUTPUT_QUAD *const r, const int32_t x, const int32_t y)
{
    ASSERT(r != nullptr);
    if (r->repeat.x == x && r->repeat.y == y) {
        return;
    }
    r->repeat.x = x;
    r->repeat.y = y;
    M_UploadVertices(r);
}

void Output_Quad_SetEffect(OUTPUT_QUAD *const r, const uint32_t effect)
{
    ASSERT(r != nullptr);

    if (r->effect != effect) {
        M_BindProgram(r);
        glUniform1i(r->loc[M_UNIFORM_EFFECT], effect);
        TRX_GL_CheckError();
        r->effect = effect;
    }
}

void Output_Quad_SetOpacity(OUTPUT_QUAD *const r, const float opacity)
{
    ASSERT(r != nullptr);

    if (r->opacity != opacity) {
        M_BindProgram(r);
        glUniform1f(r->loc[M_UNIFORM_OPACITY], opacity);
        TRX_GL_CheckError();
        r->opacity = opacity;
    }
}

void Output_Quad_SetBrightnessScale(
    OUTPUT_QUAD *const r, const float brightness_scale)
{
    ASSERT(r != nullptr);

    if (r->brightness_scale != brightness_scale) {
        M_BindProgram(r);
        glUniform1f(r->loc[M_UNIFORM_BRIGHTNESS_SCALE], brightness_scale);
        TRX_GL_CheckError();
        r->brightness_scale = brightness_scale;
    }
}

void Output_Quad_SetFit(
    OUTPUT_QUAD *const r, const OUTPUT_QUAD_FIT_MODE fit_mode,
    const float src_w, const float src_h)
{
    ASSERT(r != nullptr);

    if (src_w <= 0.0f || src_h <= 0.0f) {
        Output_Quad_ClearFit(r);
        return;
    }
    const float src_aspect = src_w / src_h;
    if (r->fit_mode == fit_mode && r->src_aspect == src_aspect) {
        return;
    }

    r->fit_mode = fit_mode;
    r->src_aspect = src_aspect;

    M_BindProgram(r);
    glUniform1i(r->loc[M_UNIFORM_FIT_MODE], (int32_t)fit_mode);
    glUniform1f(r->loc[M_UNIFORM_SRC_ASPECT], src_aspect);
    TRX_GL_CheckError();
}

void Output_Quad_ClearFit(OUTPUT_QUAD *const r)
{
    ASSERT(r != nullptr);
    if (r->fit_mode == OUTPUT_QUAD_FIT_STRETCH && r->src_aspect == 1.0f) {
        return;
    }

    r->fit_mode = OUTPUT_QUAD_FIT_STRETCH;
    r->src_aspect = 1.0f;
    M_BindProgram(r);
    glUniform1i(r->loc[M_UNIFORM_FIT_MODE], (int32_t)r->fit_mode);
    glUniform1f(r->loc[M_UNIFORM_SRC_ASPECT], r->src_aspect);
    TRX_GL_CheckError();
}

void Output_Quad_Render(OUTPUT_QUAD *const r)
{
    ASSERT(r != nullptr);

    M_BindProgram(r);
    glUniform1i(r->loc[M_UNIFORM_EFFECT], r->effect);
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);

    glActiveTexture(GL_TEXTURE0);
    if (r->use_external_texture) {
        glBindTexture(GL_TEXTURE_2D, r->external_texture_id);
    } else {
        glBindTexture(GL_TEXTURE_2D, r->texture);
    }

    const GLboolean was_blend_enabled = glIsEnabled(GL_BLEND);
    if (was_blend_enabled) {
        glDisable(GL_BLEND);
    }

    GLint bound_polygon_mode[2];
    glGetIntegerv(GL_POLYGON_MODE, &bound_polygon_mode[0]);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const GLboolean was_depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    if (was_depth_test_enabled) {
        glDisable(GL_DEPTH_TEST);
    }

    glDrawArrays(GL_TRIANGLES, 0, r->vertex_count);

    glPolygonMode(GL_FRONT_AND_BACK, bound_polygon_mode[0]);
    if (was_depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (was_blend_enabled) {
        glEnable(GL_BLEND);
    }
    TRX_GL_CheckError();
}

void Output_Quad_RenderWithBlend(OUTPUT_QUAD *const r)
{
    ASSERT(r != nullptr);

    M_BindProgram(r);
    glUniform1i(r->loc[M_UNIFORM_EFFECT], r->effect);
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);

    glActiveTexture(GL_TEXTURE0);
    if (r->use_external_texture) {
        glBindTexture(GL_TEXTURE_2D, r->external_texture_id);
    } else {
        glBindTexture(GL_TEXTURE_2D, r->texture);
    }

    const GLboolean was_blend_enabled = glIsEnabled(GL_BLEND);
    GLint prev_blend_src = 0;
    GLint prev_blend_dst = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &prev_blend_src);
    glGetIntegerv(GL_BLEND_DST_RGB, &prev_blend_dst);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLint bound_polygon_mode[2];
    glGetIntegerv(GL_POLYGON_MODE, &bound_polygon_mode[0]);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const GLboolean was_depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    if (was_depth_test_enabled) {
        glDisable(GL_DEPTH_TEST);
    }

    glDrawArrays(GL_TRIANGLES, 0, r->vertex_count);

    glPolygonMode(GL_FRONT_AND_BACK, bound_polygon_mode[0]);
    if (was_depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    }
    glBlendFunc(prev_blend_src, prev_blend_dst);
    if (!was_blend_enabled) {
        glDisable(GL_BLEND);
    }
    TRX_GL_CheckError();
}

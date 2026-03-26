#include <trx/game/output/sources/lightnings.h>

#include <trx/game/output.h>
#include <trx/game/output/scene_compositor.h>
#include <trx/game/output/utils.h>
#include <trx/game/output/vertex_range.h>
#include <trx/gl/utils.h>

typedef struct {
    XYZW_F pos;
    XYZW_F normal;
    RGBA_8888 color;
} M_VERTEX;

typedef struct {
    SCENE_SOURCE source;
    OUTPUT_MESH_SHADER *shader;
    VECTOR *vertices;
    VECTOR *scheduled;
    GLuint vao;
    GLuint vbo;
} M_PRIV;

static M_PRIV m_Priv;

static void M_GenerateLightningSegment(
    M_PRIV *const p, const LIGHTNING_SEGMENT *const segment)
{
    const RGBA_8888 blue = { 0, 0, 255, 128 };
    const RGBA_8888 white = { 255, 255, 255, 128 };
    const int32_t w = segment->thickness / 2;

    const XYZW_F pos_0 = {
        .x = segment->from.x,
        .y = segment->from.y,
        .z = segment->from.z,
        .w = 0.0f,
    };
    const XYZW_F pos_1 = {
        .x = segment->to.x,
        .y = segment->to.y,
        .z = segment->to.z,
        .w = 0.0f,
    };

    // 2 quads side-to-side (blue-white) (white-blue);
    // double-sided so that visible from both sides
    const M_VERTEX vertices[4][4] = {
        // clang-format off
        {
            { .pos = pos_0, .normal = { 0, 0, 0, 0 }, .color = white },
            { .pos = pos_1, .normal = { 0, 0, 0, 0 }, .color = white },
            { .pos = pos_1, .normal = { w, 0, 0, 0 }, .color = blue },
            { .pos = pos_0, .normal = { w, 0, 0, 0 }, .color = blue },
        },
        {
            { .pos = pos_0, .normal = { 0, 0, 0, 0 }, .color = white },
            { .pos = pos_1, .normal = { 0, 0, 0, 0 }, .color = white },
            { .pos = pos_1, .normal = { -w, 0, 0, 0 }, .color = blue },
            { .pos = pos_0, .normal = { -w, 0, 0, 0 }, .color = blue },
        },
        {
            { .pos = pos_0, .normal = { 0, 0, 0, 0 }, .color = white },
            { .pos = pos_0, .normal = { w, 0, 0, 0 }, .color = blue },
            { .pos = pos_1, .normal = { w, 0, 0, 0 }, .color = blue },
            { .pos = pos_1, .normal = { 0, 0, 0, 0 }, .color = white },
        },
        {
            { .pos = pos_0, .normal = { 0, 0, 0, 0 }, .color = white },
            { .pos = pos_0, .normal = { -w, 0, 0, 0 }, .color = blue },
            { .pos = pos_1, .normal = { -w, 0, 0, 0 }, .color = blue },
            { .pos = pos_1, .normal = { 0, 0, 0, 0 }, .color = white },
        },
        // clang-format on
    };

    for (int32_t quad = 0; quad < 4; quad++) {
        for (int32_t i = 0; i < OUTPUT_QUAD_VERTICES; i++) {
            const int32_t j = OUTPUT_QUAD_TO_FAN(i);
            Vector_Add(p->vertices, &vertices[quad][j]);
        }
    }
}

static void M_RenderBegin(const SCENE_SOURCE *const source)
{
    M_PRIV *const p = &m_Priv;
    Vector_Clear(p->scheduled);
    Vector_Clear(p->vertices);
}

static void M_RenderPass(
    const SCENE_SOURCE *const source, const SCENE_PASS pass)
{
    M_PRIV *const p = &m_Priv;
    if (pass != SCENE_PASS_TRANSPARENT) {
        return;
    }

    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glVertexAttrib4f(OUTPUT_MESH_ATTR_NORMAL, 0.0f, 0.0f, 0.0f, 0.0f);
    glVertexAttrib3f(OUTPUT_MESH_ATTR_UVW, 0.0f, 0.0f, 0.0f);
    glVertexAttrib4f(OUTPUT_MESH_ATTR_TEXTURE_SIZE, 0.0f, 0.0f, 1.0f, 1.0f);
    glVertexAttrib2f(OUTPUT_MESH_ATTR_TRAPEZOID_RATIO, 1.0f, 1.0f);
    glVertexAttribI1ui(
        OUTPUT_MESH_ATTR_FLAGS,
        VERT_FLAT_SHADED | VERT_NO_LIGHTING | VERT_NO_WIBBLE | VERT_BILLBOARD
            | VERT_ABS_SPRITE);
    glVertexAttrib1f(OUTPUT_MESH_ATTR_SHADE, SHADE_NEUTRAL);

    for (int32_t i = 0; i < p->scheduled->count; i++) {
        const LIGHTNING_SEGMENT *const segment = Vector_Get(p->scheduled, i);
        M_GenerateLightningSegment(p, segment);
    }

    TRX_GL_TRACK_DATA(
        glBufferData, GL_ARRAY_BUFFER, p->vertices->count * sizeof(M_VERTEX),
        Vector_GetData(p->vertices), GL_STATIC_DRAW);

    Output_MeshShader_UploadModelMatrix(p->shader, &g_IDMatrix);
    glDrawArrays(GL_TRIANGLES, 0, p->vertices->count);
    TRX_GL_CheckError();
}

static bool M_IsDirty(const SCENE_SOURCE *const source, const SCENE_PASS pass)
{
    const M_PRIV *const p = &m_Priv;
    return pass == SCENE_PASS_TRANSPARENT && p->scheduled->count > 0;
}

void OutputSource_Lightnings_Init(void)
{
    M_PRIV *const p = &m_Priv;
    p->shader = Output_GetMeshShader();
    p->vertices = Vector_Create(sizeof(M_VERTEX));
    p->scheduled = Vector_Create(sizeof(LIGHTNING_SEGMENT));
    p->source.render_begin = M_RenderBegin;
    p->source.render_pass = M_RenderPass;
    p->source.is_dirty = M_IsDirty;
    SceneCompositor_AddSource(&p->source);

    glGenVertexArrays(1, &p->vao);
    glBindVertexArray(p->vao);

    glGenBuffers(1, &p->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_POS);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_NORMAL);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_UVW);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_TEXTURE_SIZE);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_TRAPEZOID_RATIO);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_FLAGS);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_COLOR);
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_POS, 4, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, pos));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_NORMAL, 4, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, normal));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, color));
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_SHADE);
}

void OutputSource_Lightnings_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->vertices != nullptr) {
        Vector_Free(p->vertices);
        p->vertices = nullptr;
    }
    if (p->scheduled != nullptr) {
        Vector_Free(p->scheduled);
        p->scheduled = nullptr;
    }
    if (p->vao != 0) {
        glDeleteVertexArrays(1, &p->vao);
        p->vao = 0;
    }
    if (p->vbo != 0) {
        glDeleteBuffers(1, &p->vbo);
        p->vbo = 0;
    }
}

void OutputSource_Lightnings_StageSegment(
    const LIGHTNING_SEGMENT *const segment)
{
    M_PRIV *const p = &m_Priv;
    Vector_Add(p->scheduled, segment);
}

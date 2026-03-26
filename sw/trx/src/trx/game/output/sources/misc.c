#include <trx/game/output/sources/misc.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/game/output.h>
#include <trx/game/output/scene_compositor.h>
#include <trx/game/output/utils.h>
#include <trx/game/output/vertex_range.h>
#include <trx/gl/utils.h>

#include <math.h>

typedef struct {
    XYZW_F pos;
} M_VERTEX;

typedef enum {
    M_PRIMITIVE_SPHERE,
    M_PRIMITIVE_CUBOID,
    M_PRIMITIVE_NUMBER_OF,
} M_PRIMITIVE_TYPE;

typedef struct {
    MATRIX matrix;
    M_PRIMITIVE_TYPE prim_type;
    RGBA_8888 color;
} M_INSTANCE;

typedef struct {
    SCENE_SOURCE source;
    OUTPUT_MESH_SHADER *shader;
    OUTPUT_VERTEX_RANGE primitive_ranges[M_PRIMITIVE_NUMBER_OF];
    VECTOR *vertices;
    VECTOR *scheduled_spheres;
    VECTOR *scheduled_cuboids;
    GLuint vao;
    GLuint vbo;
    int32_t vertex_count;
} M_PRIV;

static M_PRIV m_Priv;

static void M_SealPrimitive(
    M_PRIV *const p, OUTPUT_VERTEX_RANGE *const target_range)
{
    target_range->vertex_start = p->vertex_count;
    target_range->vertex_count =
        p->vertices->count - target_range->vertex_start;
    p->vertex_count += target_range->vertex_count;
}

static void M_GenerateSphere(
    M_PRIV *const p, OUTPUT_VERTEX_RANGE *const target_range,
    const int32_t subdivisions)
{
    // More subdivisions means smoother spheres.
    const int32_t position_count = SQUARE(subdivisions + 1);
    XYZW_F positions[position_count];
    int32_t index = 0;

    for (int32_t i = 0; i <= subdivisions; i++) {
        const float theta = (M_PI * i) / subdivisions; // Latitude angle
        const float sin_theta = sinf(theta);
        const float cos_theta = cosf(theta);

        for (int32_t j = 0; j <= subdivisions; j++) {
            const float phi = (2 * M_PI * j) / subdivisions; // Longitude angle
            const float sin_phi = sinf(phi);
            const float cos_phi = cosf(phi);

            // Convert spherical coordinates to 3D points.
            positions[index] = (XYZW_F) {
                .x = cos_phi * sin_theta,
                .y = cos_theta,
                .z = sin_phi * sin_theta,
                .w = 0.0f,
            };
            index++;
        }
    }

    const int32_t vertex_count =
        subdivisions * subdivisions * OUTPUT_QUAD_VERTICES;
    for (int32_t i = 0; i < subdivisions; i++) {
        for (int32_t j = 0; j < subdivisions; j++) {
            const int32_t indices[4] = {
                i * (subdivisions + 1) + j,
                (i + 1) * (subdivisions + 1) + j,
                (i + 1) * (subdivisions + 1) + (j + 1),
                i * (subdivisions + 1) + (j + 1),
            };
            for (int32_t k = 0; k < OUTPUT_QUAD_VERTICES; k++) {
                const int32_t l = OUTPUT_QUAD_TO_FAN(k);
                Vector_Add(
                    p->vertices,
                    &(M_VERTEX) {
                        .pos = positions[indices[l]],
                    });
            }
            for (int32_t k = 0; k < OUTPUT_QUAD_VERTICES; k++) {
                const int32_t l = OUTPUT_QUAD_TO_FAN_CW(k);
                Vector_Add(
                    p->vertices,
                    &(M_VERTEX) {
                        .pos = positions[indices[l]],
                    });
            }
        }
    }
    M_SealPrimitive(p, target_range);
}

static void M_GenerateCuboid(
    M_PRIV *const p, OUTPUT_VERTEX_RANGE *const target_range)
{
    const XYZW_F vertices[8] = {
        { -1, -1, 1, 0 },  { 1, -1, 1, 0 },
        { 1, 1, 1, 0 },    { -1, 1, 1, 0 }, // front
        { -1, -1, -1, 0 }, { 1, -1, -1, 0 },
        { 1, 1, -1, 0 },   { -1, 1, -1, 0 } // back
    };
    const uint8_t order[6][6] = {
        { 0, 1, 2, 0, 2, 3 }, { 5, 4, 7, 5, 7, 6 }, { 4, 0, 3, 4, 3, 7 },
        { 1, 5, 6, 1, 6, 2 }, { 3, 2, 6, 3, 6, 7 }, { 4, 5, 1, 4, 1, 0 },
    };
    for (int32_t i = 0; i < 6; i++) {
        for (int32_t j = 0; j < 6; j++) {
            Vector_Add(
                p->vertices,
                &(M_VERTEX) {
                    .pos = vertices[order[i][j]],
                });
        }
    }
    M_SealPrimitive(p, target_range);
}

static void M_DrawScheduled(M_PRIV *const p, VECTOR *const scheduled)
{
    for (int32_t i = 0; i < scheduled->count; i++) {
        const M_INSTANCE *const instance = Vector_Get(scheduled, i);
        Output_MeshShader_UploadModelMatrix(p->shader, &instance->matrix);
        const RGBA_8888 c = instance->color;
        glVertexAttrib4f(
            OUTPUT_MESH_ATTR_COLOR, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
            c.a / 255.0f);

        const OUTPUT_VERTEX_RANGE *const range =
            &p->primitive_ranges[instance->prim_type];
        glDrawArrays(GL_TRIANGLES, range->vertex_start, range->vertex_count);
    }
}

static void M_RenderBegin(const SCENE_SOURCE *const source)
{
    M_PRIV *const p = &m_Priv;
    Vector_Clear(p->scheduled_spheres);
    Vector_Clear(p->scheduled_cuboids);
}

static bool M_IsDirty(const SCENE_SOURCE *const source, const SCENE_PASS pass)
{
    const M_PRIV *const p = &m_Priv;
    return pass == SCENE_PASS_TRANSPARENT
        && (p->scheduled_spheres->count > 0 || p->scheduled_cuboids->count > 0);
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
        VERT_FLAT_SHADED | VERT_NO_LIGHTING | VERT_NO_WIBBLE);
    glVertexAttrib1f(OUTPUT_MESH_ATTR_SHADE, SHADE_NEUTRAL);

    GLint bound_polygon_mode[2];
    glGetIntegerv(GL_POLYGON_MODE, &bound_polygon_mode[0]);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (p->scheduled_spheres->count > 0) {
        M_DrawScheduled(p, p->scheduled_spheres);
    }
    if (p->scheduled_cuboids->count > 0) {
        M_DrawScheduled(p, p->scheduled_cuboids);
    }
    glPolygonMode(GL_FRONT_AND_BACK, bound_polygon_mode[0]);
}

void OutputSource_Misc_Init(void)
{
    M_PRIV *const p = &m_Priv;
    p->shader = Output_GetMeshShader();
    p->vertices = Vector_Create(sizeof(M_VERTEX));
    p->scheduled_spheres = Vector_Create(sizeof(M_INSTANCE));
    p->scheduled_cuboids = Vector_Create(sizeof(M_INSTANCE));
    p->source.render_begin = M_RenderBegin;
    p->source.render_pass = M_RenderPass;
    p->source.is_dirty = M_IsDirty;
    SceneCompositor_AddSource(&p->source);

    glGenVertexArrays(1, &p->vao);
    glBindVertexArray(p->vao);

    glGenBuffers(1, &p->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_POS);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_COLOR);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_NORMAL);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_UVW);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_TEXTURE_SIZE);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_TRAPEZOID_RATIO);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_FLAGS);
    glDisableVertexAttribArray(OUTPUT_MESH_ATTR_SHADE);
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_POS, 4, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, pos));

    M_GenerateSphere(p, &p->primitive_ranges[M_PRIMITIVE_SPHERE], 12);
    M_GenerateCuboid(p, &p->primitive_ranges[M_PRIMITIVE_CUBOID]);

    TRX_GL_TRACK_DATA(
        glBufferData, GL_ARRAY_BUFFER, p->vertex_count * sizeof(M_VERTEX),
        Vector_GetData(p->vertices), GL_STATIC_DRAW);
}

void OutputSource_Misc_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->vertices != nullptr) {
        Vector_Free(p->vertices);
        p->vertices = nullptr;
    }
    if (p->scheduled_spheres != nullptr) {
        Vector_Free(p->scheduled_spheres);
        p->scheduled_spheres = nullptr;
    }
    if (p->scheduled_cuboids != nullptr) {
        Vector_Free(p->scheduled_cuboids);
        p->scheduled_cuboids = nullptr;
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

void OutputSource_Misc_StageSphere(const RGBA_8888 color)
{
    M_PRIV *const p = &m_Priv;
    M_INSTANCE inst = {
        .matrix = *g_WMatrixPtr,
        .prim_type = M_PRIMITIVE_SPHERE,
        .color = color,
    };
    Vector_Add(p->scheduled_spheres, &inst);
}

void OutputSource_Misc_StageCuboid(const RGBA_8888 color)
{
    M_PRIV *const p = &m_Priv;
    M_INSTANCE inst = {
        .matrix = *g_WMatrixPtr,
        .prim_type = M_PRIMITIVE_CUBOID,
        .color = color,
    };
    Vector_Add(p->scheduled_cuboids, &inst);
}

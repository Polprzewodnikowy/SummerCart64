#include <trx/game/output/sources/poly_fx.h>

#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/effects.h>
#include <trx/game/interpolation.h>
#include <trx/game/items.h>
#include <trx/game/output.h>
#include <trx/game/output/scene_compositor.h>
#include <trx/game/output/shaders/mesh.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/utils.h>
#include <trx/game/sparks.h>
#include <trx/gl/utils.h>

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    XYZW_F pos;
    XYZW_F normal;
    OUTPUT_UVW uvw;
    OUTPUT_TEXTURE_SIZE texture_size;
    float trapezoid_ratio[2];
    OUTPUT_USHORT flags;
    RGBA_8888 color;
    float shade;
} M_VERTEX;

typedef struct {
    int32_t sprite_idx;
    bool use_custom_uv;
    uint8_t corner_count;
    float z_depth_adjust;
    XYZ_32 world_pos[4];
    OUTPUT_UVW uvw[4];
    OUTPUT_TEXTURE_SIZE texture_size[4];
    float disp[4][2];
    RGBA_8888 color[4];
    uint16_t flags;
} M_PRIM;

typedef struct {
    int32_t sort_key;
    const M_PRIM *prim;
} M_PRIM_SORT;

typedef struct {
    SCENE_SOURCE source;
    OUTPUT_MESH_SHADER *shader;
    VECTOR *scheduled_transparent; // M_PRIM
    VECTOR *scheduled_blend_add; // M_PRIM
    VECTOR *scheduled_blend_sub; // M_PRIM
    VECTOR *sorted; // M_PRIM_SORT
    VECTOR *vertices; // M_VERTEX
    GLuint vao;
    GLuint vbo;
} M_PRIV;

static M_PRIV m_Priv;

static int M_ComparePrimDepth(const void *const a, const void *const b)
{
    const M_PRIM_SORT *const prim_a = a;
    const M_PRIM_SORT *const prim_b = b;
    if (prim_b->sort_key == prim_a->sort_key) {
        return (intptr_t)prim_b->prim - (intptr_t)prim_a->prim;
    }
    return prim_b->sort_key - prim_a->sort_key;
}

static int32_t M_GetViewDepth(const XYZ_32 pos)
{
    // clang-format off
    return
        g_ViewMatrix._20 * pos.x +
        g_ViewMatrix._21 * pos.y +
        g_ViewMatrix._22 * pos.z +
        g_ViewMatrix._23;
    // clang-format on
}

static XYZ_32 M_GetSparkRenderPos(const SPARK *const spark, const float ratio)
{
    const bool use_current_state =
        (int32_t)spark->s_life - (int32_t)spark->life <= 1;
    if (use_current_state) {
        return Sparks_GetWorldPos(spark);
    }

    if ((spark->flags & SPARK_F_ATTACHED_NODE) != 0U) {
        const XYZ_32 current_pos = Sparks_GetWorldPos(spark);
        return (XYZ_32) {
            .x = (int32_t)LERP(spark->prev_world_pos.x, current_pos.x, ratio),
            .y = (int32_t)LERP(spark->prev_world_pos.y, current_pos.y, ratio),
            .z = (int32_t)LERP(spark->prev_world_pos.z, current_pos.z, ratio),
        };
    }

    const XYZ_32 local_pos = {
        .x = (int32_t)LERP(spark->prev_pos.x, spark->pos.x, ratio),
        .y = (int32_t)LERP(spark->prev_pos.y, spark->pos.y, ratio),
        .z = (int32_t)LERP(spark->prev_pos.z, spark->pos.z, ratio),
    };

    if ((spark->flags & SPARK_F_FX) != 0U) {
        const EFFECT *const effect = Effect_Get(spark->effect_num);
        if ((spark->flags & SPARK_F_ATTACHED_POS) != 0U) {
            return effect->interp.result.pos;
        }
        return (XYZ_32) {
            .x = effect->interp.result.pos.x + local_pos.x,
            .y = effect->interp.result.pos.y + local_pos.y,
            .z = effect->interp.result.pos.z + local_pos.z,
        };
    }

    if ((spark->flags & SPARK_F_ITEM) != 0U) {
        const ITEM *const item = Item_Get(spark->item_num);
        if ((spark->flags & SPARK_F_ATTACHED_POS) != 0U) {
            return item->interp.result.pos;
        }
        return (XYZ_32) {
            .x = item->interp.result.pos.x + local_pos.x,
            .y = item->interp.result.pos.y + local_pos.y,
            .z = item->interp.result.pos.z + local_pos.z,
        };
    }

    return local_pos;
}

static void M_ApplyTintToColors(RGBA_8888 color[4], const uint8_t corner_count)
{
    const RGB_F tint = Output_GetTint();
    for (uint8_t i = 0; i < corner_count; i++) {
        int32_t r = (int32_t)((float)color[i].r * tint.r);
        int32_t g = (int32_t)((float)color[i].g * tint.g);
        int32_t b = (int32_t)((float)color[i].b * tint.b);
        CLAMP(r, 0, 255);
        CLAMP(g, 0, 255);
        CLAMP(b, 0, 255);
        color[i].r = (uint8_t)r;
        color[i].g = (uint8_t)g;
        color[i].b = (uint8_t)b;
    }
}

static XYZ_32 M_GetPrimCentroid(const M_PRIM *const prim)
{
    XYZ_32 centroid = { 0, 0, 0 };
    for (uint8_t i = 0; i < prim->corner_count; i++) {
        centroid.x += prim->world_pos[i].x;
        centroid.y += prim->world_pos[i].y;
        centroid.z += prim->world_pos[i].z;
    }
    centroid.x /= (int32_t)prim->corner_count;
    centroid.y /= (int32_t)prim->corner_count;
    centroid.z /= (int32_t)prim->corner_count;
    return centroid;
}

static VECTOR *M_GetScheduledVectorForPass(
    M_PRIV *const p, const SCENE_PASS pass)
{
    if (pass == SCENE_PASS_BLEND_ADD) {
        return p->scheduled_blend_add;
    }
    if (pass == SCENE_PASS_BLEND_SUB) {
        return p->scheduled_blend_sub;
    }
    return p->scheduled_transparent;
}

static void M_SortPrims(M_PRIV *const p, const SCENE_PASS pass)
{
    Vector_Clear(p->sorted);

    const VECTOR *const prims = M_GetScheduledVectorForPass(p, pass);
    for (int32_t i = 0; i < prims->count; i++) {
        const M_PRIM *const prim = Vector_Get(prims, i);
        const XYZ_32 centroid = M_GetPrimCentroid(prim);
        const M_PRIM_SORT sort = {
            .sort_key = M_GetViewDepth(centroid),
            .prim = prim,
        };
        Vector_Add(p->sorted, &sort);
    }

    if (p->sorted->count > 1) {
        qsort(
            Vector_GetData(p->sorted), p->sorted->count, sizeof(M_PRIM_SORT),
            M_ComparePrimDepth);
    }
}

static void M_EmitPrimVertices(M_PRIV *const p, const M_PRIM *const prim)
{
    const uint16_t flags = prim->flags;
    const uint8_t corner_count = prim->corner_count;

    const int32_t sprite_corner_map_quad[4] = { 0, 1, 2, 3 };
    const int32_t sprite_corner_map_tri[3] = { 0, 1, 3 };
    const int32_t *sprite_corner_map = sprite_corner_map_quad;
    if (corner_count == 3U) {
        sprite_corner_map = sprite_corner_map_tri;
    }

    OUTPUT_UVW uvw[4];
    OUTPUT_TEXTURE_SIZE texture_size[4];
    for (uint8_t i = 0; i < corner_count; i++) {
        if (prim->use_custom_uv) {
            uvw[i] = prim->uvw[i];
            texture_size[i] = prim->texture_size[i];
        } else if (prim->sprite_idx >= 0) {
            const int32_t sprite_corner = sprite_corner_map[i];
            const int32_t uvw_idx = Output_Textures_GetSpriteUVWIndex(
                prim->sprite_idx, (int16_t)sprite_corner);
            uvw[i] = Output_Textures_GetUVW(uvw_idx);
            texture_size[i] = Output_Textures_GetAtlasSize(uvw_idx / 4);
        } else {
            uvw[i] = (OUTPUT_UVW) { 0.0f, 0.0f, 0.0f };
            texture_size[i] = (OUTPUT_TEXTURE_SIZE) { 0.0f, 0.0f, 0.0f, 0.0f };
        }
    }

    const int32_t idx_quad[2][OUTPUT_QUAD_VERTICES] = {
        { 0, 1, 2, 0, 2, 3 }, // front
        { 0, 2, 1, 0, 3, 2 }, // back
    };
    const int32_t idx_tri[2][3] = {
        { 0, 1, 2 }, // front
        { 0, 2, 1 }, // back
    };

    const int32_t *idx = (const int32_t *)idx_quad;
    int32_t vertex_count = OUTPUT_QUAD_VERTICES;
    if (corner_count == 3U) {
        idx = (const int32_t *)idx_tri;
        vertex_count = 3;
    }

    for (int32_t side = 0; side < 2; side++) {
        for (int32_t i = 0; i < vertex_count; i++) {
            const int32_t corner = idx[side * vertex_count + i];
            const int32_t uv_idx = corner;
            const M_VERTEX v = {
                .pos = {
                    .x = (float)prim->world_pos[corner].x,
                    .y = (float)prim->world_pos[corner].y,
                    .z = (float)prim->world_pos[corner].z,
                    .w = prim->z_depth_adjust,
                },
                .normal = {
                    .x = prim->disp[corner][0],
                    .y = prim->disp[corner][1],
                    .z = 0.0f,
                    .w = 0.0f,
                },
                .uvw = uvw[uv_idx],
                .texture_size = texture_size[uv_idx],
                .trapezoid_ratio = { 1.0f, 1.0f },
                .flags = flags,
                .color = prim->color[corner],
                .shade = (float)SHADE_NEUTRAL,
            };
            Vector_Add(p->vertices, &v);
        }
    }
}

static void M_RenderBegin(const SCENE_SOURCE *const source)
{
    M_PRIV *const p = &m_Priv;
    Vector_Clear(p->scheduled_transparent);
    Vector_Clear(p->scheduled_blend_add);
    Vector_Clear(p->scheduled_blend_sub);
    Vector_Clear(p->vertices);
    Vector_Clear(p->sorted);
}

static void M_RenderPass(const SCENE_SOURCE *const source, SCENE_PASS pass)
{
    M_PRIV *const p = &m_Priv;
    if (pass != SCENE_PASS_OPAQUE && pass != SCENE_PASS_TRANSPARENT
        && pass != SCENE_PASS_BLEND_SUB && pass != SCENE_PASS_BLEND_ADD) {
        return;
    }

    if (pass == SCENE_PASS_OPAQUE) {
        pass = SCENE_PASS_TRANSPARENT;
    } else {
        Vector_Clear(p->vertices);
        Vector_Clear(p->sorted);
    }

    M_SortPrims(p, pass);
    for (int32_t i = 0; i < p->sorted->count; i++) {
        const M_PRIM_SORT *const sort = Vector_Get(p->sorted, i);
        M_EmitPrimVertices(p, sort->prim);
    }

    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    TRX_GL_TRACK_DATA(
        glBufferData, GL_ARRAY_BUFFER, p->vertices->count * sizeof(M_VERTEX),
        Vector_GetData(p->vertices), GL_DYNAMIC_DRAW);

    // PolyFX primitives are staged with Output_GetTint() already applied at
    // stage time, because different rooms can have different water state.
    Output_MeshShader_UploadTint(p->shader, COLOR_RGB_F_WHITE);
    Output_MeshShader_UploadWaterEffect(p->shader, 0);
    Output_MeshShader_UploadWibbleEffect(p->shader, false);
    Output_MeshShader_UploadModelMatrix(p->shader, &g_IDMatrix);
    glDrawArrays(GL_TRIANGLES, 0, p->vertices->count);
    TRX_GL_CheckError();
}

static bool M_IsDirty(const SCENE_SOURCE *const source, const SCENE_PASS pass)
{
    const M_PRIV *const p = &m_Priv;
    if (pass == SCENE_PASS_TRANSPARENT || pass == SCENE_PASS_OPAQUE) {
        return p->scheduled_transparent->count > 0;
    }
    if (pass == SCENE_PASS_BLEND_SUB) {
        return p->scheduled_blend_sub->count > 0;
    }
    if (pass == SCENE_PASS_BLEND_ADD) {
        return p->scheduled_blend_add->count > 0;
    }
    return false;
}

void OutputSource_PolyFX_Init(void)
{
    M_PRIV *const p = &m_Priv;
    p->shader = Output_GetMeshShader();
    p->scheduled_transparent = Vector_Create(sizeof(M_PRIM));
    p->scheduled_blend_add = Vector_Create(sizeof(M_PRIM));
    p->scheduled_blend_sub = Vector_Create(sizeof(M_PRIM));
    p->sorted = Vector_Create(sizeof(M_PRIM_SORT));
    p->vertices = Vector_Create(sizeof(M_VERTEX));
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
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_UVW);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_TEXTURE_SIZE);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_TRAPEZOID_RATIO);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_FLAGS);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_COLOR);
    glEnableVertexAttribArray(OUTPUT_MESH_ATTR_SHADE);

    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_POS, 4, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, pos));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_NORMAL, 4, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, normal));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_UVW, 3, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, uvw));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_TEXTURE_SIZE, 4, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, texture_size));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_TRAPEZOID_RATIO, 2, GL_FLOAT, GL_FALSE,
        sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, trapezoid_ratio));
    glVertexAttribIPointer(
        OUTPUT_MESH_ATTR_FLAGS, 1, OUTPUT_USHORT_GL, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, flags));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, color));
    glVertexAttribPointer(
        OUTPUT_MESH_ATTR_SHADE, 1, GL_FLOAT, GL_FALSE, sizeof(M_VERTEX),
        (void *)(intptr_t)offsetof(M_VERTEX, shade));
}

void OutputSource_PolyFX_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->scheduled_transparent != nullptr) {
        Vector_Free(p->scheduled_transparent);
        p->scheduled_transparent = nullptr;
    }
    if (p->scheduled_blend_add != nullptr) {
        Vector_Free(p->scheduled_blend_add);
        p->scheduled_blend_add = nullptr;
    }
    if (p->scheduled_blend_sub != nullptr) {
        Vector_Free(p->scheduled_blend_sub);
        p->scheduled_blend_sub = nullptr;
    }
    if (p->sorted != nullptr) {
        Vector_Free(p->sorted);
        p->sorted = nullptr;
    }
    if (p->vertices != nullptr) {
        Vector_Free(p->vertices);
        p->vertices = nullptr;
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

static void M_StagePrim(
    const int32_t sprite_idx, const uint8_t corner_count,
    const XYZ_32 *const world_pos, const float (*disp)[2],
    const RGBA_8888 *const color, const uint16_t flags,
    const float z_depth_adjust, VECTOR *const target)
{
    M_PRIM prim;
    prim.sprite_idx = sprite_idx;
    prim.use_custom_uv = false;
    prim.corner_count = corner_count;
    prim.z_depth_adjust = z_depth_adjust;
    memset(prim.world_pos, 0, sizeof(prim.world_pos));
    memcpy(prim.world_pos, world_pos, sizeof(prim.world_pos[0]) * corner_count);
    memset(prim.uvw, 0, sizeof(prim.uvw));
    memset(prim.texture_size, 0, sizeof(prim.texture_size));
    if (disp != nullptr) {
        memset(prim.disp, 0, sizeof(prim.disp));
        memcpy(prim.disp, disp, sizeof(prim.disp[0]) * corner_count);
    } else {
        memset(prim.disp, 0, sizeof(prim.disp));
    }
    memset(prim.color, 0, sizeof(prim.color));
    memcpy(prim.color, color, sizeof(prim.color[0]) * corner_count);
    M_ApplyTintToColors(prim.color, corner_count);
    prim.flags = flags;
    Vector_Add(target, &prim);
}

static VECTOR *M_GetScheduledVectorForDrawType(
    M_PRIV *const p, const DRAW_TYPE draw_type)
{
    if (draw_type == DRAW_BLEND_ADD || draw_type == DRAW_REFLECTIVE_BLEND_ADD) {
        return p->scheduled_blend_add;
    }
    if (draw_type == DRAW_BLEND_SUB) {
        return p->scheduled_blend_sub;
    }
    return p->scheduled_transparent;
}

void OutputSource_PolyFX_StageSpriteQuadWorld(
    const int32_t sprite_idx, const XYZ_32 world_pos[4],
    const RGBA_8888 color[4], const DRAW_TYPE draw_type)
{
    OutputSource_PolyFX_StageSpriteQuadWorldDepth(
        sprite_idx, world_pos, color, 0.0f, draw_type);
}

void OutputSource_PolyFX_StageSpriteQuadWorldDepth(
    const int32_t sprite_idx, const XYZ_32 world_pos[4],
    const RGBA_8888 color[4], const float z_depth_adjust,
    const DRAW_TYPE draw_type)
{
    M_PRIV *const p = &m_Priv;
    VECTOR *const target = M_GetScheduledVectorForDrawType(p, draw_type);
    M_StagePrim(
        sprite_idx, 4, &world_pos[0], nullptr, &color[0],
        VERT_NO_LIGHTING | VERT_NO_WIBBLE, z_depth_adjust, target);
}

void OutputSource_PolyFX_StageSpriteTriWorld(
    const int32_t sprite_idx, const XYZ_32 world_pos[3],
    const RGBA_8888 color[3], const DRAW_TYPE draw_type)
{
    OutputSource_PolyFX_StageSpriteTriWorldDepth(
        sprite_idx, world_pos, color, 0.0f, draw_type);
}

void OutputSource_PolyFX_StageSpriteTriWorldDepth(
    const int32_t sprite_idx, const XYZ_32 world_pos[3],
    const RGBA_8888 color[3], const float z_depth_adjust,
    const DRAW_TYPE draw_type)
{
    M_PRIV *const p = &m_Priv;
    VECTOR *const target = M_GetScheduledVectorForDrawType(p, draw_type);
    M_StagePrim(
        sprite_idx, 3, &world_pos[0], nullptr, &color[0],
        VERT_NO_LIGHTING | VERT_NO_WIBBLE, z_depth_adjust, target);
}

void OutputSource_PolyFX_StageQuadExt(
    const int32_t sprite_idx, const XYZ_32 world_pos[4], const float disp[4][2],
    const RGBA_8888 color[4], const uint16_t flags, const DRAW_TYPE draw_type)
{
    M_PRIV *const p = &m_Priv;
    VECTOR *const target = M_GetScheduledVectorForDrawType(p, draw_type);
    M_StagePrim(
        sprite_idx, 4, &world_pos[0], disp, &color[0], flags, 0.0f, target);
}

void OutputSource_PolyFX_StageQuadExtUV(
    const XYZ_32 world_pos[4], const OUTPUT_UVW uvw[4],
    const OUTPUT_TEXTURE_SIZE texture_size[4], const float disp[4][2],
    const RGBA_8888 color[4], const uint16_t flags, const DRAW_TYPE draw_type)
{
    M_PRIV *const p = &m_Priv;
    VECTOR *const target = M_GetScheduledVectorForDrawType(p, draw_type);

    M_PRIM prim;
    prim.sprite_idx = -1;
    prim.use_custom_uv = true;
    prim.corner_count = 4;
    prim.z_depth_adjust = 0.0f;
    memcpy(prim.world_pos, world_pos, sizeof(prim.world_pos));
    memcpy(prim.uvw, uvw, sizeof(prim.uvw));
    memcpy(prim.texture_size, texture_size, sizeof(prim.texture_size));
    if (disp != nullptr) {
        memcpy(prim.disp, disp, sizeof(prim.disp));
    } else {
        memset(prim.disp, 0, sizeof(prim.disp));
    }
    memcpy(prim.color, color, sizeof(prim.color));
    M_ApplyTintToColors(prim.color, 4);
    prim.flags = flags;
    Vector_Add(target, &prim);
}

void OutputSource_PolyFX_StageTriExtUV(
    const XYZ_32 world_pos[3], const OUTPUT_UVW uvw[3],
    const OUTPUT_TEXTURE_SIZE texture_size[3], const float disp[3][2],
    const RGBA_8888 color[3], const uint16_t flags, const DRAW_TYPE draw_type)
{
    M_PRIV *const p = &m_Priv;
    VECTOR *const target = M_GetScheduledVectorForDrawType(p, draw_type);

    M_PRIM prim;
    prim.sprite_idx = -1;
    prim.use_custom_uv = true;
    prim.corner_count = 3;
    prim.z_depth_adjust = 0.0f;
    memset(prim.world_pos, 0, sizeof(prim.world_pos));
    memcpy(prim.world_pos, world_pos, sizeof(world_pos[0]) * 3);
    memset(prim.uvw, 0, sizeof(prim.uvw));
    memcpy(prim.uvw, uvw, sizeof(uvw[0]) * 3);
    memset(prim.texture_size, 0, sizeof(prim.texture_size));
    memcpy(prim.texture_size, texture_size, sizeof(texture_size[0]) * 3);
    if (disp != nullptr) {
        memset(prim.disp, 0, sizeof(prim.disp));
        memcpy(prim.disp, disp, sizeof(disp[0]) * 3);
    } else {
        memset(prim.disp, 0, sizeof(prim.disp));
    }
    memset(prim.color, 0, sizeof(prim.color));
    memcpy(prim.color, color, sizeof(color[0]) * 3);
    M_ApplyTintToColors(prim.color, 3);
    prim.flags = flags;
    Vector_Add(target, &prim);
}

void OutputSource_PolyFX_StageLineSegment(
    const XYZ_32 from, const RGBA_8888 from_color, const XYZ_32 to,
    const RGBA_8888 to_color, const float half_width, const DRAW_TYPE draw_type)
{
    const int64_t zv_mid = (M_GetViewDepth(from) + M_GetViewDepth(to)) / 2;
    const int64_t near_z = Output_GetNearZ();
    const int64_t far_z = Output_GetFarZ();
    if (zv_mid <= near_z || zv_mid >= far_z) {
        return;
    }

    const float delta_x = (float)(to.x - from.x);
    const float delta_y = (float)(to.y - from.y);
    const float delta_z = (float)(to.z - from.z);
    float dir_len =
        sqrtf(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);
    if (dir_len <= 0.00001f) {
        return;
    }
    dir_len = 1.0f / dir_len;

    const float dir_x = delta_x * dir_len;
    const float dir_y = delta_y * dir_len;
    const float dir_z = delta_z * dir_len;

    const XYZ_32 mid = {
        (from.x + to.x) / 2,
        (from.y + to.y) / 2,
        (from.z + to.z) / 2,
    };
    const float vx = (float)(mid.x - g_Camera.pos.x);
    const float vy = (float)(mid.y - g_Camera.pos.y);
    const float vz = (float)(mid.z - g_Camera.pos.z);
    float v_len = sqrtf(vx * vx + vy * vy + vz * vz);
    if (v_len <= 0.00001f) {
        return;
    }
    v_len = 1.0f / v_len;
    const float view_x = vx * v_len;
    const float view_y = vy * v_len;
    const float view_z = vz * v_len;

    float side_x = dir_y * view_z - dir_z * view_y;
    float side_y = dir_z * view_x - dir_x * view_z;
    float side_z = dir_x * view_y - dir_y * view_x;
    float side_len = sqrtf(side_x * side_x + side_y * side_y + side_z * side_z);
    if (side_len <= 0.00001f) {
        side_x = dir_y * 0.0f - dir_z * 1.0f;
        side_y = dir_z * 0.0f - dir_x * 0.0f;
        side_z = dir_x * 1.0f - dir_y * 0.0f;
        side_len = sqrtf(side_x * side_x + side_y * side_y + side_z * side_z);
        if (side_len <= 0.00001f) {
            return;
        }
    }
    side_len = 1.0f / side_len;
    side_x *= side_len;
    side_y *= side_len;
    side_z *= side_len;

    const XYZ_32 world_pos[4] = {
        {
            from.x - (int32_t)lrintf(side_x * half_width),
            from.y - (int32_t)lrintf(side_y * half_width),
            from.z - (int32_t)lrintf(side_z * half_width),
        },
        {
            from.x + (int32_t)lrintf(side_x * half_width),
            from.y + (int32_t)lrintf(side_y * half_width),
            from.z + (int32_t)lrintf(side_z * half_width),
        },
        {
            to.x + (int32_t)lrintf(side_x * half_width),
            to.y + (int32_t)lrintf(side_y * half_width),
            to.z + (int32_t)lrintf(side_z * half_width),
        },
        {
            to.x - (int32_t)lrintf(side_x * half_width),
            to.y - (int32_t)lrintf(side_y * half_width),
            to.z - (int32_t)lrintf(side_z * half_width),
        },
    };
    const RGBA_8888 color[4] = { from_color, from_color, to_color, to_color };
    const float disp[4][2] = {};

    OutputSource_PolyFX_StageQuadExt(
        -1, world_pos, disp, color,
        VERT_FLAT_SHADED | VERT_NO_LIGHTING | VERT_NO_WIBBLE, draw_type);
}

void OutputSource_PolyFX_StageSpark(const SPARK *const spark)
{
    if (spark == nullptr || !spark->on) {
        return;
    }

    DRAW_TYPE draw_type = spark->draw_type;
    const float ratio = Interpolation_GetWorldRate();
    const bool use_current_state =
        (int32_t)spark->s_life - (int32_t)spark->life <= 1;
    const XYZ_32 pos = M_GetSparkRenderPos(spark, ratio);
    const XYZ_32 world_pos[4] = { pos, pos, pos, pos };

    const int64_t zv = M_GetViewDepth(pos);
    const int64_t near_z = Output_GetNearZ();
    const int64_t far_z = Output_GetFarZ();
    if (zv <= near_z || zv >= far_z) {
        return;
    }

    int32_t vpos_z = (int32_t)(zv >> W2V_SHIFT);
    if (vpos_z == 0) {
        vpos_z = 1;
    }

    const RGB_888 render_color = use_current_state
        ? spark->color
        : (RGB_888) {
              .r = (uint8_t)LERP(
                  (int32_t)spark->prev_color.r, (int32_t)spark->color.r, ratio),
              .g = (uint8_t)LERP(
                  (int32_t)spark->prev_color.g, (int32_t)spark->color.g, ratio),
              .b = (uint8_t)LERP(
                  (int32_t)spark->prev_color.b, (int32_t)spark->color.b, ratio),
          };

    const int32_t render_width = use_current_state
        ? (int32_t)spark->size.width
        : (int32_t)LERP(
              (int32_t)spark->prev_size.width, (int32_t)spark->size.width,
              ratio);
    const int32_t render_height = use_current_state
        ? (int32_t)spark->size.height
        : (int32_t)LERP(
              (int32_t)spark->prev_size.height, (int32_t)spark->size.height,
              ratio);
    int32_t sw = render_width;
    int32_t sh = render_height;

    const bool use_sprite = (spark->flags & SPARK_F_SPRITE) != 0U;
    if ((spark->flags & SPARK_F_SCALE) != 0U) {
        const int32_t scalar = spark->scalar;
        sw = (int32_t)(((((int64_t)sw * g_PhdPersp) << scalar) / vpos_z));
        sh = (int32_t)(((((int64_t)sh * g_PhdPersp) << scalar) / vpos_z));

        if (use_sprite) {
            const int32_t max_w = render_width << scalar;
            const int32_t max_h = render_height << scalar;
            int32_t min_wh = 4;
            if ((spark->flags & SPARK_F_ATTACHED_NODE) != 0U
                && spark->node_num == 0U) {
                min_wh = 2;
            }
            CLAMP(sw, min_wh, max_w);
            CLAMP(sh, min_wh, max_h);
        } else {
            const int32_t max_w = render_width << 2;
            const int32_t max_h = render_height << 2;
            CLAMP(sw, 1, max_w);
            CLAMP(sh, 1, max_h);
        }
    }

    const float w = ((sw / 2.0f) * (float)vpos_z) / (float)g_PhdPersp;
    const float h = ((sh / 2.0f) * (float)vpos_z) / (float)g_PhdPersp;
    float disp[4][2] = {
        { -w, -h },
        { -w, h },
        { w, h },
        { w, -h },
    };

    RGBA_8888 color = { render_color.r, render_color.g, render_color.b, 255 };

    if ((spark->flags & SPARK_F_ROTATE) != 0U) {
        const int32_t rot_angle = use_current_state
            ? (int32_t)spark->rot_angle
            : Math_AngleMean(
                  (int32_t)spark->prev_rot_angle, (int32_t)spark->rot_angle,
                  ratio);
        const int32_t angle = rot_angle * DEG_360 / 0xFFF.p0;
        const float s = Math_Sin(angle) / (float)(1 << W2V_SHIFT);
        const float c = Math_Cos(angle) / (float)(1 << W2V_SHIFT);
        for (int32_t i = 0; i < 4; i++) {
            const float x = disp[i][0];
            const float y = disp[i][1];
            disp[i][0] = x * c - y * s;
            disp[i][1] = x * s + y * c;
        }
    }

    uint16_t flags =
        VERT_NO_LIGHTING | VERT_NO_WIBBLE | VERT_BILLBOARD | VERT_ABS_SPRITE;
    int32_t sprite_idx = spark->sprite_idx;
    if ((spark->flags & SPARK_F_SPRITE) == 0U) {
        flags |= VERT_FLAT_SHADED;
        sprite_idx = -1;
        if (draw_type == DRAW_BLEND_ADD
            || draw_type == DRAW_REFLECTIVE_BLEND_ADD) {
            color.a = 128;
        }
        draw_type = DRAW_BLEND;
    }
    M_PRIV *const p = &m_Priv;
    VECTOR *const target = M_GetScheduledVectorForDrawType(p, draw_type);

    const RGBA_8888 world_color[4] = { color, color, color, color };
    M_StagePrim(
        sprite_idx, 4, &world_pos[0], disp, &world_color[0], flags, 0.0f,
        target);
}

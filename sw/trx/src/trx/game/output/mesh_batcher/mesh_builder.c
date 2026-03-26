#include <trx/game/output/mesh_batcher/mesh_builder.h>

#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/output/mesh_batcher/mesh.h>
#include <trx/game/output/shaders/mesh.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/vertex_range.h>

#include <string.h>

struct MESH_BUILDER {
    size_t pending_vertex_count;
    OUTPUT_MESH *mesh;
    VECTOR *indices;
};

static void M_EnsureMesh(MESH_BUILDER *const builder)
{
    if (builder->mesh == nullptr) {
        builder->mesh = Output_Mesh_Create();
        builder->pending_vertex_count = 0;
    }
}

static void M_AddAnimatedVertexRanges(
    OUTPUT_MESH *const mesh, const OUTPUT_MESH_VERTEX *const vertices,
    const size_t vertex_count, const size_t vertex_start)
{
    size_t range_start = 0;
    size_t range_count = 0;

    for (size_t i = 0; i < vertex_count; i++) {
        const bool animated = !(vertices[i].flags & VERT_FLAT_SHADED)
            && Output_Textures_IsObjectTextureAnimated(vertices[i].uvw_idx / 4);
        if (!animated) {
            if (range_count > 0) {
                Vector_Add(
                    mesh->animated_vertices,
                    &(OUTPUT_VERTEX_RANGE) {
                        .vertex_start = vertex_start + range_start,
                        .vertex_count = range_count,
                    });
                range_count = 0;
            }
            continue;
        }

        if (range_count == 0) {
            range_start = i;
        }
        range_count++;
    }

    if (range_count > 0) {
        Vector_Add(
            mesh->animated_vertices,
            &(OUTPUT_VERTEX_RANGE) {
                .vertex_start = vertex_start + range_start,
                .vertex_count = range_count,
            });
    }
}

static void M_FillFanIndices(
    VECTOR *const indices, const size_t vertex_count, const bool double_sided)
{
    ASSERT(vertex_count >= 3);
    const size_t tri_count = vertex_count - 2;
    const size_t index_count = tri_count * 3 * (double_sided ? 2 : 1);
    int32_t *const out = Vector_Expand(indices, index_count);
    for (size_t i = 0, j = 0; i < tri_count; i++) {
        out[j++] = 0;
        out[j++] = i + 2;
        out[j++] = i + 1;

        if (double_sided) {
            out[j++] = i + 1;
            out[j++] = i + 2;
            out[j++] = 0;
        }
    }
}

MESH_BUILDER *MeshBuilder_Create(void)
{
    MESH_BUILDER *const builder = Memory_Alloc(sizeof(*builder));
    builder->indices = Vector_Create(sizeof(int32_t));
    return builder;
}

void MeshBuilder_Destroy(MESH_BUILDER *const builder)
{
    ASSERT(builder != nullptr);
    if (builder->mesh != nullptr) {
        Output_Mesh_Destroy(builder->mesh);
        builder->mesh = nullptr;
    }
    if (builder->indices != nullptr) {
        Vector_Free(builder->indices);
        builder->indices = nullptr;
    }

    Memory_Free(builder);
}

void MeshBuilder_AddVertex(
    MESH_BUILDER *const builder, const OUTPUT_MESH_VERTEX *const vertex)
{
    MeshBuilder_AddVertices(builder, vertex, 1);
}

void MeshBuilder_AddVertices(
    MESH_BUILDER *const builder, const OUTPUT_MESH_VERTEX *const vertices,
    const size_t vertex_count)
{
    ASSERT(builder != nullptr);
    ASSERT(vertex_count > 0);
    M_EnsureMesh(builder);
    ASSERT(builder->mesh != nullptr);
    ASSERT(!builder->mesh->sealed);

    const size_t vertex_start = builder->mesh->vertices->count;
    memcpy(
        Vector_Expand(builder->mesh->vertices, vertex_count), vertices,
        sizeof(OUTPUT_MESH_VERTEX) * vertex_count);
    M_AddAnimatedVertexRanges(
        builder->mesh, vertices, vertex_count, vertex_start);
    builder->pending_vertex_count += vertex_count;
}

void MeshBuilder_AddFace(
    MESH_BUILDER *const builder, const SCENE_PASS pass, const int32_t *indices,
    const size_t idx_count)
{
    ASSERT(builder != nullptr);
    ASSERT(
        (pass == SCENE_PASS_TRANSPARENT) || (pass == SCENE_PASS_OPAQUE)
        || (pass == SCENE_PASS_BLEND_SUB) || (pass == SCENE_PASS_BLEND_ADD));
    M_EnsureMesh(builder);
    ASSERT(builder->mesh != nullptr);
    ASSERT(!builder->mesh->sealed);

    const size_t vtx_count = builder->pending_vertex_count;
    const size_t start = builder->mesh->vertices->count - vtx_count;
    const OUTPUT_MESH_VERTEX *const vbuf =
        Vector_GetData(builder->mesh->vertices);
    XYZ_F centroid = { 0.0f, 0.0f, 0.0f };
    for (size_t i = 0; i < vtx_count; i++) {
        centroid.x += vbuf[start + i].pos.x;
        centroid.y += vbuf[start + i].pos.y;
        centroid.z += vbuf[start + i].pos.z;
    }
    centroid.x /= (float)vtx_count;
    centroid.y /= (float)vtx_count;
    centroid.z /= (float)vtx_count;
    if (pass == SCENE_PASS_TRANSPARENT) {
        OUTPUT_MESH_FACE face = {
            .vertex_count = idx_count,
            .mesh_centroid = centroid,
        };
        face.vertex_indices = Memory_ArenaAlloc(
            &builder->mesh->allocator, sizeof(int32_t) * idx_count);
        for (size_t i = 0; i < idx_count; i++) {
            face.vertex_indices[i] = start + indices[i];
        }
        Vector_Add(builder->mesh->transparent_faces, &face);
    }
    VECTOR *const target = pass == SCENE_PASS_BLEND_ADD
        ? builder->mesh->blend_add_vertex_indices
        : builder->mesh->opaque_vertex_indices;
    uint32_t *const out = Vector_Expand(target, idx_count);
    for (size_t i = 0; i < idx_count; i++) {
        out[i] = start + indices[i];
    }
    builder->pending_vertex_count = 0;
}

void MeshBuilder_AddFan(
    MESH_BUILDER *const builder, const SCENE_PASS pass, const bool double_sided)
{
    ASSERT(builder != nullptr);
    M_EnsureMesh(builder);
    const size_t vtx_count = builder->pending_vertex_count;
    ASSERT(vtx_count >= 3);
    M_FillFanIndices(builder->indices, vtx_count, double_sided);
    MeshBuilder_AddFace(
        builder, pass, Vector_GetData(builder->indices),
        builder->indices->count);
    Vector_Clear(builder->indices);
}

void MeshBuilder_AddRoomSprite(
    MESH_BUILDER *const builder, const ROOM_SPRITE *const room_sprite,
    const ROOM *const room, const float depth_adjust,
    const uint16_t extra_flags)
{
    const int16_t texture_idx = room_sprite->texture;
    const SPRITE_TEXTURE *const sprite = Output_GetSpriteTexture(texture_idx);
    const ROOM_VERTEX *const room_vert =
        &room->mesh.vertices[room_sprite->vertex];
    const XYZ_16 *pos = &room_vert->pos;
    const struct {
        float x, y;
    } normal[4] = {
        { .x = sprite->x0, .y = sprite->y0 },
        { .x = sprite->x1, .y = sprite->y0 },
        { .x = sprite->x1, .y = sprite->y1 },
        { .x = sprite->x0, .y = sprite->y1 },
    };
    for (int32_t j = 0; j < 4; j++) {
        const OUTPUT_MESH_VERTEX vertex = {
            .pos = { .x = pos->x, .y = pos->y, .z = pos->z, .w = depth_adjust },
            .normal = { .x = normal[j].x, .y = normal[j].y, .z = 0.0f },
            .flags = Output_Textures_GetSpriteTextureFlags(texture_idx)
                | extra_flags,
            .color = { 255, 255, 255, 255 },
            .uvw_idx = Output_Textures_GetSpriteUVWIndex(texture_idx, j),
            .shade = room_vert->light_base,
            .trapezoid_ratio = { 1.0f, 1.0f },
        };
        MeshBuilder_AddVertex(builder, &vertex);
    }
    MeshBuilder_AddFan(builder, SCENE_PASS_TRANSPARENT, false);
}

void MeshBuilder_AdjustDepth(MESH_BUILDER *const builder, const float depth)
{
    if (builder->mesh == nullptr || builder->mesh->vertices == nullptr) {
        return;
    }
    OUTPUT_MESH_VERTEX *const vbuf = Vector_GetData(builder->mesh->vertices);
    for (int32_t i = 0; i < builder->mesh->vertices->count; i++) {
        vbuf[i].pos.w = depth;
    }
}

OUTPUT_MESH *MeshBuilder_Seal(MESH_BUILDER *const builder)
{
    ASSERT(builder != nullptr);
    if (builder->mesh == nullptr) {
        return nullptr;
    }
    OUTPUT_MESH *const mesh = builder->mesh;
    Output_GlueVertexRanges(mesh->animated_vertices);
    mesh->sealed = 1;
    builder->mesh = nullptr;
    return mesh;
}

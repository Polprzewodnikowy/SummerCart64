#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/game_buf.h>
#include <trx/game/inject.h>
#include <trx/game/level/context.h>
#include <trx/game/level/sections/append.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/objects.h>

static void M_ReadVertex(XYZ_16 *const vertex, VFILE *const file)
{
    vertex->x = VFile_ReadS16(file);
    vertex->y = VFile_ReadS16(file);
    vertex->z = VFile_ReadS16(file);
}

static void M_ReadFace(
    FACE *const face, const size_t vertex_count, VFILE *const file)
{
    face->vertex_count = vertex_count;
    for (size_t i = 0; i < vertex_count; i++) {
        face->vertices[i] = VFile_ReadU16(file);
        face->texture_zw[i].z = 1.0f;
        face->texture_zw[i].w = 1.0f;
    }
    const uint16_t texture_idx = VFile_ReadU16(file);
    face->texture_idx = texture_idx & 0x7FFF;
    face->double_sided = (texture_idx & 0x8000) != 0;
    face->enable_reflections = false;
}

static void M_ReadObjectMesh(OBJECT_MESH *const mesh, VFILE *const file)
{
    M_ReadVertex(&mesh->center, file);
    mesh->radius = VFile_ReadS16(file);
    VFile_Skip(file, sizeof(int16_t));

    mesh->enable_reflections = false;
    mesh->enable_caustics = false;
    mesh->depth_adjustment = 0.005;

    {
        mesh->num_vertices = VFile_ReadS16(file);
        mesh->vertices =
            GameBuf_Alloc(sizeof(XYZ_16) * mesh->num_vertices, GBUF_MESHES);
        for (int32_t i = 0; i < mesh->num_vertices; i++) {
            M_ReadVertex(&mesh->vertices[i], file);
        }
    }

    {
        mesh->num_lights = VFile_ReadS16(file);
        if (mesh->num_lights > 0) {
            mesh->lighting.normals =
                GameBuf_Alloc(sizeof(XYZ_16) * mesh->num_lights, GBUF_MESHES);
            for (int32_t i = 0; i < mesh->num_lights; i++) {
                M_ReadVertex(&mesh->lighting.normals[i], file);
            }
        } else {
            mesh->lighting.lights = GameBuf_Alloc(
                sizeof(int16_t) * ABS(mesh->num_lights), GBUF_MESHES);
            for (int32_t i = 0; i < ABS(mesh->num_lights); i++) {
                mesh->lighting.lights[i] = VFile_ReadS16(file);
            }
        }
    }

    {
        mesh->tex_face4s.count = VFile_ReadS16(file);
        size_t pos = VFile_GetPos(file);
        VFile_Skip(file, 10 * mesh->tex_face4s.count);
        mesh->tex_face3s.count = VFile_ReadS16(file);
        VFile_Skip(file, 8 * mesh->tex_face3s.count);
        mesh->flat_face4s.count = VFile_ReadS16(file);
        VFile_Skip(file, 10 * mesh->flat_face4s.count);
        mesh->flat_face3s.count = VFile_ReadS16(file);
        VFile_SetPos(file, pos);

        mesh->tex_faces.count = mesh->tex_face4s.count + mesh->tex_face3s.count;
        mesh->flat_faces.count =
            mesh->flat_face4s.count + mesh->flat_face3s.count;
        mesh->all_faces.count = mesh->tex_faces.count + mesh->flat_faces.count;
        FACE *face_ptr =
            GameBuf_Alloc(sizeof(FACE) * mesh->all_faces.count, GBUF_MESHES);

        mesh->all_faces.data = face_ptr;
        mesh->tex_faces.data = face_ptr;
        mesh->tex_face4s.data = face_ptr;
        for (int32_t i = 0; i < mesh->tex_face4s.count; i++) {
            M_ReadFace(face_ptr++, 4, file);
        }
        VFile_Skip(file, 2);

        mesh->tex_face3s.data = face_ptr;
        for (int32_t i = 0; i < mesh->tex_face3s.count; i++) {
            M_ReadFace(face_ptr++, 3, file);
        }
        VFile_Skip(file, 2);

        mesh->flat_faces.data = face_ptr;
        mesh->flat_face4s.data = face_ptr;
        for (int32_t i = 0; i < mesh->flat_face4s.count; i++) {
            M_ReadFace(face_ptr++, 4, file);
        }
        VFile_Skip(file, 2);

        mesh->flat_face3s.data = face_ptr;
        for (int32_t i = 0; i < mesh->flat_face3s.count; i++) {
            M_ReadFace(face_ptr++, 3, file);
        }
        VFile_Skip(file, 2);
    }
}

void Level_Section_ReadObjectMeshes(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_meshes = VFile_ReadS32(file);
    LOG_INFO("object mesh data: %d", num_meshes);

    const size_t data_start_pos = VFile_GetPos(file);
    VFile_Skip(file, num_meshes * sizeof(int16_t));

    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    info->mesh_ptr_count = VFile_ReadS32(file);
    LOG_INFO("object mesh offsets: %d", info->mesh_ptr_count);
    const int32_t alloc_size = info->mesh_ptr_count * sizeof(int32_t);
    int32_t *mesh_offsets = Memory_Alloc(alloc_size);
    VFile_Read(file, mesh_offsets, alloc_size);

    const size_t end_pos = VFile_GetPos(file);
    VFile_SetPos(file, data_start_pos);

    Object_InitialiseMeshes(
        info->mesh_ptr_count + Inject_GetDataCount(IDT_MESH_POINTERS));
    Level_Section_AppendObjectMeshes(info->mesh_ptr_count, mesh_offsets, file);

    VFile_SetPos(file, end_pos);
    Memory_FreePointer(&mesh_offsets);

    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_AppendObjectMeshes(
    const int32_t num_offsets, const int32_t *const offsets, VFILE *const file)
{
#define L_ALIGN 2

    // Savegames identify meshes by their file pointer values divided by 2.
    // (Historically, meshes were stored in int16_t[] arrays, so the so-called
    // "pointers" are really just array indices into that layout.)
    //
    // Original level meshes work fine under this scheme, but injected meshes
    // are different, as they come from separate VFiles and bring their own
    // pointer values. To prevent conflicts, calling
    // Level_Section_AppendObjectMeshes() for injected content must assign
    // unique pseudo-pointers.
    //
    // Rules for injected meshes:
    // - Pointers do not need to match real file offsets.
    // - They only need to be unique and preserve ordering.
    //
    // Only the original level data requires true offset congruence so that old
    // savegames remain compatible. For everything else, simple linear indexing
    // is sufficient.
    int32_t base_index = 0;
    if (Object_GetMeshCount() > 0) {
        // NOTE(Dash): Not assuming offsets are strictly increasing, so we scan
        // all meshes and pick the max.
        for (int32_t i = 0; i < Object_GetMeshCount(); i++) {
            base_index =
                MAX(base_index, Object_GetMeshOffset(Object_GetMesh(i)));
        }
        base_index += L_ALIGN;
    }

    // Construct and store distinct meshes only e.g. Lara's hips are referenced
    // by several pointers as a dummy mesh.
    VECTOR *const unique_offsets =
        Vector_CreateAtCapacity(sizeof(int32_t), num_offsets);
    int32_t pointer_map[num_offsets];
    for (int32_t i = 0; i < num_offsets; i++) {
        const int32_t pointer = offsets[i] + base_index;
        const int32_t index = Vector_IndexOf(unique_offsets, (void *)&pointer);
        if (index == -1) {
            pointer_map[i] = unique_offsets->count;
            Vector_Add(unique_offsets, (void *)&pointer);
        } else {
            pointer_map[i] = index;
        }
    }

    OBJECT_MESH *const meshes =
        GameBuf_Alloc(sizeof(OBJECT_MESH) * unique_offsets->count, GBUF_MESHES);
    size_t start_pos = VFile_GetPos(file);
    for (int32_t i = 0; i < unique_offsets->count; i++) {
        const int32_t pointer = *(const int32_t *)Vector_Get(unique_offsets, i);
        VFile_SetPos(file, start_pos + pointer - base_index);
        M_ReadObjectMesh(&meshes[i], file);

        // The original data position is required for backward compatibility
        // with savegame files, specifically for Lara's mesh pointers.
        Object_SetMeshOffset(&meshes[i], pointer / L_ALIGN);
    }

    for (int32_t i = 0; i < num_offsets; i++) {
        Object_StoreMesh(&meshes[pointer_map[i]]);
    }
#undef L_ALIGN

    LOG_INFO("%d unique meshes constructed", unique_offsets->count);

    Vector_Free(unique_offsets);
}

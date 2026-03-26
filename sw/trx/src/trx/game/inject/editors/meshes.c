#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/inject.h>
#include <trx/game/objects/common.h>

typedef struct {
    INJECTION_OBJECT_INFO obj_info;
    int16_t source_identifier;
    FACE_TYPE face_type;
    int16_t face_index;
    int32_t target_count;
    int16_t *targets;
} FACE_EDIT;

typedef struct {
    int16_t index;
    XYZ_16 shift;
} VERTEX_EDIT;

typedef struct {
    INJECTION_OBJECT_INFO obj_info;
    int16_t mesh_idx;
    XYZ_16 centre_shift;
    int32_t radius_shift;
    int32_t face_edit_count;
    int32_t vertex_edit_count;
    FACE_EDIT *face_edits;
    VERTEX_EDIT *vertex_edits;
} MESH_EDIT;

static BOUNDS_16 M_ReadBounds16(VFILE *const file)
{
    BOUNDS_16 bounds = {};
    bounds.min.x = VFile_ReadS16(file);
    bounds.max.x = VFile_ReadS16(file);
    bounds.min.y = VFile_ReadS16(file);
    bounds.max.y = VFile_ReadS16(file);
    bounds.min.z = VFile_ReadS16(file);
    bounds.max.z = VFile_ReadS16(file);
    return bounds;
}

static uint16_t *M_GetMeshTexture(const FACE_EDIT *const edit)
{
    const OBJECT *const obj = Object_Get(edit->obj_info.id);
    if (!obj->loaded) {
        return nullptr;
    }
    ASSERT(edit->source_identifier >= 0);
    ASSERT(edit->source_identifier < obj->mesh_count);

    const OBJECT_MESH *const mesh =
        Object_GetMesh(obj->mesh_idx + edit->source_identifier);

    if (edit->face_type == FT_TEXTURED_QUAD) {
        ASSERT(edit->face_index >= 0);
        ASSERT(edit->face_index < mesh->tex_face4s.count);
        FACE *const face = &mesh->tex_face4s.data[edit->face_index];
        return &face->texture_idx;
    }
    if (edit->face_type == FT_TEXTURED_TRIANGLE) {
        ASSERT(edit->face_index >= 0);
        ASSERT(edit->face_index < mesh->tex_face3s.count);
        FACE *const face = &mesh->tex_face3s.data[edit->face_index];
        return &face->texture_idx;
    }

    if (edit->face_type == FT_COLOURED_QUAD) {
        ASSERT(edit->face_index >= 0);
        ASSERT(edit->face_index < mesh->flat_face4s.count);
        FACE *const face = &mesh->flat_face4s.data[edit->face_index];
        return &face->palette_idx;
    }
    if (edit->face_type == FT_COLOURED_TRIANGLE) {
        ASSERT(edit->face_index >= 0);
        ASSERT(edit->face_index < mesh->flat_face3s.count);
        FACE *const face = &mesh->flat_face3s.data[edit->face_index];
        return &face->palette_idx;
    }

    return nullptr;
}

static void M_ApplyFaceEdit(
    const FACE_EDIT *const edit, FACE *const faces, const int32_t face_count,
    const uint16_t texture)
{
    for (int32_t i = 0; i < edit->target_count; i++) {
        ASSERT(edit->targets[i] >= 0);
        ASSERT(edit->targets[i] < face_count);
        FACE *const face = &faces[edit->targets[i]];
        face->texture_idx = texture;
    }
}

static void M_ApplyMeshEdit(const MESH_EDIT *const edit)
{
    OBJECT_MESH *mesh;
    if (edit->obj_info.type == OBJ_TYPE_OBJECT) {
        const OBJECT *const obj = Object_Get(edit->obj_info.id);
        if (!obj->loaded) {
            return;
        }
        ASSERT(edit->mesh_idx >= 0);
        ASSERT(edit->mesh_idx < obj->mesh_count);

        mesh = Object_GetMesh(obj->mesh_idx + edit->mesh_idx);
    } else if (edit->obj_info.type == OBJ_TYPE_STATIC3D) {
        if (edit->obj_info.id < 0
            || edit->obj_info.id >= Object_GetStaticObjects3DCount()) {
            return;
        }
        const STATIC_OBJECT_3D *const obj =
            Object_Get3DStatic(edit->obj_info.id);
        mesh = Object_GetMesh(obj->mesh_idx);
    } else {
        LOG_WARNING("Invalid mesh edit type %d", edit->obj_info.type);
        return;
    }

    mesh->center.x += edit->centre_shift.x;
    mesh->center.y += edit->centre_shift.y;
    mesh->center.z += edit->centre_shift.z;
    mesh->radius += edit->radius_shift;

    for (int32_t i = 0; i < edit->vertex_edit_count; i++) {
        const VERTEX_EDIT *const vertex_edit = &edit->vertex_edits[i];
        if (vertex_edit->index < 0
            || vertex_edit->index >= mesh->num_vertices) {
            const int32_t object_id = edit->obj_info.type == OBJ_TYPE_OBJECT
                ? Object_ToGameID(edit->obj_info.id)
                : edit->obj_info.id;
            LOG_ERROR(
                "Invalid mesh vertex edit: obj_type=%d obj_id=%d mesh_idx=%d "
                "vertex_idx=%d num_vertices=%d",
                edit->obj_info.type, object_id, edit->mesh_idx,
                vertex_edit->index, mesh->num_vertices);
        }
        ASSERT(vertex_edit->index >= 0);
        ASSERT(vertex_edit->index < mesh->num_vertices);
        XYZ_16 *const vertex = &mesh->vertices[vertex_edit->index];
        vertex->x += vertex_edit->shift.x;
        vertex->y += vertex_edit->shift.y;
        vertex->z += vertex_edit->shift.z;
    }

    // Find each face we are interested in and replace its texture
    // or palette reference with the one selected from each edit's
    // instructions.
    for (int32_t i = 0; i < edit->face_edit_count; i++) {
        const FACE_EDIT *const face_edit = &edit->face_edits[i];
        uint16_t texture;
        if (face_edit->source_identifier < 0) {
            texture = Inject_GetPaletteIndex(-face_edit->source_identifier);
        } else {
            const uint16_t *const tex_ptr = M_GetMeshTexture(face_edit);
            if (tex_ptr == nullptr) {
                continue;
            }
            texture = *tex_ptr;
        }

        switch (face_edit->face_type) {
        case FT_TEXTURED_QUAD:
            M_ApplyFaceEdit(
                face_edit, mesh->tex_face4s.data, mesh->tex_face4s.count,
                texture);
            break;
        case FT_TEXTURED_TRIANGLE:
            M_ApplyFaceEdit(
                face_edit, mesh->tex_face3s.data, mesh->tex_face3s.count,
                texture);
            break;
        case FT_COLOURED_QUAD:
            M_ApplyFaceEdit(
                face_edit, mesh->flat_face4s.data, mesh->flat_face4s.count,
                texture);
            break;
        case FT_COLOURED_TRIANGLE:
            M_ApplyFaceEdit(
                face_edit, mesh->flat_face3s.data, mesh->flat_face3s.count,
                texture);
            break;
        }
    }
}

static void M_MeshEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        MESH_EDIT edit = {
            .obj_info = Inject_ReadObjectPtr(injection),
            .mesh_idx = VFile_ReadS16(injection->fp),
            .centre_shift.x = VFile_ReadS16(injection->fp),
            .centre_shift.y = VFile_ReadS16(injection->fp),
            .centre_shift.z = VFile_ReadS16(injection->fp),
            .radius_shift = VFile_ReadS32(injection->fp),
        };

        edit.face_edit_count = VFile_ReadS32(injection->fp);
        edit.face_edits =
            Memory_Alloc(sizeof(FACE_EDIT) * edit.face_edit_count);
        for (int32_t j = 0; j < edit.face_edit_count; j++) {
            FACE_EDIT *const face_edit = &edit.face_edits[j];
            face_edit->obj_info = Inject_ReadObjectPtr(injection);
            face_edit->source_identifier = VFile_ReadS16(injection->fp);
            face_edit->face_type = VFile_ReadS32(injection->fp);
            face_edit->face_index = VFile_ReadS16(injection->fp);

            face_edit->target_count = VFile_ReadS32(injection->fp);
            face_edit->targets =
                Memory_Alloc(sizeof(int16_t) * face_edit->target_count);
            VFile_Read(
                injection->fp, face_edit->targets,
                sizeof(int16_t) * face_edit->target_count);
        }

        edit.vertex_edit_count = VFile_ReadS32(injection->fp);
        edit.vertex_edits =
            Memory_Alloc(sizeof(VERTEX_EDIT) * edit.vertex_edit_count);
        for (int32_t j = 0; j < edit.vertex_edit_count; j++) {
            VERTEX_EDIT *vertex_edit = &edit.vertex_edits[j];
            vertex_edit->index = VFile_ReadS16(injection->fp);
            vertex_edit->shift.x = VFile_ReadS16(injection->fp);
            vertex_edit->shift.y = VFile_ReadS16(injection->fp);
            vertex_edit->shift.z = VFile_ReadS16(injection->fp);
        }

        if (ctx->mode != INJECTION_MODE_STATS) {
            M_ApplyMeshEdit(&edit);
        }

        for (int32_t j = 0; j < edit.face_edit_count; j++) {
            FACE_EDIT *face_edit = &edit.face_edits[j];
            Memory_FreePointer(&face_edit->targets);
        }

        Memory_FreePointer(&edit.face_edits);
        Memory_FreePointer(&edit.vertex_edits);
    }
}

static void M_Object3DEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        const int32_t obj_id = VFile_ReadS32(injection->fp);
        const bool collidable = VFile_ReadU8(injection->fp) == 1;
        const bool visible = VFile_ReadU8(injection->fp) == 1;
        const BOUNDS_16 collision_bounds = M_ReadBounds16(injection->fp);
        const BOUNDS_16 draw_bounds = M_ReadBounds16(injection->fp);

        if (obj_id < 0 || obj_id >= Object_GetStaticObjects3DCount()) {
            continue;
        }
        STATIC_OBJECT_3D *const obj = Object_Get3DStatic(obj_id);
        if (!obj->loaded) {
            continue;
        }

        obj->collidable = collidable;
        obj->visible = visible;
        obj->collision_bounds = collision_bounds;
        obj->draw_bounds = draw_bounds;
    }
}

REGISTER_INJECT_EDITOR(IDT_MESH_EDITS, M_MeshEdits)
REGISTER_INJECT_EDITOR(IDT_OBJECT_3D_EDITS, M_Object3DEdits)

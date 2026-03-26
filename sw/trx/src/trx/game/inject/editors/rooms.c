#include <trx/core/log.h>
#include <trx/game/inject.h>
#include <trx/game/objects.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

static uint16_t *M_GetRoomTexture(
    const int16_t room_num, const FACE_TYPE face_type, const int16_t face_index)
{
    const ROOM *const room = Room_Get(room_num);
    if (face_type == FT_TEXTURED_QUAD && face_index < room->mesh.face4s.count) {
        FACE *const face = &room->mesh.face4s.data[face_index];
        return &face->texture_idx;
    }

    if (face_type == FT_TEXTURED_TRIANGLE
        && face_index < room->mesh.face3s.count) {
        FACE *const face = &room->mesh.face3s.data[face_index];
        return &face->texture_idx;
    }

    LOG_WARNING(
        "Invalid room face lookup: %d, %d, %d", room_num, face_type,
        face_index);
    return nullptr;
}

static void M_TextureRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE target_face_type = VFile_ReadS32(injection->fp);
    const int16_t target_face = VFile_ReadS16(injection->fp);
    const int16_t source_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE source_face_type = VFile_ReadS32(injection->fp);
    const int16_t source_face = VFile_ReadS16(injection->fp);

    const uint16_t *const source_texture =
        M_GetRoomTexture(source_room, source_face_type, source_face);
    uint16_t *const target_texture =
        M_GetRoomTexture(target_room, target_face_type, target_face);
    if (source_texture != nullptr && target_texture != nullptr) {
        *target_texture = *source_texture;
    }
}

static uint16_t *M_GetRoomFaceVertices(
    const int16_t room_num, const FACE_TYPE face_type, const int16_t face_index)
{
    if (room_num < 0 || room_num >= Room_GetCount()) {
        LOG_WARNING("Room index %d is invalid", room_num);
        return nullptr;
    }

    const ROOM *const room = Room_Get(room_num);
    if (face_type == FT_TEXTURED_QUAD) {
        if (face_index < 0 || face_index >= room->mesh.face4s.count) {
            LOG_WARNING(
                "Face4 index %d, room %d is invalid", face_index, room_num);
            return nullptr;
        }

        FACE *const face = &room->mesh.face4s.data[face_index];
        return (uint16_t *)(void *)&face->vertices;
    }

    if (face_type == FT_TEXTURED_TRIANGLE) {
        if (face_index < 0 || face_index >= room->mesh.face3s.count) {
            LOG_WARNING(
                "Face3 index %d, room %d is invalid", face_index, room_num);
            return nullptr;
        }

        FACE *const face = &room->mesh.face3s.data[face_index];
        return (uint16_t *)(void *)&face->vertices;
    }

    return nullptr;
}

static void M_MoveRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE face_type = VFile_ReadS32(injection->fp);
    const int16_t target_face = VFile_ReadS16(injection->fp);
    const int32_t vertex_count = VFile_ReadS32(injection->fp);

    for (int32_t j = 0; j < vertex_count; j++) {
        const int16_t vertex_index = VFile_ReadS16(injection->fp);
        const int16_t new_vertex = VFile_ReadS16(injection->fp);

        uint16_t *const vertices =
            M_GetRoomFaceVertices(target_room, face_type, target_face);
        if (vertices != nullptr) {
            vertices[vertex_index] = new_vertex;
        }
    }
}

static void M_AlterRoomVertex(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const int16_t target_vertex = VFile_ReadS16(injection->fp);
    const int16_t x_change = VFile_ReadS16(injection->fp);
    const int16_t y_change = VFile_ReadS16(injection->fp);
    const int16_t z_change = VFile_ReadS16(injection->fp);
    const int16_t shade_change = VFile_ReadS16(injection->fp);

    if (target_room < 0 || target_room >= Room_GetCount()) {
        LOG_WARNING("Room index %d is invalid", target_room);
        return;
    }

    const ROOM *const room = Room_Get(target_room);
    if (target_vertex < 0 || target_vertex >= room->mesh.num_vertices) {
        LOG_WARNING(
            "Vertex index %d, room %d is invalid", target_vertex, target_room);
        return;
    }

    ROOM_VERTEX *const vertex = &room->mesh.vertices[target_vertex];
    vertex->pos.x += x_change;
    vertex->pos.y += y_change;
    vertex->pos.z += z_change;
    vertex->light_base += shade_change;
}

static void M_SetVertexFlags(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const int16_t target_vertex = VFile_ReadS16(injection->fp);
    const uint16_t flags = VFile_ReadU16(injection->fp);

    if (target_room < 0 || target_room >= Room_GetCount()) {
        LOG_WARNING("Room index %d is invalid", target_room);
        return;
    }

    const ROOM *const room = Room_Get(target_room);
    if (target_vertex < 0 || target_vertex >= room->mesh.num_vertices) {
        LOG_WARNING(
            "Vertex index %d, room %d is invalid", target_vertex, target_room);
        return;
    }

    ROOM_VERTEX *const vertex = &room->mesh.vertices[target_vertex];
    if (g_TRVersion == 1) {
        vertex->flags.disable_wibble = (flags & 0x2000u) != 0u;
    } else {
        vertex->flags.disable_wibble = (flags & 0x8000u) != 0u;
        vertex->light_table_value = flags & 0xFF;
    }
}

static void M_RotateRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE face_type = VFile_ReadS32(injection->fp);
    const int16_t target_face = VFile_ReadS16(injection->fp);
    const uint8_t num_rotations = VFile_ReadU8(injection->fp);

    uint16_t *const face_vertices =
        M_GetRoomFaceVertices(target_room, face_type, target_face);
    if (face_vertices == nullptr) {
        return;
    }

    const int32_t num_vertices = face_type == FT_TEXTURED_QUAD ? 4 : 3;
    uint16_t *vertices[num_vertices];
    for (int32_t i = 0; i < num_vertices; i++) {
        vertices[i] = face_vertices + i;
    }

    for (int32_t i = 0; i < num_rotations; i++) {
        const uint16_t first = *vertices[0];
        for (int32_t j = 0; j < num_vertices - 1; j++) {
            *vertices[j] = *vertices[j + 1];
        }
        *vertices[num_vertices - 1] = first;
    }
}

static void M_AddRoomFace(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    const FACE_TYPE face_type = VFile_ReadS32(injection->fp);
    const int16_t source_room = VFile_ReadS16(injection->fp);
    const int16_t source_face = VFile_ReadS16(injection->fp);

    const int32_t num_vertices = face_type == FT_TEXTURED_QUAD ? 4 : 3;
    uint16_t vertices[num_vertices];
    for (int32_t i = 0; i < num_vertices; i++) {
        vertices[i] = VFile_ReadU16(injection->fp);
    }

    if (target_room < 0 || target_room >= Room_GetCount()) {
        LOG_WARNING("Room index %d is invalid", target_room);
        return;
    }

    const uint16_t *const source_texture =
        M_GetRoomTexture(source_room, face_type, source_face);
    if (source_texture == nullptr) {
        return;
    }

    ROOM *const room = Room_Get(target_room);
    uint16_t *face_vertices;
    if (face_type == FT_TEXTURED_QUAD) {
        FACE *const face = &room->mesh.face4s.data[room->mesh.face4s.count];
        face->texture_idx = *source_texture;
        for (int32_t i = 0; i < face->vertex_count; i++) {
            face->texture_zw[i].z = 1.0f;
            face->texture_zw[i].w = 1.0f;
        }
        face_vertices = face->vertices;
        room->mesh.face4s.count++;
    } else {
        FACE *const face = &room->mesh.face3s.data[room->mesh.face3s.count];
        face->vertex_count = 3;
        for (int32_t i = 0; i < face->vertex_count; i++) {
            face->texture_zw[i].z = 1.0f;
            face->texture_zw[i].w = 1.0f;
        }
        face->texture_idx = *source_texture;
        face_vertices = face->vertices;
        room->mesh.face3s.count++;
    }

    for (int32_t i = 0; i < num_vertices; i++) {
        face_vertices[i] = vertices[i];
    }
}

static void M_AddRoomVertex(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const XYZ_16 pos = {
        .x = VFile_ReadS16(injection->fp),
        .y = VFile_ReadS16(injection->fp),
        .z = VFile_ReadS16(injection->fp),
    };
    int16_t shade = 0;
    RGBA_8888 color = COLOR_RGBA_8888_WHITE;
    if (g_TRVersion < 3) {
        shade = VFile_ReadS16(injection->fp);
    } else {
        color = Color_ARGB1555ToRGBA8888(VFile_ReadU16(injection->fp));
        color.a = 255;
    }

    ROOM *const room = Room_Get(target_room);
    ROOM_VERTEX *const vertex = &room->mesh.vertices[room->mesh.num_vertices];
    vertex->pos = pos;
    vertex->light_base = shade;
    vertex->flags.disable_wibble = false;
    vertex->flags.move = false;
    vertex->flags.glow = false;
    vertex->light_table_value = 0;
    vertex->color = color;
    room->mesh.num_vertices++;
}

static void M_AddRoomStatic2D(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const int32_t id = VFile_ReadS32(injection->fp);
    const uint16_t vertex = VFile_ReadU16(injection->fp);
    const uint16_t frame_idx = VFile_ReadU16(injection->fp);

    const STATIC_OBJECT_2D *const obj = Object_Get2DStatic(id);
    if (obj == nullptr) {
        LOG_WARNING("Invalid static 2D id: %d", id);
        return;
    }
    if (!obj->loaded) {
        LOG_WARNING("Static 2D %d is not loaded", id);
        return;
    }

    if (frame_idx >= obj->frame_count) {
        LOG_WARNING("Invalid frame (%d) on static 2D %d", frame_idx, id);
        return;
    }

    ROOM *const room = Room_Get(target_room);
    ROOM_SPRITE *const sprite =
        &room->mesh.sprites.data[room->mesh.sprites.count];
    sprite->vertex = vertex;
    sprite->texture = obj->texture_idx + frame_idx;
    room->mesh.sprites.count++;
}

static void M_AddRoomStatic3D(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    ROOM *const room = Room_Get(target_room);

    STATIC_MESH *const mesh = &room->static_meshes[room->num_static_meshes];
    mesh->pos.x = VFile_ReadS32(injection->fp);
    mesh->pos.y = VFile_ReadS32(injection->fp);
    mesh->pos.z = VFile_ReadS32(injection->fp);
    mesh->rot.y = VFile_ReadS16(injection->fp);
    mesh->shade.value_1 = VFile_ReadS16(injection->fp);
    if (g_TRVersion >= 2) {
        mesh->shade.value_2 = mesh->shade.value_1;
    }
    mesh->static_num = VFile_ReadS16(injection->fp);

    room->num_static_meshes++;
}

static void M_EditRoomStatic3D(const INJECTION *const injection)
{
    const int16_t target_room = VFile_ReadS16(injection->fp);
    VFile_Skip(injection->fp, sizeof(int32_t));
    const ROOM *const room = Room_Get(target_room);
    const int32_t mesh_idx = VFile_ReadS32(injection->fp);
    if (mesh_idx < 0 || mesh_idx >= room->num_static_meshes) {
        LOG_WARNING(
            "Invalid static mesh index (%d) for room %d", mesh_idx,
            target_room);
        VFile_Skip(injection->fp, 4 * sizeof(int32_t));
        return;
    }

    STATIC_MESH *const mesh = &room->static_meshes[mesh_idx];
    mesh->pos.x = VFile_ReadS32(injection->fp);
    mesh->pos.y = VFile_ReadS32(injection->fp);
    mesh->pos.z = VFile_ReadS32(injection->fp);
    mesh->rot.y = VFile_ReadS16(injection->fp);
    mesh->shade.value_1 = VFile_ReadS16(injection->fp);
    if (g_TRVersion >= 2) {
        mesh->shade.value_2 = mesh->shade.value_1;
    }
}

static void M_RoomMeshEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        const ROOM_MESH_EDIT_TYPE type = VFile_ReadS32(injection->fp);
        switch (type) {
        case RMET_TEXTURE_FACE:
            M_TextureRoomFace(injection);
            break;
        case RMET_MOVE_FACE:
            M_MoveRoomFace(injection);
            break;
        case RMET_ALTER_VERTEX:
            M_AlterRoomVertex(injection);
            break;
        case RMET_VERTEX_FLAGS:
            M_SetVertexFlags(injection);
            break;
        case RMET_ROTATE_FACE:
            M_RotateRoomFace(injection);
            break;
        case RMET_ADD_FACE:
            M_AddRoomFace(injection);
            break;
        case RMET_ADD_VERTEX:
            M_AddRoomVertex(injection);
            break;
        case RMET_ADD_STATIC_2D:
            M_AddRoomStatic2D(injection);
            break;
        case RMET_ADD_STATIC_3D:
            M_AddRoomStatic3D(injection);
            break;
        case RMET_EDIT_STATIC_3D:
            M_EditRoomStatic3D(injection);
            break;
        default:
            LOG_WARNING("Unrecognised room mesh edit type: %d", type);
            break;
        }
    }
}

static void M_RoomPortalEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        const int16_t base_room = VFile_ReadS16(injection->fp);
        const int16_t link_room = VFile_ReadS16(injection->fp);
        const int16_t portal_index = VFile_ReadS16(injection->fp);

        if (base_room < 0 || base_room >= Room_GetCount()) {
            VFile_Skip(injection->fp, sizeof(int16_t) * 12);
            LOG_WARNING("Room index %d is invalid", base_room);
            continue;
        }

        const ROOM *const room = Room_Get(base_room);
        PORTAL *portal = nullptr;
        for (int32_t j = 0; j < room->portals->count; j++) {
            const PORTAL room_portal = room->portals->portal[j];
            if (room_portal.room_num == link_room && j == portal_index) {
                portal = &room->portals->portal[j];
                break;
            }
        }

        if (portal == nullptr) {
            VFile_Skip(injection->fp, sizeof(int16_t) * 12);
            LOG_WARNING(
                "Room index %d has no matching portal to %d", base_room,
                link_room);
            continue;
        }

        bool empty_portal = true;
        for (int32_t j = 0; j < 4; j++) {
            portal->vertex[j].x += VFile_ReadS16(injection->fp);
            portal->vertex[j].y += VFile_ReadS16(injection->fp);
            portal->vertex[j].z += VFile_ReadS16(injection->fp);
            empty_portal &= portal->vertex[j].x == 0 && portal->vertex[j].y == 0
                && portal->vertex[j].z == 0;
        }

        // An injection that has reset all vertices such that the portal size is
        // now 0, should be interpreted as a command to ignore that portal.
        if (empty_portal) {
            for (int32_t j = portal_index + 1; j < room->portals->count; j++) {
                room->portals->portal[j - 1] = room->portals->portal[j];
            }
            room->portals->portal[room->portals->count - 1] = *portal;
            room->portals->count--;
        }
    }
}

REGISTER_INJECT_EDITOR(IDT_ROOM_EDITS, M_RoomMeshEdits)
REGISTER_INJECT_EDITOR(IDT_VIS_PORTAL_EDITS, M_RoomPortalEdits)

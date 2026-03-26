#include <trx/core/benchmark.h>
#include <trx/core/colors.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/effects.h>
#include <trx/game/game_buf.h>
#include <trx/game/inject.h>
#include <trx/game/level/format/format.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/output/bind.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>
#include <trx/game/shell.h>
#include <trx/game/viewport.h>

#define M_NO_ROOM_LEGACY 255
#define M_NO_BOX_TR3_LEGACY 0x7FF

static void M_ReadPosition(XYZ_32 *const pos, VFILE *const file)
{
    pos->x = VFile_ReadS32(file);
    pos->y = VFile_ReadS32(file);
    pos->z = VFile_ReadS32(file);
}

static void M_ReadVertex(XYZ_16 *const vertex, VFILE *const file)
{
    vertex->x = VFile_ReadS16(file);
    vertex->y = VFile_ReadS16(file);
    vertex->z = VFile_ReadS16(file);
}

static void M_ReadShade(
    const LEVEL_FORMAT_LOADER *const loader, SHADE *const shade,
    VFILE *const file)
{
    shade->value_1 = VFile_ReadS16(file);
    if (loader->game_version == 1) {
        shade->value_2 = shade->value_1;
    } else {
        shade->value_2 = VFile_ReadS16(file);
    }
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

static void M_ReadRoomMesh(
    const LEVEL_FORMAT_LOADER *const loader, const int32_t room_num,
    VFILE *const file, const INJECTION_MESH_META inj_data)
{
    ROOM *const room = Room_Get(room_num);
    const uint32_t mesh_length = VFile_ReadU32(file);
    const size_t start_pos = VFile_GetPos(file);

    {
        room->mesh.num_vertices = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.num_vertices + inj_data.num_vertices;
        room->mesh.vertices =
            GameBuf_Alloc(sizeof(ROOM_VERTEX) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.num_vertices; i++) {
            ROOM_VERTEX *const vertex = &room->mesh.vertices[i];
            M_ReadVertex(&vertex->pos, file);
            if (loader->game_version == 1) {
                vertex->light_base = VFile_ReadS16(file);
                vertex->flags.disable_wibble = false;
                vertex->flags.move = false;
                vertex->flags.glow = false;
                vertex->color = COLOR_RGBA_8888_WHITE;
            } else if (loader->game_version == 2) {
                vertex->light_base = VFile_ReadS16(file);
                vertex->light_table_value = VFile_ReadU8(file);
                const uint8_t flags = VFile_ReadU8(file);
                vertex->flags.disable_wibble = (flags & 0x80u) != 0u;
                vertex->flags.move = false;
                vertex->flags.glow = false;
                VFile_Skip(file, 2);
                vertex->color = COLOR_RGBA_8888_WHITE;
            } else if (loader->game_version == 3) {
                VFile_Skip(file, 2); // lighting - unused in TR3
                const uint16_t flags = VFile_ReadU16(file);
                vertex->flags.disable_wibble = (flags & 0x8000u) != 0u;
                vertex->flags.move = (flags & 0x2000u) != 0u;
                vertex->flags.glow = (flags & 0x4000u) != 0u;
                vertex->color = Color_ARGB1555ToRGBA8888(VFile_ReadU16(file));
                vertex->color.a = 255;
                vertex->light_base = 0;
            }
        }
    }

    {
        room->mesh.face4s.count = VFile_ReadS16(file);
        const size_t pos = VFile_GetPos(file);
        VFile_Skip(file, 10 * room->mesh.face4s.count);
        room->mesh.face3s.count = VFile_ReadS16(file);
        VFile_SetPos(file, pos);

        room->mesh.all_faces.count = room->mesh.face4s.count
            + inj_data.num_quads + room->mesh.face3s.count
            + inj_data.num_triangles;
        FACE *face_ptr = GameBuf_Alloc(
            sizeof(FACE) * room->mesh.all_faces.count, GBUF_ROOM_MESH);

        room->mesh.all_faces.data = face_ptr;
        room->mesh.face4s.data = face_ptr;
        for (int32_t i = 0; i < room->mesh.face4s.count; i++) {
            M_ReadFace(face_ptr++, 4, file);
        }
        for (int32_t i = 0; i < inj_data.num_quads; i++) {
            face_ptr->vertex_count = 4;
            face_ptr++;
        }

        VFile_Skip(file, 2);

        room->mesh.face3s.data = face_ptr;
        for (int32_t i = 0; i < room->mesh.face3s.count; i++) {
            M_ReadFace(face_ptr++, 3, file);
        }
        for (int32_t i = 0; i < inj_data.num_triangles; i++) {
            face_ptr->vertex_count = 3;
            face_ptr++;
        }
    }

    {
        room->mesh.sprites.count = VFile_ReadS16(file);
        const int32_t alloc_count =
            room->mesh.sprites.count + inj_data.num_static_2ds;
        room->mesh.sprites.data =
            GameBuf_Alloc(sizeof(ROOM_SPRITE) * alloc_count, GBUF_ROOM_MESH);
        for (int32_t i = 0; i < room->mesh.sprites.count; i++) {
            ROOM_SPRITE *const sprite = &room->mesh.sprites.data[i];
            sprite->vertex = VFile_ReadU16(file);
            sprite->texture = VFile_ReadU16(file);
        }
    }

    const size_t total_read =
        (VFile_GetPos(file) - start_pos) / sizeof(int16_t);
    ASSERT(total_read == mesh_length);
}

static XYZ_16 M_ComputePortalNormal(PORTAL *const p)
{
    // This fixes a bug in TombEditor where certain portals would get emitted
    // with wrong normals. TE is guaranteed to emit normals with a good sign in
    // the Y component, but for sloped ceiling portals, their X and Z
    // compontents have the wrong sign.
    //
    // To fix this, we compute the normal the regular way. We don't know which
    // way the portal faces, but since the Y component is guaranteed to be
    // good, we can orient our vector using this information, which should fix
    // the X/Z components.

    ASSERT(p != nullptr);

    // Geometric normal (ab x ac)
    const XYZ_32 a = { p->vertex[0].x, p->vertex[0].y, p->vertex[0].z };
    const XYZ_32 b = { p->vertex[1].x, p->vertex[1].y, p->vertex[1].z };
    const XYZ_32 c = { p->vertex[2].x, p->vertex[2].y, p->vertex[2].z };
    const XYZ_32 ab = { b.x - a.x, b.y - a.y, b.z - a.z };
    const XYZ_32 ac = { c.x - a.x, c.y - a.y, c.z - a.z };
    XYZ_32 n = {
        (ab.y * ac.z) - (ab.z * ac.y),
        (ab.z * ac.x) - (ab.x * ac.z),
        (ab.x * ac.y) - (ab.y * ac.x),
    };

    // Degenerate guard
    if (n.x == 0 && n.y == 0 && n.z == 0) {
        return (XYZ_16) { .x = 0, .y = 1, .z = 0 };
    }

    // Integer normalization
    const int32_t gx = ABS(n.x);
    const int32_t gy = ABS(n.y);
    const int32_t gz = ABS(n.z);
    int32_t g = gx;
    if (gy != 0) {
        g = Math_GCD(g, gy);
    }
    if (gz != 0) {
        g = Math_GCD(g, gz);
    }
    if (g == 0) {
        g = 1;
    }
    n.x /= g;
    n.y /= g;
    n.z /= g;

    // NOTE: we only care about horizontal portals.
    if (p->normal.y == 0) {
        return p->normal;
    }
    if (p->normal.y != n.y) {
        n.x *= -1;
        n.y *= -1;
        n.z *= -1;
    }
    return (XYZ_16) { n.x, n.y, n.z };
}

void Level_Section_ReadRooms(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;

    const int32_t num_rooms = VFile_ReadS16(file);
    LOG_INFO("rooms: %d", num_rooms);
    if (num_rooms > MAX_ROOMS) {
        Shell_ExitSystem("Too many rooms");
        goto finish;
    }

    Room_InitialiseRooms(num_rooms);
    for (int32_t i = 0; i < num_rooms; i++) {
        ROOM *const room = Room_Get(i);

        room->pos.x = VFile_ReadS32(file);
        room->pos.y = 0;
        room->pos.z = VFile_ReadS32(file);

        room->min_floor = VFile_ReadS32(file);
        room->max_ceiling = VFile_ReadS32(file);

        const INJECTION_MESH_META inj_data = Inject_GetRoomMeshMeta(i);
        M_ReadRoomMesh(loader, i, file, inj_data);

        const int16_t num_portals = VFile_ReadS16(file);
        if (num_portals <= 0) {
            room->portals = nullptr;
        } else {
            room->portals = GameBuf_Alloc(
                sizeof(PORTAL) * num_portals + sizeof(PORTALS),
                GBUF_ROOM_PORTALS);
            room->portals->count = num_portals;
            for (int32_t j = 0; j < num_portals; j++) {
                PORTAL *const portal = &room->portals->portal[j];
                portal->room_num = VFile_ReadS16(file);
                M_ReadVertex(&portal->normal, file);
                for (int32_t k = 0; k < 4; k++) {
                    M_ReadVertex(&portal->vertex[k], file);
                }
            }
        }

        room->size.z = VFile_ReadS16(file);
        room->size.x = VFile_ReadS16(file);

        const int32_t sector_count = room->size.x * room->size.z;
        room->sectors =
            GameBuf_Alloc(sizeof(SECTOR) * sector_count, GBUF_ROOM_SECTORS);
        for (int32_t j = 0; j < sector_count; j++) {
            SECTOR *const sector = &room->sectors[j];
            sector->idx = VFile_ReadU16(file);
            if (loader->game_version == 3) {
                uint16_t misc_info = VFile_ReadU16(file);
                sector->fx = (uint8_t)(misc_info & 0x0F);
                sector->box = (int16_t)((misc_info & 0x7FF0) >> 4);
                sector->stopper = (bool)((misc_info & 0x8000) >> 15);
                if (sector->box == M_NO_BOX_TR3_LEGACY) {
                    sector->box = NO_BOX;
                }
            } else {
                sector->fx = 0;
                sector->box = VFile_ReadS16(file);
                sector->stopper = false;
            }
            sector->portal_room.pit = VFile_ReadU8(file);
            sector->floor.height = VFile_ReadS8(file) * STEP_L;
            sector->portal_room.sky = VFile_ReadU8(file);
            sector->ceiling.height = VFile_ReadS8(file) * STEP_L;
            if (sector->portal_room.pit == M_NO_ROOM_LEGACY) {
                sector->portal_room.pit = NO_ROOM;
            }
            if (sector->portal_room.sky == M_NO_ROOM_LEGACY) {
                sector->portal_room.sky = NO_ROOM;
            }
        }

        room->ambient = VFile_ReadS16(file);
        if (loader->game_version == 1) {
            room->light_mode = RLM_NORMAL;
        } else if (loader->game_version == 2) {
            VFile_Skip(file, sizeof(int16_t)); // Unused second ambient
            room->light_mode = VFile_ReadS16(file);
        } else {
            room->light_mode = VFile_ReadS16(file);
        }

        room->num_lights = VFile_ReadS16(file);
        room->lights = room->num_lights == 0
            ? nullptr
            : GameBuf_Alloc(sizeof(LIGHT) * room->num_lights, GBUF_ROOM_LIGHTS);
        for (int32_t j = 0; j < room->num_lights; j++) {
            LIGHT *const light = &room->lights[j];
            if (loader->game_version == 3) {
                // TR3 room lights use the LIGHT_INFO struct layout:
                // pos (s32*3) + rgb (u8*3) + type (u8) + union (8 bytes).
                M_ReadPosition(&light->pos, file);
                light->color.r = VFile_ReadU8(file);
                light->color.g = VFile_ReadU8(file);
                light->color.b = VFile_ReadU8(file);
                light->type = VFile_ReadU8(file);
                if (light->type != 0u) {
                    light->dir.x = VFile_ReadS16(file);
                    light->dir.y = VFile_ReadS16(file);
                    light->dir.z = VFile_ReadS16(file);
                    VFile_Skip(file, sizeof(int16_t)); // pad
                    light->shade.value_1 = 0;
                    light->shade.value_2 = 0;
                    light->falloff.value_1 = 0;
                    light->falloff.value_2 = 0;
                } else {
                    int32_t intensity = VFile_ReadS32(file);
                    const int32_t falloff = VFile_ReadS32(file);
                    CLAMP(intensity, INT16_MIN, INT16_MAX);
                    light->shade.value_1 = (int16_t)intensity;
                    light->shade.value_2 = (int16_t)intensity;
                    light->falloff.value_1 = falloff;
                    light->falloff.value_2 = falloff;
                    light->dir = (XYZ_16) { 0, 0, 0 };
                }
            } else {
                M_ReadPosition(&light->pos, file);
                M_ReadShade(loader, &light->shade, file);
                light->falloff.value_1 = VFile_ReadS32(file);
                if (loader->game_version >= 2) {
                    light->falloff.value_2 = VFile_ReadS32(file);
                } else {
                    light->falloff.value_2 = light->falloff.value_1;
                }
                light->color = COLOR_RGB_888_WHITE;
                light->type = 0;
                light->dir = (XYZ_16) { 0, 0, 0 };
            }
        }

        room->num_static_meshes = VFile_ReadS16(file);
        const int32_t static_count =
            room->num_static_meshes + inj_data.num_static_3ds;
        room->static_meshes = static_count == 0
            ? nullptr
            : GameBuf_Alloc(
                  sizeof(STATIC_MESH) * static_count, GBUF_ROOM_STATIC_MESHES);
        for (int32_t j = 0; j < room->num_static_meshes; j++) {
            STATIC_MESH *const mesh = &room->static_meshes[j];
            M_ReadPosition(&mesh->pos, file);
            mesh->rot.y = VFile_ReadS16(file);
            M_ReadShade(loader, &mesh->shade, file);
            mesh->static_num = VFile_ReadS16(file);
        }

        room->flipped_room = VFile_ReadS16(file);

        const uint16_t flags = VFile_ReadU16(file);
        // clang-format off
        room->flags.underwater  = (flags & 0x01) != 0;
        room->flags.outside     = (flags & 0x08) != 0;
        room->flags.dynamic_lit = (flags & 0x10) != 0;
        room->flags.wind        = (flags & 0x20) != 0;
        room->flags.inside      = (flags & 0x40) != 0;
        room->flags.swamp       = (flags & 0x80) != 0;
        // clang-format on

        OUTPUT_ROOM_BIND *const bind = Output_Bind_GetRoom(room);
        bind->bound_left = Viewport_GetMaxX(VIEWPORT_GAME);
        bind->bound_top = Viewport_GetMaxY(VIEWPORT_GAME);
        bind->bound_bottom = Viewport_GetMinY(VIEWPORT_GAME);
        bind->bound_right = Viewport_GetMinX(VIEWPORT_GAME);
        room->item_num = NO_ITEM;
        room->effect_num = NO_EFFECT;

        if (loader->game_version == 3) {
            room->water_scheme = VFile_ReadU8(file);
            room->reverb_info = VFile_ReadU8(file);
            VFile_Skip(file, 1);
        }
    }

    for (int32_t i = 0; i < num_rooms; i++) {
        ROOM *const room = Room_Get(i);
        if (room->portals == nullptr) {
            continue;
        }
        for (int32_t j = 0; j < room->portals->count; j++) {
            PORTAL *const portal = &room->portals->portal[j];
            const XYZ_16 new_normal = M_ComputePortalNormal(portal);
            if (new_normal.x != portal->normal.x
                || new_normal.y != portal->normal.y
                || new_normal.z != portal->normal.z) {
                LOG_WARNING("Fixed room %d, portal normal %d", i, j);
                portal->normal = new_normal;
            }
        }
    }

    Room_InitialiseFlipStatus();

    const int32_t floor_data_size = VFile_ReadS32(file);
    int16_t *floor_data = Memory_Alloc(sizeof(int16_t) * floor_data_size);
    VFile_Read(file, floor_data, sizeof(int16_t) * floor_data_size);

    Room_ParseFloorData(floor_data);
    Memory_FreePointer(&floor_data);

    Room_BuildOutsideTable();

finish:
    Benchmark_End(&benchmark, nullptr);
}

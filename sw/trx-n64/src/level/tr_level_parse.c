/*
 * TRX-N64: TR1 .PHD level file parser
 *
 * Binary format reference:
 *   https://opentomb.github.io/TRosettaStone3/trosettastone.html
 *   Cross-referenced with TRX src/trx/game/level/sections/*.c
 *
 * File is little-endian (native on original PC target).
 * N64 is big-endian, so we byte-swap all multi-byte reads.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tr_level_parse.h"
#include "../platform/n64_platform.h"

#include <string.h>

/* External debug position array (defined in main) */
extern uint32_t g_parse_pos[16];
extern int g_parse_count;
#define MARK_POS(r) do { if (g_parse_count < 16) g_parse_pos[g_parse_count++] = (r)->pos; } while(0)

/* -------------------------------------------------------------------
 * Reader helpers (little-endian → big-endian swap)
 * ------------------------------------------------------------------- */

static inline bool r_has(tr_reader_t *r, uint32_t bytes) {
    return (r->pos + bytes) <= r->size;
}

static inline uint8_t r_u8(tr_reader_t *r) {
    if (!r_has(r, 1)) return 0;
    return r->data[r->pos++];
}

static inline int8_t r_s8(tr_reader_t *r) {
    return (int8_t)r_u8(r);
}

static inline uint16_t r_u16(tr_reader_t *r) {
    if (!r_has(r, 2)) return 0;
    uint16_t v = r->data[r->pos]
               | ((uint16_t)r->data[r->pos + 1] << 8);
    r->pos += 2;
    return v;
}

static inline int16_t r_s16(tr_reader_t *r) {
    return (int16_t)r_u16(r);
}

static inline uint32_t r_u32(tr_reader_t *r) {
    if (!r_has(r, 4)) return 0;
    uint32_t v = r->data[r->pos]
               | ((uint32_t)r->data[r->pos + 1] << 8)
               | ((uint32_t)r->data[r->pos + 2] << 16)
               | ((uint32_t)r->data[r->pos + 3] << 24);
    r->pos += 4;
    return v;
}

static inline int32_t r_s32(tr_reader_t *r) {
    return (int32_t)r_u32(r);
}

static inline void r_skip(tr_reader_t *r, uint32_t bytes) {
    r->pos += bytes;
    if (r->pos > r->size) r->pos = r->size;
}

static inline void r_read(tr_reader_t *r, void *buf, uint32_t bytes) {
    if (!r_has(r, bytes)) { memset(buf, 0, bytes); return; }
    memcpy(buf, r->data + r->pos, bytes);
    r->pos += bytes;
}

static inline tr_xyz16_t r_xyz16(tr_reader_t *r) {
    tr_xyz16_t v;
    v.x = r_s16(r);
    v.y = r_s16(r);
    v.z = r_s16(r);
    return v;
}

static inline tr_xyz32_t r_xyz32(tr_reader_t *r) {
    tr_xyz32_t v;
    v.x = r_s32(r);
    v.y = r_s32(r);
    v.z = r_s32(r);
    return v;
}

/* -------------------------------------------------------------------
 * Memory allocation (uses n64_mem_alloc bump allocator)
 * ------------------------------------------------------------------- */

static void *alloc(uint32_t size) {
    void *p = n64_mem_alloc(size);
    if (p) memset(p, 0, size);
    return p;
}

#define ALLOC_ARRAY(type, count) ((type *)alloc(sizeof(type) * (count)))

/* -------------------------------------------------------------------
 * Section parsers
 * ------------------------------------------------------------------- */

static tr_parse_result_t parse_rooms(tr_reader_t *r, tr_level_t *level) {
    level->room_count = r_s16(r);
    if (level->room_count > TR_MAX_ROOMS)
        return TR_PARSE_TOO_MANY_ROOMS;

    level->rooms = ALLOC_ARRAY(tr_room_t, level->room_count);
    if (!level->rooms) return TR_PARSE_OUT_OF_MEMORY;

    for (int i = 0; i < level->room_count; i++) {
        tr_room_t *room = &level->rooms[i];

        /* Room position */
        room->x = r_s32(r);
        room->z = r_s32(r);
        room->min_floor = r_s32(r);
        room->max_ceiling = r_s32(r);

        /* Room mesh data */
        uint32_t mesh_data_len = r_u32(r);  /* In S16 words */
        (void)mesh_data_len;

        /* Vertices */
        room->mesh.vertex_count = r_s16(r);
        room->mesh.vertices = ALLOC_ARRAY(tr_room_vertex_t, room->mesh.vertex_count);
        if (!room->mesh.vertices && room->mesh.vertex_count > 0)
            return TR_PARSE_OUT_OF_MEMORY;

        for (int v = 0; v < room->mesh.vertex_count; v++) {
            room->mesh.vertices[v].x = r_s16(r);
            room->mesh.vertices[v].y = r_s16(r);
            room->mesh.vertices[v].z = r_s16(r);
            room->mesh.vertices[v].light = r_s16(r);
        }

        /* Quad faces (4 vertices + texture) */
        int16_t num_quads = r_s16(r);
        /* Tri faces counted after quads */
        tr_face_t *quad_buf = NULL;
        if (num_quads > 0) {
            quad_buf = ALLOC_ARRAY(tr_face_t, num_quads);
            if (!quad_buf) return TR_PARSE_OUT_OF_MEMORY;
        }
        for (int f = 0; f < num_quads; f++) {
            quad_buf[f].vertices[0] = r_u16(r);
            quad_buf[f].vertices[1] = r_u16(r);
            quad_buf[f].vertices[2] = r_u16(r);
            quad_buf[f].vertices[3] = r_u16(r);
            quad_buf[f].texture = r_u16(r);
            quad_buf[f].vertex_count = 4;
            quad_buf[f].flags = 0;
        }

        int16_t num_tris = r_s16(r);
        tr_face_t *tri_buf = NULL;
        if (num_tris > 0) {
            tri_buf = ALLOC_ARRAY(tr_face_t, num_tris);
            if (!tri_buf) return TR_PARSE_OUT_OF_MEMORY;
        }
        for (int f = 0; f < num_tris; f++) {
            tri_buf[f].vertices[0] = r_u16(r);
            tri_buf[f].vertices[1] = r_u16(r);
            tri_buf[f].vertices[2] = r_u16(r);
            tri_buf[f].vertices[3] = tri_buf[f].vertices[2]; /* Degenerate */
            tri_buf[f].texture = r_u16(r);
            tri_buf[f].vertex_count = 3;
            tri_buf[f].flags = 0;
        }

        /* Merge quads + tris into single face array */
        room->mesh.face_count = num_quads + num_tris;
        room->mesh.faces = ALLOC_ARRAY(tr_face_t, room->mesh.face_count);
        if (!room->mesh.faces && room->mesh.face_count > 0)
            return TR_PARSE_OUT_OF_MEMORY;
        if (num_quads > 0)
            memcpy(room->mesh.faces, quad_buf,
                   num_quads * sizeof(tr_face_t));
        if (num_tris > 0)
            memcpy(room->mesh.faces + num_quads, tri_buf,
                   num_tris * sizeof(tr_face_t));

        /* Sprites */
        room->mesh.sprite_count = r_s16(r);
        if (room->mesh.sprite_count > 0) {
            room->mesh.sprite_vertices = ALLOC_ARRAY(uint16_t, room->mesh.sprite_count);
            room->mesh.sprite_textures = ALLOC_ARRAY(uint16_t, room->mesh.sprite_count);
            for (int s = 0; s < room->mesh.sprite_count; s++) {
                room->mesh.sprite_vertices[s] = r_u16(r);
                room->mesh.sprite_textures[s] = r_u16(r);
            }
        }

        /* Portals */
        room->portal_count = r_s16(r);
        room->portals = ALLOC_ARRAY(tr_portal_t, room->portal_count);
        if (!room->portals && room->portal_count > 0)
            return TR_PARSE_OUT_OF_MEMORY;
        for (int p = 0; p < room->portal_count; p++) {
            room->portals[p].room = r_s16(r);
            room->portals[p].normal = r_xyz16(r);
            for (int v = 0; v < 4; v++)
                room->portals[p].vertices[v] = r_xyz16(r);
        }

        /* Sectors */
        room->sectors_z = r_s16(r);
        room->sectors_x = r_s16(r);
        int sector_count = room->sectors_z * room->sectors_x;
        room->sectors = ALLOC_ARRAY(tr_sector_t, sector_count);
        if (!room->sectors && sector_count > 0)
            return TR_PARSE_OUT_OF_MEMORY;
        for (int s = 0; s < sector_count; s++) {
            room->sectors[s].floor_data_idx = r_u16(r);
            room->sectors[s].box = r_s16(r);
            room->sectors[s].pit_room = r_u8(r);
            room->sectors[s].floor = r_s8(r);
            room->sectors[s].sky_room = r_u8(r);
            room->sectors[s].ceiling = r_s8(r);
        }

        /* Lights */
        room->ambient = r_s16(r);
        room->light_count = r_s16(r);
        room->lights = ALLOC_ARRAY(tr_light_t, room->light_count);
        for (int l = 0; l < room->light_count; l++) {
            room->lights[l].pos = r_xyz32(r);
            room->lights[l].intensity = r_s16(r);
            room->lights[l].falloff = r_s32(r);
        }

        /* Static meshes */
        room->static_mesh_count = r_s16(r);
        room->static_meshes = ALLOC_ARRAY(tr_room_static_mesh_t, room->static_mesh_count);
        for (int m = 0; m < room->static_mesh_count; m++) {
            room->static_meshes[m].pos = r_xyz32(r);
            room->static_meshes[m].rotation = r_s16(r);
            room->static_meshes[m].shade = r_s16(r);
            room->static_meshes[m].static_mesh_id = r_s16(r);
        }

        /* Room properties */
        room->flipped_room = r_s16(r);
        room->flags = r_u16(r);
    }

    /* Floor data (after all rooms) */
    level->floor_data_size = r_s32(r);
    level->floor_data = ALLOC_ARRAY(int16_t, level->floor_data_size);
    if (!level->floor_data && level->floor_data_size > 0)
        return TR_PARSE_OUT_OF_MEMORY;
    for (int i = 0; i < level->floor_data_size; i++) {
        level->floor_data[i] = r_s16(r);
    }

    return TR_PARSE_OK;
}

static tr_parse_result_t parse_meshes(tr_reader_t *r, tr_level_t *level) {
    /* Mesh data blob (raw S16 values, indexed by mesh pointers) */
    int32_t mesh_data_count = r_s32(r);
    uint32_t mesh_blob_start = r->pos;
    uint8_t *mesh_blob = (uint8_t *)(r->data + r->pos);
    r_skip(r, mesh_data_count * 2);  /* Skip raw data, parse via pointers */

    /* Mesh pointers (offsets into the blob) */
    int32_t mesh_ptr_count = r_s32(r);
    int32_t *mesh_offsets = ALLOC_ARRAY(int32_t, mesh_ptr_count);
    if (!mesh_offsets && mesh_ptr_count > 0) return TR_PARSE_OUT_OF_MEMORY;
    for (int i = 0; i < mesh_ptr_count; i++) {
        mesh_offsets[i] = r_s32(r);
    }

    /* Parse each mesh from the blob */
    level->mesh_count = mesh_ptr_count;
    if (level->mesh_count > TR_MAX_MESHES)
        level->mesh_count = TR_MAX_MESHES;
    level->meshes = ALLOC_ARRAY(tr_object_mesh_t, level->mesh_count);
    if (!level->meshes) return TR_PARSE_OUT_OF_MEMORY;

    for (int m = 0; m < level->mesh_count; m++) {
        tr_reader_t mr = {
            .data = mesh_blob,
            .size = mesh_data_count * 2,
            .pos = mesh_offsets[m],
        };
        tr_object_mesh_t *mesh = &level->meshes[m];

        mesh->center = r_xyz16(&mr);
        mesh->radius = r_s16(&mr);
        r_skip(&mr, 2);  /* Unused */

        mesh->vertex_count = r_s16(&mr);
        mesh->vertices = ALLOC_ARRAY(tr_xyz16_t, mesh->vertex_count);
        for (int v = 0; v < mesh->vertex_count; v++) {
            mesh->vertices[v] = r_xyz16(&mr);
        }

        mesh->num_lights = r_s16(&mr);
        int light_abs = mesh->num_lights < 0 ? -mesh->num_lights : mesh->num_lights;
        if (mesh->num_lights > 0) {
            mesh->normals = ALLOC_ARRAY(tr_xyz16_t, light_abs);
            for (int n = 0; n < light_abs; n++) {
                mesh->normals[n] = r_xyz16(&mr);
            }
        } else if (mesh->num_lights < 0) {
            mesh->lights = ALLOC_ARRAY(int16_t, light_abs);
            for (int l = 0; l < light_abs; l++) {
                mesh->lights[l] = r_s16(&mr);
            }
        }

        /* Textured quads */
        int16_t tex_quad_count = r_s16(&mr);
        tr_face_t *tex_quads = NULL;
        if (tex_quad_count > 0) {
            tex_quads = ALLOC_ARRAY(tr_face_t, tex_quad_count);
            for (int f = 0; f < tex_quad_count; f++) {
                tex_quads[f].vertices[0] = r_u16(&mr);
                tex_quads[f].vertices[1] = r_u16(&mr);
                tex_quads[f].vertices[2] = r_u16(&mr);
                tex_quads[f].vertices[3] = r_u16(&mr);
                tex_quads[f].texture = r_u16(&mr);
                tex_quads[f].vertex_count = 4;
                tex_quads[f].flags = 0;
            }
        }

        /* Textured tris */
        int16_t tex_tri_count = r_s16(&mr);
        tr_face_t *tex_tris = NULL;
        if (tex_tri_count > 0) {
            tex_tris = ALLOC_ARRAY(tr_face_t, tex_tri_count);
            for (int f = 0; f < tex_tri_count; f++) {
                tex_tris[f].vertices[0] = r_u16(&mr);
                tex_tris[f].vertices[1] = r_u16(&mr);
                tex_tris[f].vertices[2] = r_u16(&mr);
                tex_tris[f].vertices[3] = tex_tris[f].vertices[2];
                tex_tris[f].texture = r_u16(&mr);
                tex_tris[f].vertex_count = 3;
                tex_tris[f].flags = 0;
            }
        }

        /* Flat quads (colored, not textured) */
        int16_t flat_quad_count = r_s16(&mr);
        tr_face_t *flat_quads = NULL;
        if (flat_quad_count > 0) {
            flat_quads = ALLOC_ARRAY(tr_face_t, flat_quad_count);
            for (int f = 0; f < flat_quad_count; f++) {
                flat_quads[f].vertices[0] = r_u16(&mr);
                flat_quads[f].vertices[1] = r_u16(&mr);
                flat_quads[f].vertices[2] = r_u16(&mr);
                flat_quads[f].vertices[3] = r_u16(&mr);
                flat_quads[f].texture = r_u16(&mr) | 0x8000; /* Flag as flat */
                flat_quads[f].vertex_count = 4;
                flat_quads[f].flags = 0;
            }
        }

        /* Flat tris */
        int16_t flat_tri_count = r_s16(&mr);
        tr_face_t *flat_tris = NULL;
        if (flat_tri_count > 0) {
            flat_tris = ALLOC_ARRAY(tr_face_t, flat_tri_count);
            for (int f = 0; f < flat_tri_count; f++) {
                flat_tris[f].vertices[0] = r_u16(&mr);
                flat_tris[f].vertices[1] = r_u16(&mr);
                flat_tris[f].vertices[2] = r_u16(&mr);
                flat_tris[f].vertices[3] = flat_tris[f].vertices[2];
                flat_tris[f].texture = r_u16(&mr) | 0x8000;
                flat_tris[f].vertex_count = 3;
                flat_tris[f].flags = 0;
            }
        }

        /* Merge all faces (textured first, then flat-colored) */
        mesh->textured_face_count = tex_quad_count + tex_tri_count;
        mesh->face_count = tex_quad_count + tex_tri_count +
                           flat_quad_count + flat_tri_count;
        mesh->faces = ALLOC_ARRAY(tr_face_t, mesh->face_count);
        int offset = 0;
        if (tex_quad_count > 0) {
            memcpy(mesh->faces + offset, tex_quads, tex_quad_count * sizeof(tr_face_t));
            offset += tex_quad_count;
        }
        if (tex_tri_count > 0) {
            memcpy(mesh->faces + offset, tex_tris, tex_tri_count * sizeof(tr_face_t));
            offset += tex_tri_count;
        }
        if (flat_quad_count > 0) {
            memcpy(mesh->faces + offset, flat_quads, flat_quad_count * sizeof(tr_face_t));
            offset += flat_quad_count;
        }
        if (flat_tri_count > 0) {
            memcpy(mesh->faces + offset, flat_tris, flat_tri_count * sizeof(tr_face_t));
            offset += flat_tri_count;
        }
    }

    return TR_PARSE_OK;
}

static tr_parse_result_t parse_anims(tr_reader_t *r, tr_level_t *level) {
    /* Animations */
    level->anim_count = r_s32(r);
    level->anims = ALLOC_ARRAY(tr_anim_t, level->anim_count);
    if (!level->anims && level->anim_count > 0) return TR_PARSE_OUT_OF_MEMORY;
    for (int i = 0; i < level->anim_count; i++) {
        level->anims[i].frame_offset = r_u32(r);
        level->anims[i].interpolation = r_u8(r);
        level->anims[i].frame_size = r_u8(r);
        level->anims[i].state_id = r_s16(r);
        level->anims[i].velocity = r_s32(r);
        level->anims[i].acceleration = r_s32(r);
        level->anims[i].frame_base = r_s16(r);
        level->anims[i].frame_end = r_s16(r);
        level->anims[i].jump_anim = r_s16(r);
        level->anims[i].jump_frame = r_s16(r);
        level->anims[i].num_changes = r_s16(r);
        level->anims[i].change_idx = r_s16(r);
        level->anims[i].num_commands = r_s16(r);
        level->anims[i].command_idx = r_s16(r);
    }

    /* Animation changes */
    level->anim_change_count = r_s32(r);
    level->anim_changes = ALLOC_ARRAY(tr_anim_change_t, level->anim_change_count);
    for (int i = 0; i < level->anim_change_count; i++) {
        level->anim_changes[i].goal_state = r_s16(r);
        level->anim_changes[i].num_ranges = r_s16(r);
        level->anim_changes[i].range_idx = r_s16(r);
    }

    /* Animation ranges */
    level->anim_range_count = r_s32(r);
    level->anim_ranges = ALLOC_ARRAY(tr_anim_range_t, level->anim_range_count);
    for (int i = 0; i < level->anim_range_count; i++) {
        level->anim_ranges[i].start_frame = r_s16(r);
        level->anim_ranges[i].end_frame = r_s16(r);
        level->anim_ranges[i].link_anim = r_s16(r);
        level->anim_ranges[i].link_frame = r_s16(r);
    }

    /* Animation commands (raw S16 data) */
    level->anim_command_count = r_s32(r);
    level->anim_commands = ALLOC_ARRAY(int16_t, level->anim_command_count);
    for (int i = 0; i < level->anim_command_count; i++) {
        level->anim_commands[i] = r_s16(r);
    }

    /* Animation bones */
    int32_t bone_data_size = r_s32(r);  /* In S32 units */
    level->anim_bone_count = bone_data_size / 4;
    level->anim_bones = ALLOC_ARRAY(tr_anim_bone_t, level->anim_bone_count);
    for (int i = 0; i < level->anim_bone_count; i++) {
        level->anim_bones[i].flags = r_s32(r);
        level->anim_bones[i].x = r_s32(r);
        level->anim_bones[i].y = r_s32(r);
        level->anim_bones[i].z = r_s32(r);
    }

    /* Animation frames (raw S16 data) */
    level->anim_frame_count = r_s32(r);
    level->anim_frames = ALLOC_ARRAY(int16_t, level->anim_frame_count);
    for (int i = 0; i < level->anim_frame_count; i++) {
        level->anim_frames[i] = r_s16(r);
    }

    return TR_PARSE_OK;
}

static tr_parse_result_t parse_objects(tr_reader_t *r, tr_level_t *level) {
    /* Object definitions */
    level->object_count = r_s32(r);
    level->objects = ALLOC_ARRAY(tr_object_def_t, level->object_count);
    for (int i = 0; i < level->object_count; i++) {
        level->objects[i].object_id = r_s32(r);
        level->objects[i].mesh_count = r_s16(r);
        level->objects[i].mesh_idx = r_s16(r);
        level->objects[i].bone_idx = r_s32(r);
        level->objects[i].frame_offset = r_u32(r);
        level->objects[i].anim_idx = r_s16(r);
    }

    /* Static objects */
    level->static_object_count = r_s32(r);
    level->static_objects = ALLOC_ARRAY(tr_static_object_t, level->static_object_count);
    for (int i = 0; i < level->static_object_count; i++) {
        level->static_objects[i].object_id = r_s32(r);
        level->static_objects[i].mesh_idx = r_s16(r);
        level->static_objects[i].draw_min.x = r_s16(r);
        level->static_objects[i].draw_max.x = r_s16(r);
        level->static_objects[i].draw_min.y = r_s16(r);
        level->static_objects[i].draw_max.y = r_s16(r);
        level->static_objects[i].draw_min.z = r_s16(r);
        level->static_objects[i].draw_max.z = r_s16(r);
        level->static_objects[i].coll_min.x = r_s16(r);
        level->static_objects[i].coll_max.x = r_s16(r);
        level->static_objects[i].coll_min.y = r_s16(r);
        level->static_objects[i].coll_max.y = r_s16(r);
        level->static_objects[i].coll_min.z = r_s16(r);
        level->static_objects[i].coll_max.z = r_s16(r);
        level->static_objects[i].flags = r_u16(r);
    }

    return TR_PARSE_OK;
}

static tr_parse_result_t parse_textures(tr_reader_t *r, tr_level_t *level) {
    /* Object textures */
    level->object_texture_count = r_s32(r);
    level->object_textures = ALLOC_ARRAY(tr_object_texture_t, level->object_texture_count);
    if (!level->object_textures && level->object_texture_count > 0)
        return TR_PARSE_OUT_OF_MEMORY;
    for (int i = 0; i < level->object_texture_count; i++) {
        level->object_textures[i].draw_type = r_u16(r);
        level->object_textures[i].tex_page = r_u16(r);
        for (int v = 0; v < 4; v++) {
            level->object_textures[i].uv[v].u = r_u16(r);
            level->object_textures[i].uv[v].v = r_u16(r);
        }
    }

    /* Sprite textures */
    level->sprite_texture_count = r_s32(r);
    level->sprite_textures = ALLOC_ARRAY(tr_sprite_texture_t, level->sprite_texture_count);
    for (int i = 0; i < level->sprite_texture_count; i++) {
        level->sprite_textures[i].tex_page = r_u16(r);
        level->sprite_textures[i].offset = r_u16(r);
        level->sprite_textures[i].width = r_u16(r);
        level->sprite_textures[i].height = r_u16(r);
        level->sprite_textures[i].x0 = r_s16(r);
        level->sprite_textures[i].y0 = r_s16(r);
        level->sprite_textures[i].x1 = r_s16(r);
        level->sprite_textures[i].y1 = r_s16(r);
    }

    /* Sprite sequences */
    level->sprite_sequence_count = r_s32(r);
    level->sprite_sequences = ALLOC_ARRAY(tr_sprite_sequence_t, level->sprite_sequence_count);
    for (int i = 0; i < level->sprite_sequence_count; i++) {
        level->sprite_sequences[i].object_id = r_s32(r);
        level->sprite_sequences[i].frame_count = r_s16(r);
        level->sprite_sequences[i].texture_idx = r_s16(r);
    }

    return TR_PARSE_OK;
}

static tr_parse_result_t parse_items(tr_reader_t *r, tr_level_t *level) {
    level->item_count = r_s32(r);
    if (level->item_count > TR_MAX_ITEMS)
        return TR_PARSE_TOO_MANY_ITEMS;

    level->items = ALLOC_ARRAY(tr_item_t, level->item_count);
    if (!level->items && level->item_count > 0)
        return TR_PARSE_OUT_OF_MEMORY;

    for (int i = 0; i < level->item_count; i++) {
        level->items[i].object_id = r_s16(r);
        level->items[i].room = r_s16(r);
        level->items[i].pos = r_xyz32(r);
        level->items[i].rotation = r_s16(r);
        level->items[i].shade = r_s16(r);
        level->items[i].flags = r_u16(r);
    }

    return TR_PARSE_OK;
}

static void parse_palette_and_lightmap(tr_reader_t *r, tr_level_t *level) {
    /* Light map: 32 × 256 bytes */
    r_read(r, level->light_map, 32 * 256);

    /* Palette: 256 × RGB888 */
    r_read(r, level->palette, 256 * 3);
}

static void parse_texture_pages(tr_reader_t *r, tr_level_t *level) {
    /*
     * TR1 texture pages are 256×256 8-bit indexed.
     * They appear at the beginning of the file (after version + count)
     * but we read them from the saved offset.
     */
    level->tex_page_count = 0;  /* Set by caller from header */
    if (level->tex_page_count > 0) {
        uint32_t page_size = 256 * 256;
        level->tex_pages = alloc(page_size * level->tex_page_count);
        if (level->tex_pages) {
            r_read(r, level->tex_pages, page_size * level->tex_page_count);
        }
    }
}

/* -------------------------------------------------------------------
 * Main parse entry point
 * ------------------------------------------------------------------- */

tr_parse_result_t tr_level_parse(const uint8_t *level_data,
                                  uint32_t level_size,
                                  tr_level_t *level) {
    memset(level, 0, sizeof(*level));

    tr_reader_t r = {
        .data = level_data,
        .size = level_size,
        .pos = 0,
    };

    /* Version */
    uint32_t version = r_u32(&r);
    if (version != 32) {
        return TR_PARSE_BAD_VERSION;
    }

    /* Texture pages are at the start of the file — read count, skip data.
     * We'll come back for them after palette is read. */
    level->tex_page_count = r_s32(&r);
    uint32_t tex_pages_offset = r.pos;
    uint32_t tex_pages_size = level->tex_page_count * 256 * 256;
    r_skip(&r, tex_pages_size);

    /* File level number (unused) */
    r_s32(&r);

    /* Rooms + floor data */
    tr_parse_result_t res;
    g_parse_count = 0;
    MARK_POS(&r);  /* [0] rooms */
    res = parse_rooms(&r, level);
    if (res != TR_PARSE_OK) return res;

    /* Object meshes */
    MARK_POS(&r);  /* [1] meshes */
    res = parse_meshes(&r, level);
    if (res != TR_PARSE_OK) return res;

    /* Animations (all sub-sections) */
    MARK_POS(&r);  /* [2] anims */
    res = parse_anims(&r, level);
    if (res != TR_PARSE_OK) return res;

    /* Objects + static objects */
    MARK_POS(&r);  /* [3] objects */
    res = parse_objects(&r, level);
    if (res != TR_PARSE_OK) return res;

    /* Textures, sprites, sprite sequences */
    MARK_POS(&r);  /* [4] textures */
    res = parse_textures(&r, level);
    if (res != TR_PARSE_OK) return res;

    /* Cameras + sinks (skip for now) */
    MARK_POS(&r);  /* [5] cameras */
    int32_t num_cameras = r_s32(&r);
    r_skip(&r, num_cameras * 16);

    /* Sound sources (skip) */
    MARK_POS(&r);  /* [6] sound_src */
    int32_t num_sound_sources = r_s32(&r);
    r_skip(&r, num_sound_sources * 16);

    /* Pathfinding boxes (skip) */
    MARK_POS(&r);  /* [7] boxes */
    int32_t num_boxes = r_s32(&r);
    r_skip(&r, num_boxes * 20);  /* TR1: 20 bytes per box */
    int32_t num_overlaps = r_s32(&r);
    r_skip(&r, num_overlaps * 2);
    /* Zone data: 6 arrays × num_boxes × int16
     * TR1: GroundZone1, GroundZone2, FlyZone, then alt versions = 6 arrays */
    r_skip(&r, 6 * num_boxes * 2);

    /* Animated texture ranges (skip for now) */
    MARK_POS(&r);  /* [8] anim_tex */
    int32_t anim_tex_size = r_s32(&r);
    r_skip(&r, anim_tex_size * 2);

    /* Items */
    MARK_POS(&r);  /* [9] items */
    res = parse_items(&r, level);
    if (res != TR_PARSE_OK) return res;

    /* Light map + palette */
    MARK_POS(&r);  /* [10] lightmap */
    parse_palette_and_lightmap(&r, level);

    /* Cinematic frames (skip) */
    MARK_POS(&r);  /* [11] cine */
    int16_t num_cine = r_s16(&r);
    r_skip(&r, num_cine * 16);

    /* Demo data (skip) */
    MARK_POS(&r);  /* [12] demo */
    uint16_t demo_size = r_u16(&r);
    r_skip(&r, demo_size);

    /* Sound map: 256 entries mapping effect ID → sample info index */
    MARK_POS(&r);  /* [13] sound_map */
    for (int i = 0; i < 256; i++)
        level->sound_map[i] = r_s16(&r);

    /* Sound details (sample info entries): 8 bytes each */
    MARK_POS(&r);  /* [14] snd_details */
    level->sound_detail_count = r_s32(&r);
    level->sound_details = alloc(level->sound_detail_count * 8);
    if (level->sound_details) {
        for (int i = 0; i < level->sound_detail_count * 4; i++)
            ((int16_t *)level->sound_details)[i] = r_s16(&r);
    } else {
        r_skip(&r, level->sound_detail_count * 8);
    }

    /* TR1 sample data: uint32 byte count + concatenated RIFF/WAV chunks.
     * There are NO separate sample indices in TR1 .PHD files —
     * we scan for RIFF headers to build the index ourselves. */
    MARK_POS(&r);  /* [15] sample_data */
    level->sample_data_size = r_s32(&r);
    if (level->sample_data_size > 0 && level->sample_data_size < 2*1024*1024 &&
        r_has(&r, level->sample_data_size)) {
        level->sample_data = alloc(level->sample_data_size);
        if (level->sample_data) {
            r_read(&r, level->sample_data, level->sample_data_size);
            /* Scan for RIFF headers to build sample indices */
            int max_samples = 256;
            level->sample_indices = ALLOC_ARRAY(int32_t, max_samples);
            level->sample_index_count = 0;
            if (level->sample_indices) {
                uint32_t pos = 0;
                while (pos + 8 < (uint32_t)level->sample_data_size &&
                       level->sample_index_count < max_samples) {
                    uint8_t *p = level->sample_data + pos;
                    if (p[0]=='R' && p[1]=='I' && p[2]=='F' && p[3]=='F') {
                        level->sample_indices[level->sample_index_count++] = pos;
                        uint32_t chunk_size = p[4]|(p[5]<<8)|(p[6]<<16)|(p[7]<<24);
                        pos += 8 + chunk_size;
                    } else {
                        break;
                    }
                }
            }
        }
    } else {
        level->sample_data = NULL;
        level->sample_data_size = 0;
    }

    /* Point tex_pages directly at the source data in level_data —
     * no copy.  Saves ~704 KB of heap (11 pages × 64 KB).
     * Safe because level_data lives in the bump heap (never freed). */
    if (level->tex_page_count > 0 &&
        tex_pages_offset + (uint32_t)(256 * 256 * level->tex_page_count) <= level_size) {
        level->tex_pages = (uint8_t *)(level_data + tex_pages_offset);
    }

    return TR_PARSE_OK;
}

const char *tr_parse_error_string(tr_parse_result_t result) {
    switch (result) {
        case TR_PARSE_OK:               return "OK";
        case TR_PARSE_BAD_VERSION:      return "Not a TR1 level (bad version)";
        case TR_PARSE_TRUNCATED:        return "Truncated file";
        case TR_PARSE_OUT_OF_MEMORY:    return "Out of memory";
        case TR_PARSE_TOO_MANY_ROOMS:   return "Too many rooms";
        case TR_PARSE_TOO_MANY_ITEMS:   return "Too many items";
        default:                        return "Unknown error";
    }
}

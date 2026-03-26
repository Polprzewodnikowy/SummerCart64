/*
 * TRX-N64: Entity mesh drawing — implementation
 *
 * Transforms object mesh vertices through the bone matrix stack,
 * projects to screen, and emits textured triangles to the RDP.
 *
 * Every vertex position and face comes directly from the TR1 level
 * data. The bone matrix applies the exact animation frame rotation.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_entity_draw.h"
#include "n64_room_draw.h"

#include <string.h>

/* Scratch buffer for transformed mesh vertices */
static transformed_vertex_t mesh_verts[MAX_MESH_VERTICES];

/* Forward declare the textured triangle emitter from n64_room_draw.c */
extern void emit_rdp_textured_triangle(rdp_cmd_buffer_t *buf,
    const transformed_vertex_t *v0,
    const transformed_vertex_t *v1,
    const transformed_vertex_t *v2,
    uint8_t r0, uint8_t g0, uint8_t b0,
    uint8_t r1, uint8_t g1, uint8_t b1,
    uint8_t r2, uint8_t g2, uint8_t b2,
    int16_t s0, int16_t t0,
    int16_t s1, int16_t t1,
    int16_t s2, int16_t t2);

void n64_draw_object_mesh_piece(const tr_object_mesh_t *mesh,
                                 const tr_matrix_stack_t *ms,
                                 const transform_state_t *ts,
                                 rdp_cmd_buffer_t *buf,
                                 tmem_state_t *tmem,
                                 const n64_texture_page_t *tex_pages,
                                 const tr_level_t *level) {
    if (!mesh || mesh->vertex_count <= 0) return;
    int count = mesh->vertex_count;
    if (count > MAX_MESH_VERTICES) count = MAX_MESH_VERTICES;

    /*
     * Transform each mesh vertex:
     *   1. Apply bone matrix (ms->current) → world space
     *   2. Apply view-projection (ts->vp) → clip/screen space
     *
     * The bone matrix already includes entity position + rotation
     * + bone hierarchy transforms.
     */
    for (int i = 0; i < count; i++) {
        /* Step 1: bone matrix transform → world coords */
        int32_t wx, wy, wz;
        tr_matrix_transform_point(ms,
            mesh->vertices[i].x,
            mesh->vertices[i].y,
            mesh->vertices[i].z,
            &wx, &wy, &wz);

        /* Step 2: view-projection → screen coords */
        const mat4_t *vp = &ts->vp;
        int64_t cx = (int64_t)vp->m[0][0] * wx + (int64_t)vp->m[0][1] * wy +
                     (int64_t)vp->m[0][2] * wz + ((int64_t)vp->m[0][3] << 16);
        int64_t cy = (int64_t)vp->m[1][0] * wx + (int64_t)vp->m[1][1] * wy +
                     (int64_t)vp->m[1][2] * wz + ((int64_t)vp->m[1][3] << 16);
        int64_t cz = (int64_t)vp->m[2][0] * wx + (int64_t)vp->m[2][1] * wy +
                     (int64_t)vp->m[2][2] * wz + ((int64_t)vp->m[2][3] << 16);
        int64_t cw = (int64_t)vp->m[3][0] * wx + (int64_t)vp->m[3][1] * wy +
                     (int64_t)vp->m[3][2] * wz + ((int64_t)vp->m[3][3] << 16);

        int32_t clip_x = (int32_t)(cx >> 16);
        int32_t clip_y = (int32_t)(cy >> 16);
        int32_t clip_z = (int32_t)(cz >> 16);
        int32_t clip_w = (int32_t)(cw >> 16);

        uint8_t clip_flags = 0;
        if (clip_x < -clip_w) clip_flags |= CLIP_LEFT;
        if (clip_x >  clip_w) clip_flags |= CLIP_RIGHT;
        if (clip_y < -clip_w) clip_flags |= CLIP_TOP;
        if (clip_y >  clip_w) clip_flags |= CLIP_BOTTOM;
        if (clip_z < 0)       clip_flags |= CLIP_NEAR;
        if (clip_z >  clip_w) clip_flags |= CLIP_FAR;
        mesh_verts[i].clip = clip_flags;

        if (clip_w > 0 && clip_flags == 0) {
            int32_t inv_w = (int32_t)(((int64_t)(1 << 16) << 16) / clip_w);
            int32_t ndc_x = (int32_t)(((int64_t)clip_x * inv_w) >> 16);
            int32_t ndc_y = (int32_t)(((int64_t)clip_y * inv_w) >> 16);

            mesh_verts[i].sx = (int16_t)(ts->half_w + ((ndc_x * ts->half_w) >> 16));
            mesh_verts[i].sy = (int16_t)(ts->half_h + ((ndc_y * ts->half_h) >> 16));

            int32_t ndc_z = (int32_t)(((int64_t)clip_z * inv_w) >> 16);
            mesh_verts[i].sz = (int16_t)(((ndc_z + (1 << 16)) >> 2));
            if (mesh_verts[i].sz < 0) mesh_verts[i].sz = 0;
            if (mesh_verts[i].sz > 32767) mesh_verts[i].sz = 32767;
        } else {
            mesh_verts[i].sx = -1;
            mesh_verts[i].sy = -1;
            mesh_verts[i].sz = 32767;
        }

        /* Per-vertex lighting from mesh data (exact original values) */
        int32_t brightness = 192;  /* Default if no lighting data */
        if (mesh->num_lights < 0 && mesh->lights) {
            /* Pre-computed per-vertex light values */
            int idx = i < (-mesh->num_lights) ? i : 0;
            int16_t light = mesh->lights[idx];
            brightness = 255 - ((light * 255) >> 13);
            if (brightness < 0) brightness = 0;
            if (brightness > 255) brightness = 255;
        }
        mesh_verts[i].r = mesh_verts[i].g = mesh_verts[i].b = (uint8_t)brightness;
        mesh_verts[i].a = 255;
    }

    /*
     * Draw faces — same approach as room faces.
     * Object meshes have textured and flat faces merged in our parser.
     */
    for (int f = 0; f < mesh->face_count; f++) {
        const tr_face_t *face = &mesh->faces[f];

        /* Gather transformed vertices */
        const transformed_vertex_t *fv[4];
        bool bad = false;
        uint8_t and_clip = 0xFF;

        for (int v = 0; v < face->vertex_count; v++) {
            uint16_t vi = face->vertices[v];
            if (vi >= (uint16_t)count) { bad = true; break; }
            fv[v] = &mesh_verts[vi];
            and_clip &= fv[v]->clip;
        }
        if (bad || and_clip != 0) continue;  /* Fully clipped or invalid */

        /* Look up texture */
        uint16_t tex_idx = face->texture & 0x7FFF;
        bool is_flat = (face->texture & 0x8000) != 0;
        bool textured = false;
        tex_tile_desc_t tile_desc;
        const tr_object_texture_t *obj_tex = NULL;

        if (!is_flat && tex_idx < level->object_texture_count) {
            obj_tex = &level->object_textures[tex_idx];
            n64_tex_get_tile(obj_tex, face->vertex_count, &tile_desc);
            if (tile_desc.fits_tmem && tile_desc.page < level->tex_page_count) {
                n64_tex_bind(buf, tmem, tex_pages, &tile_desc);
                textured = true;
            }
        }

        /* UV coords */
        int16_t vu[4] = {0}, vv[4] = {0};
        if (textured && obj_tex) {
            for (int v = 0; v < face->vertex_count; v++) {
                vu[v] = n64_tex_remap_u(obj_tex->uv[v].u, tile_desc.x, tile_desc.w);
                vv[v] = n64_tex_remap_v(obj_tex->uv[v].v, tile_desc.y, tile_desc.h);
            }
        }

        /* Emit triangles (same pattern as room draw) */
        /* Triangle 1: verts 0,1,2 */
        if (textured) {
            emit_rdp_textured_triangle(buf,
                fv[0], fv[1], fv[2],
                fv[0]->r, fv[0]->g, fv[0]->b,
                fv[1]->r, fv[1]->g, fv[1]->b,
                fv[2]->r, fv[2]->g, fv[2]->b,
                vu[0], vv[0], vu[1], vv[1], vu[2], vv[2]);
        } else {
            uint8_t cr = (fv[0]->r + fv[1]->r + fv[2]->r) / 3;
            uint8_t cg = (fv[0]->g + fv[1]->g + fv[2]->g) / 3;
            uint8_t cb = (fv[0]->b + fv[1]->b + fv[2]->b) / 3;
            /* For flat colored faces, tex_idx is the palette color index.
             * TODO: look up palette color from level->palette[tex_idx*3..] */
            (void)cr; (void)cg; (void)cb;
        }

        /* Triangle 2 for quads: verts 0,2,3 */
        if (face->vertex_count == 4) {
            if (textured) {
                emit_rdp_textured_triangle(buf,
                    fv[0], fv[2], fv[3],
                    fv[0]->r, fv[0]->g, fv[0]->b,
                    fv[2]->r, fv[2]->g, fv[2]->b,
                    fv[3]->r, fv[3]->g, fv[3]->b,
                    vu[0], vv[0], vu[2], vv[2], vu[3], vv[3]);
            }
        }
    }
}

void n64_draw_room_entities(const tr_level_t *level,
                             int16_t room_idx,
                             tr_matrix_stack_t *ms,
                             const transform_state_t *ts,
                             rdp_cmd_buffer_t *buf,
                             tmem_state_t *tmem,
                             const n64_texture_page_t *tex_pages,
                             tr_anim_state_t *entity_anims,
                             int entity_count) {
    for (int i = 0; i < level->item_count && i < entity_count; i++) {
        const tr_item_t *item = &level->items[i];
        if (item->room != room_idx) continue;

        /* Find object definition */
        const tr_object_def_t *obj = NULL;
        for (int o = 0; o < level->object_count; o++) {
            if (level->objects[o].object_id == item->object_id) {
                obj = &level->objects[o];
                break;
            }
        }
        if (!obj || obj->mesh_count <= 0) continue;

        /* Draw with animation */
        tr_anim_draw_entity(item, obj, &entity_anims[i], level,
                             ms, ts, (struct rdp_cmd_buffer *)buf);
    }
}

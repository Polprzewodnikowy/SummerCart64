/*
 * TRX-N64: Room renderer
 *
 * Implements portal-based room visibility determination and face rendering.
 *
 * Rendering flow per frame:
 *   1. Start from camera's room, full-screen clip rect
 *   2. Transform room vertices → screen space
 *   3. Draw room faces (sorted by texture for TMEM efficiency)
 *   4. For each portal: clip to parent's rect, add destination room
 *   5. Recursively draw connected rooms until depth limit
 *
 * RDP triangle commands:
 *   The N64 RDP expects edge coefficients for triangle rasterization.
 *   Each triangle command is 176 bits (22 bytes) for shaded+textured.
 *   We build these from screen-space vertices with the correct edge
 *   slope format the RDP requires.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_room_draw.h"

#include <string.h>

/* -------------------------------------------------------------------
 * RDP triangle command builder
 *
 * The RDP triangle command format requires edges sorted by Y coordinate
 * and pre-computed slopes (DxDy) for major and minor edges.
 *
 * For flat-shaded (no texture) triangles: 8 uint64_t words.
 * For shaded triangles: add shade coefficients.
 * For textured: add texture coefficients.
 * ------------------------------------------------------------------- */

/* Sort three vertices by Y coordinate (top to bottom).
 * Returns indices such that v[idx[0]].sy <= v[idx[1]].sy <= v[idx[2]].sy */
static void sort_vertices_by_y(const transformed_vertex_t *v, int *i0, int *i1, int *i2) {
    if (v[0].sy <= v[1].sy) {
        if (v[1].sy <= v[2].sy) { *i0 = 0; *i1 = 1; *i2 = 2; }
        else if (v[0].sy <= v[2].sy) { *i0 = 0; *i1 = 2; *i2 = 1; }
        else { *i0 = 2; *i1 = 0; *i2 = 1; }
    } else {
        if (v[0].sy <= v[2].sy) { *i0 = 1; *i1 = 0; *i2 = 2; }
        else if (v[1].sy <= v[2].sy) { *i0 = 1; *i1 = 2; *i2 = 0; }
        else { *i0 = 2; *i1 = 1; *i2 = 0; }
    }
}

/*
 * Emit an RDP flat-shaded triangle command.
 *
 * Uses the Non-Shaded Triangle (opcode 0x08) format:
 *   Word 0: command + edge data
 *   Words 1-7: edge coefficients (XH, XM, XL slopes)
 *
 * Screen coordinates are in 10.2 fixed-point for the RDP.
 */
static void emit_rdp_triangle(rdp_cmd_buffer_t *buf,
                                const transformed_vertex_t *v0,
                                const transformed_vertex_t *v1,
                                const transformed_vertex_t *v2,
                                uint8_t r, uint8_t g, uint8_t b) {
    /*
     * RDP triangle edge format:
     *
     * After sorting by Y (top=H, mid=M, bottom=L):
     *   YH = top Y, YM = middle Y, YL = bottom Y (in 10.2)
     *   XH = X at top, XM = X at mid-left, XL = X at bottom-left (in 16.16)
     *   DxHDy = slope of long edge (top→bottom)
     *   DxMDy = slope of top half (top→mid)
     *   DxLDy = slope of bottom half (mid→bottom)
     *
     * Slopes are 16.16 fixed-point: pixels per scanline.
     */

    /* Convert screen coords to 10.2 fixed-point */
    int32_t yh = v0->sy << 2;
    int32_t ym = v1->sy << 2;
    int32_t yl = v2->sy << 2;

    /* X values in 16.16 */
    int32_t xh = v0->sx << 16;
    int32_t xm = v0->sx << 16;  /* Start of short edge at top */
    int32_t xl = v1->sx << 16;  /* Start of second short edge at mid */

    /* Calculate slopes */
    int32_t dxhdy = 0, dxmdy = 0, dxldy = 0;

    int32_t dy_hl = v2->sy - v0->sy;
    int32_t dy_hm = v1->sy - v0->sy;
    int32_t dy_ml = v2->sy - v1->sy;

    if (dy_hl > 0)
        dxhdy = ((v2->sx - v0->sx) << 16) / dy_hl;
    if (dy_hm > 0)
        dxmdy = ((v1->sx - v0->sx) << 16) / dy_hm;
    if (dy_ml > 0)
        dxldy = ((v2->sx - v1->sx) << 16) / dy_ml;

    /* Determine winding: is the major edge on the left or right?
     * If the mid vertex is to the right of the major edge at its Y,
     * then left_major = 1 (major edge is on the left). */
    int32_t mid_x_on_major = (v0->sx << 16) + dxhdy * dy_hm;
    int left_major = (v1->sx << 16) > mid_x_on_major ? 1 : 0;

    /* RDP opcode: 0x08 = Non-Shaded Triangle */
    /* But we'll use 0x0C = Shade Triangle for flat color */
    uint8_t opcode = 0x0C;  /* Shade triangle */

    /* Word 0: opcode | left_major | tile | YL | YM | YH */
    uint64_t w0 = ((uint64_t)opcode << 56)
                | ((uint64_t)(left_major & 1) << 55)
                | ((uint64_t)(yl & 0x3FFF) << 32)
                | ((uint64_t)(ym & 0x3FFF) << 16)
                | ((uint64_t)(yh & 0x3FFF));
    rdp_emit(buf, w0);

    /* Word 1: XL (16.16) */
    rdp_emit(buf, ((uint64_t)(xl & 0xFFFFFFFF) << 32)
                | ((uint64_t)(dxldy & 0xFFFFFFFF)));

    /* Word 2: XH (16.16) */
    rdp_emit(buf, ((uint64_t)(xh & 0xFFFFFFFF) << 32)
                | ((uint64_t)(dxhdy & 0xFFFFFFFF)));

    /* Word 3: XM (16.16) */
    rdp_emit(buf, ((uint64_t)(xm & 0xFFFFFFFF) << 32)
                | ((uint64_t)(dxmdy & 0xFFFFFFFF)));

    /* Shade coefficients (flat shading — constant across triangle) */
    /* Word 4: R.FRAC | G.FRAC | B.FRAC | A.FRAC */
    uint64_t shade_rgba = ((uint64_t)r << 56) | ((uint64_t)g << 40)
                        | ((uint64_t)b << 24) | ((uint64_t)0xFF << 8);
    rdp_emit(buf, shade_rgba);

    /* Word 5: dRdX | dGdX | dBdX | dAdX (all zero for flat) */
    rdp_emit(buf, 0);

    /* Word 6: dRdE | dGdE | dBdE | dAdE (all zero for flat) */
    rdp_emit(buf, 0);

    /* Word 7: dRdY | dGdY | dBdY | dAdY (all zero for flat) */
    rdp_emit(buf, 0);
}

/* -------------------------------------------------------------------
 * Portal visibility
 * ------------------------------------------------------------------- */

/* Test if a portal is visible within the current clip rectangle.
 * Returns the intersection clip rect in (cl, ct, cr, cb). */
static bool portal_visible(const transform_state_t *ts,
                            const tr_portal_t *portal,
                            int32_t room_x, int32_t room_z,
                            int16_t clip_l, int16_t clip_t,
                            int16_t clip_r, int16_t clip_b,
                            int16_t *out_l, int16_t *out_t,
                            int16_t *out_r, int16_t *out_b) {
    /* Transform portal vertices to screen space */
    int16_t min_x = 32767, min_y = 32767;
    int16_t max_x = -32768, max_y = -32768;
    bool any_visible = false;

    for (int i = 0; i < 4; i++) {
        int32_t wx = room_x + portal->vertices[i].x;
        int32_t wy = portal->vertices[i].y;
        int32_t wz = room_z + portal->vertices[i].z;

        transformed_vertex_t tv;
        transform_vertex_direct(ts, wx, wy, wz, &tv);

        if (tv.clip == 0) {
            any_visible = true;
            if (tv.sx < min_x) min_x = tv.sx;
            if (tv.sy < min_y) min_y = tv.sy;
            if (tv.sx > max_x) max_x = tv.sx;
            if (tv.sy > max_y) max_y = tv.sy;
        } else {
            /* Vertex is clipped — extend bounds to screen edges
             * to handle partially visible portals conservatively */
            any_visible = true;  /* Might still be partially visible */
            if (tv.clip & CLIP_LEFT) { min_x = 0; }
            if (tv.clip & CLIP_RIGHT) { max_x = ts->screen_w - 1; }
            if (tv.clip & CLIP_TOP) { min_y = 0; }
            if (tv.clip & CLIP_BOTTOM) { max_y = ts->screen_h - 1; }
        }
    }

    if (!any_visible) return false;

    /* Expand to safe bounds if no valid screen-space verts */
    if (min_x > max_x || min_y > max_y) {
        min_x = clip_l; min_y = clip_t;
        max_x = clip_r; max_y = clip_b;
    }

    /* Intersect with parent clip rect */
    *out_l = (min_x > clip_l) ? min_x : clip_l;
    *out_t = (min_y > clip_t) ? min_y : clip_t;
    *out_r = (max_x < clip_r) ? max_x : clip_r;
    *out_b = (max_y < clip_b) ? max_y : clip_b;

    return (*out_l < *out_r) && (*out_t < *out_b);
}

/*
 * transform_vertex_direct: project a single world-space point to screen.
 * Used for portal visibility testing where we don't have bulk vertex arrays.
 */
static void transform_vertex_direct(const transform_state_t *ts,
                                     int32_t wx, int32_t wy, int32_t wz,
                                     transformed_vertex_t *out) {
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
    out->clip = clip_flags;

    if (clip_w > 0 && clip_flags == 0) {
        int32_t inv_w = (int32_t)(((int64_t)FIX16_ONE << 16) / clip_w);
        int32_t ndc_x = (int32_t)(((int64_t)clip_x * inv_w) >> 16);
        int32_t ndc_y = (int32_t)(((int64_t)clip_y * inv_w) >> 16);

        out->sx = (int16_t)(ts->half_w + ((ndc_x * ts->half_w) >> 16));
        out->sy = (int16_t)(ts->half_h + ((ndc_y * ts->half_h) >> 16));
        out->sz = 0;
    } else {
        out->sx = -1;
        out->sy = -1;
        out->sz = 32767;
    }
}

/* -------------------------------------------------------------------
 * Room face rendering
 * ------------------------------------------------------------------- */

static void draw_room_faces(room_draw_state_t *rds,
                             const tr_room_t *room,
                             const transformed_vertex_t *verts) {
    rdp_cmd_buffer_t *buf = &rds->renderer->cmd_buf;

    for (int f = 0; f < room->mesh.face_count; f++) {
        const tr_face_t *face = &room->mesh.faces[f];

        /* Get transformed vertices for this face */
        const transformed_vertex_t *fv[4];
        bool any_clipped = false;
        for (int i = 0; i < face->vertex_count; i++) {
            uint16_t vi = face->vertices[i];
            if (vi >= room->mesh.vertex_count) { any_clipped = true; break; }
            fv[i] = &verts[vi];
            if (fv[i]->clip != 0) any_clipped = true;
        }

        /* Skip fully clipped faces */
        if (any_clipped) {
            /* Check if ALL vertices are clipped on the same side */
            if (face->vertex_count >= 3) {
                uint8_t and_clip = 0xFF;
                for (int i = 0; i < face->vertex_count; i++) {
                    and_clip &= fv[i]->clip;
                }
                if (and_clip != 0) {
                    rds->faces_clipped++;
                    continue;  /* All verts clipped on same side — skip */
                }
            }
            /* Partially visible — draw anyway (RDP will clip to scissor) */
        }

        /* Degenerate check */
        if (fv[0]->sx == fv[1]->sx && fv[0]->sy == fv[1]->sy) continue;
        if (fv[1]->sx == fv[2]->sx && fv[1]->sy == fv[2]->sy) continue;

        /* Get face color from vertex lighting (average) */
        uint8_t avg_r = 0, avg_g = 0, avg_b = 0;
        for (int i = 0; i < face->vertex_count; i++) {
            avg_r += fv[i]->r / face->vertex_count;
            avg_g += fv[i]->g / face->vertex_count;
            avg_b += fv[i]->b / face->vertex_count;
        }

        /* TODO: look up texture from face->texture & 0x7FFF
         * and bind via n64_render_bind_texture(). For now: flat shading. */

        /* Emit triangle(s) */
        transformed_vertex_t sorted[3];

        /* First triangle */
        sorted[0] = *fv[0];
        sorted[1] = *fv[1];
        sorted[2] = *fv[2];

        /* Sort by Y for RDP */
        int i0, i1, i2;
        sort_vertices_by_y(sorted, &i0, &i1, &i2);
        emit_rdp_triangle(buf, &sorted[i0], &sorted[i1], &sorted[i2],
                          avg_r, avg_g, avg_b);
        rds->faces_drawn++;

        /* Second triangle for quads */
        if (face->vertex_count == 4) {
            sorted[0] = *fv[0];
            sorted[1] = *fv[2];
            sorted[2] = *fv[3];
            sort_vertices_by_y(sorted, &i0, &i1, &i2);
            emit_rdp_triangle(buf, &sorted[i0], &sorted[i1], &sorted[i2],
                              avg_r, avg_g, avg_b);
            rds->faces_drawn++;
        }
    }
}

/* -------------------------------------------------------------------
 * Portal traversal (recursive BFS)
 * ------------------------------------------------------------------- */

static void collect_visible_rooms(room_draw_state_t *rds,
                                   int16_t room_idx,
                                   int16_t cl, int16_t ct,
                                   int16_t cr, int16_t cb,
                                   int depth) {
    if (depth >= MAX_PORTAL_DEPTH) return;
    if (rds->visible_count >= MAX_VISIBLE_ROOMS) return;
    if (room_idx < 0 || room_idx >= rds->level->room_count) return;

    /* Check if already in visible list */
    for (int i = 0; i < rds->visible_count; i++) {
        if (rds->visible[i].room_idx == room_idx) return;
    }

    /* Add to visible list */
    visible_room_t *vr = &rds->visible[rds->visible_count++];
    vr->room_idx = room_idx;
    vr->clip_left = cl;
    vr->clip_top = ct;
    vr->clip_right = cr;
    vr->clip_bottom = cb;

    /* Traverse portals */
    const tr_room_t *room = &rds->level->rooms[room_idx];
    for (int p = 0; p < room->portal_count; p++) {
        const tr_portal_t *portal = &room->portals[p];
        rds->portals_tested++;

        int16_t pcl, pct, pcr, pcb;
        if (portal_visible(rds->transform, portal,
                           room->x, room->z,
                           cl, ct, cr, cb,
                           &pcl, &pct, &pcr, &pcb)) {
            collect_visible_rooms(rds, portal->room, pcl, pct, pcr, pcb,
                                  depth + 1);
        }
    }
}

/* -------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------- */

void room_draw_init(room_draw_state_t *rds,
                    const tr_level_t *level,
                    transform_state_t *transform,
                    n64_renderer_t *renderer) {
    memset(rds, 0, sizeof(*rds));
    rds->level = level;
    rds->transform = transform;
    rds->renderer = renderer;
}

void room_draw_frame(room_draw_state_t *rds, int16_t camera_room) {
    /* Reset per-frame state */
    rds->visible_count = 0;
    rds->rooms_drawn = 0;
    rds->faces_drawn = 0;
    rds->faces_clipped = 0;
    rds->portals_tested = 0;

    /* Build visible room list via portal traversal */
    collect_visible_rooms(rds, camera_room,
                          0, 0,
                          rds->transform->screen_w - 1,
                          rds->transform->screen_h - 1,
                          0);

    /* Draw each visible room */
    for (int i = 0; i < rds->visible_count; i++) {
        visible_room_t *vr = &rds->visible[i];
        const tr_room_t *room = &rds->level->rooms[vr->room_idx];

        /* Skip rooms with no geometry */
        if (room->mesh.vertex_count == 0 || room->mesh.face_count == 0)
            continue;

        /* Set RDP scissor to portal clip rect */
        rdp_cmd_buffer_t *buf = &rds->renderer->cmd_buf;
        /* Emit scissor command */
        uint64_t scissor = ((uint64_t)0x2D << 56)
                         | ((uint64_t)(vr->clip_left << 2) << 44)
                         | ((uint64_t)(vr->clip_top << 2) << 32)
                         | ((uint64_t)(vr->clip_right << 2) << 12)
                         | ((uint64_t)(vr->clip_bottom << 2));
        rdp_emit(buf, scissor);

        /* Transform room vertices */
        int count = room->mesh.vertex_count;
        if (count > MAX_ROOM_VERTICES) count = MAX_ROOM_VERTICES;

        transform_room_vertices(rds->transform,
                                room->x, room->z,
                                room->mesh.vertices,
                                rds->vertex_buf,
                                count,
                                room->ambient);

        /* Draw faces */
        draw_room_faces(rds, room, rds->vertex_buf);

        rds->rooms_drawn++;
    }

    /* Restore full-screen scissor */
    rdp_cmd_buffer_t *buf = &rds->renderer->cmd_buf;
    uint64_t full_scissor = ((uint64_t)0x2D << 56)
                          | ((uint64_t)(rds->transform->screen_w << 2) << 12)
                          | ((uint64_t)(rds->transform->screen_h << 2));
    rdp_emit(buf, full_scissor);
}

/* rdp_emit is inline in n64_render.h */

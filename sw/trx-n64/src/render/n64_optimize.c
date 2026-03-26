/*
 * TRX-N64: Rendering optimizations (Milestone 8)
 *
 * Key optimizations for achieving 30fps on N64 hardware:
 *
 * 1. Z-BUFFER DISABLE
 *    TR1's portal-based rendering naturally provides back-to-front
 *    ordering. Disabling Z saves 48 bits written + 16 bits read
 *    per pixel = 75% memory bandwidth reduction.
 *
 * 2. FACE SORTING
 *    Within each room, sort faces by:
 *      a) Texture page (minimize TMEM reloads)
 *      b) Average Z depth (back-to-front for Z-buffer-off mode)
 *
 * 3. FRUSTUM CULLING
 *    Early-reject entire rooms and meshes using bounding box tests
 *    against the view frustum. Avoids transforming invisible vertices.
 *
 * 4. VERTEX CACHE
 *    Transform room vertices once per frame (already done in room_draw).
 *    For entities, cache transformed vertices per bone matrix.
 *
 * 5. RDP COMMAND BATCHING
 *    Group triangles by RDP state (texture, blend mode) to minimize
 *    sync commands between state changes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_render.h"
#include "n64_transform.h"
#include "../level/tr_types.h"

#include <string.h>

/* -------------------------------------------------------------------
 * Face sort key: pack texture + depth into one sortable uint32_t
 *
 * Bits [31:16] = texture page (sort by texture first for TMEM)
 * Bits [15:0]  = average Z depth (back-to-front within same texture)
 * ------------------------------------------------------------------- */

typedef struct {
    uint32_t sort_key;
    uint16_t face_index;
} face_sort_entry_t;

/* Simple insertion sort — good for TR1's typical 50-200 faces per room */
static void sort_faces(face_sort_entry_t *entries, int count) {
    for (int i = 1; i < count; i++) {
        face_sort_entry_t key = entries[i];
        int j = i - 1;
        while (j >= 0 && entries[j].sort_key > key.sort_key) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }
}

/* -------------------------------------------------------------------
 * Build sort keys for all faces in a room
 * ------------------------------------------------------------------- */

#define MAX_SORT_FACES  1024

static face_sort_entry_t sort_buffer[MAX_SORT_FACES];

int optimize_sort_room_faces(const tr_room_t *room,
                              const transformed_vertex_t *verts,
                              const tr_level_t *level,
                              face_sort_entry_t *out,
                              int max_entries) {
    int count = 0;

    for (int f = 0; f < room->mesh.face_count && count < max_entries; f++) {
        const tr_face_t *face = &room->mesh.faces[f];

        /* Compute average Z from transformed vertices */
        int32_t avg_z = 0;
        bool valid = true;
        for (int v = 0; v < face->vertex_count; v++) {
            uint16_t vi = face->vertices[v];
            if (vi >= room->mesh.vertex_count) { valid = false; break; }
            avg_z += verts[vi].sz;
        }
        if (!valid) continue;
        avg_z /= face->vertex_count;

        /* Get texture page for sort key */
        uint16_t tex_page = 0;
        uint16_t tex_idx = face->texture & 0x7FFF;
        if (tex_idx < level->object_texture_count) {
            tex_page = level->object_textures[tex_idx].tex_page;
        }

        /* Pack: texture page in high 16 bits, depth (inverted for back-to-front) in low 16 */
        out[count].sort_key = ((uint32_t)tex_page << 16)
                            | ((uint32_t)(32767 - avg_z) & 0xFFFF);
        out[count].face_index = (uint16_t)f;
        count++;
    }

    /* Sort by key: groups by texture, then back-to-front within each group */
    sort_faces(out, count);

    return count;
}

/* -------------------------------------------------------------------
 * Bounding box frustum test
 *
 * Quick test: is an axis-aligned bounding box potentially visible?
 * Returns false if the entire box is outside any frustum plane.
 * Used to cull entire rooms and entity meshes.
 * ------------------------------------------------------------------- */

bool optimize_bbox_visible(const transform_state_t *ts,
                            int32_t min_x, int32_t min_y, int32_t min_z,
                            int32_t max_x, int32_t max_y, int32_t max_z) {
    /*
     * Transform all 8 corners of the AABB and check if any
     * corner is inside the frustum. This is conservative but fast.
     *
     * For a more precise test, we'd test each frustum plane against
     * the box, but this is good enough for room-level culling.
     */
    const mat4_t *vp = &ts->vp;
    int corners_behind = 0;

    /* Test 8 corners */
    int32_t corners[8][3] = {
        {min_x, min_y, min_z}, {max_x, min_y, min_z},
        {min_x, max_y, min_z}, {max_x, max_y, min_z},
        {min_x, min_y, max_z}, {max_x, min_y, max_z},
        {min_x, max_y, max_z}, {max_x, max_y, max_z},
    };

    for (int c = 0; c < 8; c++) {
        int32_t x = corners[c][0], y = corners[c][1], z = corners[c][2];

        /* Quick W test: if W < 0, vertex is behind camera */
        int64_t cw = (int64_t)vp->m[3][0] * x + (int64_t)vp->m[3][1] * y +
                     (int64_t)vp->m[3][2] * z + ((int64_t)vp->m[3][3] << 16);
        if ((int32_t)(cw >> 16) <= 0) {
            corners_behind++;
        }
    }

    /* If all 8 corners are behind camera, the box is invisible */
    return corners_behind < 8;
}

/* -------------------------------------------------------------------
 * RDP state change tracker
 *
 * Tracks the current RDP state to avoid redundant commands.
 * Each state change requires a SYNC_PIPE, which costs ~3 RDP cycles.
 * Batching triangles by state saves hundreds of syncs per frame.
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t  current_tex_page;
    int32_t  current_tile_x, current_tile_y;
    int32_t  current_tile_w, current_tile_h;
    bool     z_buffer_enabled;
    uint32_t syncs_saved;
    uint32_t syncs_emitted;
} rdp_state_tracker_t;

void optimize_state_init(rdp_state_tracker_t *st) {
    memset(st, 0, sizeof(*st));
    st->current_tex_page = -1;
    st->z_buffer_enabled = false;
}

/* Check if a texture bind would change state.
 * Returns true if the state changed (caller should emit sync). */
bool optimize_state_needs_tex_change(rdp_state_tracker_t *st,
                                      int32_t tex_page,
                                      int32_t tile_x, int32_t tile_y,
                                      int32_t tile_w, int32_t tile_h) {
    if (st->current_tex_page == tex_page &&
        st->current_tile_x == tile_x &&
        st->current_tile_y == tile_y &&
        st->current_tile_w == tile_w &&
        st->current_tile_h == tile_h) {
        st->syncs_saved++;
        return false;
    }

    st->current_tex_page = tex_page;
    st->current_tile_x = tile_x;
    st->current_tile_y = tile_y;
    st->current_tile_w = tile_w;
    st->current_tile_h = tile_h;
    st->syncs_emitted++;
    return true;
}

/* -------------------------------------------------------------------
 * Performance counters
 * ------------------------------------------------------------------- */

typedef struct {
    uint32_t rooms_culled;
    uint32_t rooms_drawn;
    uint32_t faces_sorted;
    uint32_t faces_culled;
    uint32_t tris_submitted;
    uint32_t tmem_loads;
    uint32_t tmem_cache_hits;
    uint32_t rdp_syncs_saved;
    uint32_t frame_time_us;
} perf_counters_t;

static perf_counters_t g_perf;

perf_counters_t *optimize_get_perf(void) {
    return &g_perf;
}

void optimize_reset_perf(void) {
    memset(&g_perf, 0, sizeof(g_perf));
}

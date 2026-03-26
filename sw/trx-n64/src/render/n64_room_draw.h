/*
 * TRX-N64: Room renderer with portal-based visibility
 *
 * TR1 rooms are connected via portals. Rendering starts from the
 * camera's current room and recursively draws visible rooms through
 * their portals, clipping to the portal's screen-space bounds.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef N64_ROOM_DRAW_H
#define N64_ROOM_DRAW_H

#include "n64_render.h"
#include "n64_transform.h"
#include "../level/tr_types.h"

/* Max rooms visible in one frame (portal recursion depth + breadth) */
#define MAX_VISIBLE_ROOMS   32

/* Max recursion depth for portal traversal */
#define MAX_PORTAL_DEPTH    8

/* Per-room transform cache */
#define MAX_ROOM_VERTICES   4096

/* -------------------------------------------------------------------
 * Visible room entry
 * ------------------------------------------------------------------- */

typedef struct {
    int16_t room_idx;
    /* Screen-space clip rectangle for this room */
    int16_t clip_left, clip_top, clip_right, clip_bottom;
} visible_room_t;

/* -------------------------------------------------------------------
 * Room draw state
 * ------------------------------------------------------------------- */

typedef struct {
    /* Level data */
    const tr_level_t *level;

    /* Transform pipeline */
    transform_state_t *transform;

    /* Renderer */
    n64_renderer_t *renderer;

    /* Visible room list (built per frame by portal traversal) */
    visible_room_t visible[MAX_VISIBLE_ROOMS];
    int visible_count;

    /* Scratch buffer for transformed vertices */
    transformed_vertex_t vertex_buf[MAX_ROOM_VERTICES];

    /* Per-frame stats */
    uint32_t rooms_drawn;
    uint32_t faces_drawn;
    uint32_t faces_clipped;
    uint32_t portals_tested;
} room_draw_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/*
 * Initialize room draw state.
 */
void room_draw_init(room_draw_state_t *rds,
                    const tr_level_t *level,
                    transform_state_t *transform,
                    n64_renderer_t *renderer);

/*
 * Draw all visible rooms from the given camera room.
 * This is the main per-frame rendering call.
 */
void room_draw_frame(room_draw_state_t *rds, int16_t camera_room);

#endif /* N64_ROOM_DRAW_H */

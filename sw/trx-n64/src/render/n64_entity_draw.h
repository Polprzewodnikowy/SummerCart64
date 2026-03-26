/*
 * TRX-N64: Entity mesh drawing
 *
 * Draws object meshes (Lara, enemies, pickups, etc.) using the
 * current bone transform matrix. Each mesh piece is transformed
 * by the matrix stack built from the animation system.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef N64_ENTITY_DRAW_H
#define N64_ENTITY_DRAW_H

#include "n64_render.h"
#include "n64_transform.h"
#include "n64_textures.h"
#include "../level/tr_types.h"
#include "../anim/tr_anim.h"

/* Max vertices per individual object mesh piece */
#define MAX_MESH_VERTICES   256

/*
 * Draw a single object mesh piece at the current bone matrix position.
 *
 * Transforms all mesh vertices through the bone matrix (ms->current),
 * then projects to screen space, and emits textured/shaded triangles.
 */
void n64_draw_object_mesh_piece(const tr_object_mesh_t *mesh,
                                 const tr_matrix_stack_t *ms,
                                 const transform_state_t *ts,
                                 rdp_cmd_buffer_t *buf,
                                 tmem_state_t *tmem,
                                 const n64_texture_page_t *tex_pages,
                                 const tr_level_t *level);

/*
 * Draw all entities in a room.
 * Iterates through the item list, finds items in the given room,
 * and renders them with their current animation frame.
 */
void n64_draw_room_entities(const tr_level_t *level,
                             int16_t room_idx,
                             tr_matrix_stack_t *ms,
                             const transform_state_t *ts,
                             rdp_cmd_buffer_t *buf,
                             tmem_state_t *tmem,
                             const n64_texture_page_t *tex_pages,
                             tr_anim_state_t *entity_anims,
                             int entity_count);

#endif /* N64_ENTITY_DRAW_H */

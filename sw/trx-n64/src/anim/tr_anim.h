/*
 * TRX-N64: TR1 Animation System
 *
 * Faithfully reproduces TR1's skeletal animation:
 *   - 10-bit Euler angle bone rotations (packed in 2×int16_t per bone)
 *   - Stack-based bone hierarchy (push/pop matrix transforms)
 *   - Frame interpolation: SLERP for rotation, linear for position
 *   - Y→X→Z extrinsic rotation order (exact TR1 convention)
 *
 * All rotation values, bone offsets, interpolation math, and frame
 * timing are preserved exactly from the original engine.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TR_ANIM_H
#define TR_ANIM_H

#include "../level/tr_types.h"
#include "../render/n64_transform.h"

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Parsed animation frame (decoded from raw S16 data)
 * ------------------------------------------------------------------- */

#define TR_MAX_MESHES_PER_OBJECT    16

typedef struct {
    /* Bounding box for this frame */
    tr_xyz16_t bounds_min;
    tr_xyz16_t bounds_max;

    /* Root mesh translation offset */
    tr_xyz16_t offset;

    /* Per-mesh bone rotations (Euler angles, 16-bit) */
    tr_xyz16_t mesh_rots[TR_MAX_MESHES_PER_OBJECT];
    int16_t    mesh_count;
} tr_anim_frame_t;

/* -------------------------------------------------------------------
 * Animation playback state for one entity
 * ------------------------------------------------------------------- */

typedef struct {
    /* Current animation */
    int16_t anim_num;
    int16_t frame_num;          /* Current frame within animation */

    /* State machine */
    int16_t current_state;
    int16_t goal_state;

    /* Interpolation */
    int16_t interp_frac;        /* Fraction between keyframes (0-rate) */
    int16_t interp_rate;        /* Keyframe span (from anim->interpolation) */

    /* Decoded keyframes for current interpolation */
    tr_anim_frame_t frame_a;    /* Previous keyframe */
    tr_anim_frame_t frame_b;    /* Next keyframe */
} tr_anim_state_t;

/* -------------------------------------------------------------------
 * 3×4 fixed-point matrix (matches TR1's internal format)
 *
 * 3×3 rotation + 3×1 translation, all in 18.14 fixed-point.
 * W2V_SHIFT = 14 (divide by 16384 to normalize).
 * ------------------------------------------------------------------- */

#define W2V_SHIFT   14

typedef struct {
    int32_t m[3][4];    /* [row][col], col 3 = translation */
} tr_matrix_t;

/* Matrix stack for bone hierarchy traversal */
#define TR_MATRIX_STACK_DEPTH   32

typedef struct {
    tr_matrix_t stack[TR_MATRIX_STACK_DEPTH];
    int         top;
    tr_matrix_t current;

    /* Interpolation matrices (two tracks for SLERP) */
    tr_matrix_t interp_a;
    tr_matrix_t interp_b;
    bool        interpolating;
    int16_t     interp_frac;
    int16_t     interp_rate;
} tr_matrix_stack_t;

/* -------------------------------------------------------------------
 * API: Frame Decoding
 * ------------------------------------------------------------------- */

/*
 * Decode a raw animation frame from the packed S16 data.
 *
 * raw_frames:   Pointer to the level's raw animation frame data
 * frame_offset: Byte offset from anim->frame_offset
 * mesh_count:   Number of meshes in the object
 * out:          Decoded frame structure
 *
 * TR1 frame format per mesh: two int16_t values encoding
 * 10-bit XYZ Euler angles (see tr_anim_decode_rotation).
 */
void tr_anim_decode_frame(const int16_t *raw_frames,
                           uint32_t frame_offset,
                           int16_t mesh_count,
                           tr_anim_frame_t *out);

/*
 * Decode a single mesh rotation from two packed int16_t values.
 *
 * TR1 encoding (2 words, 4 bytes total):
 *   word1 bits[4:13]  = X rotation (10 bits) → shifted left 6
 *   word1 bits[0:3] + word2 bits[10:15] = Y rotation (10 bits) → shifted left 6
 *   word2 bits[0:9]   = Z rotation (10 bits) → shifted left 6
 *
 * Returns 16-bit Euler angles in TR1's angle unit system.
 */
tr_xyz16_t tr_anim_decode_rotation(int16_t word1, int16_t word2);

/* -------------------------------------------------------------------
 * API: Matrix Stack (bone hierarchy)
 * ------------------------------------------------------------------- */

void tr_matrix_init(tr_matrix_stack_t *ms);
void tr_matrix_push(tr_matrix_stack_t *ms);
void tr_matrix_pop(tr_matrix_stack_t *ms);

/* Set the current matrix to world-to-view transform */
void tr_matrix_set_view(tr_matrix_stack_t *ms, const tr_matrix_t *view);

/* Apply translation (bone offset) */
void tr_matrix_translate_rel16(tr_matrix_stack_t *ms, tr_xyz16_t offset);
void tr_matrix_translate_rel32(tr_matrix_stack_t *ms, int32_t x, int32_t y, int32_t z);

/*
 * Apply rotation (bone rotation from animation frame).
 * Rotation order: Y → X → Z (TR1's exact convention).
 */
void tr_matrix_rotate_yxz(tr_matrix_stack_t *ms, tr_xyz16_t rotation);

/* Transform a point by the current matrix */
void tr_matrix_transform_point(const tr_matrix_stack_t *ms,
                                int16_t x, int16_t y, int16_t z,
                                int32_t *out_x, int32_t *out_y, int32_t *out_z);

/* -------------------------------------------------------------------
 * API: Interpolation
 * ------------------------------------------------------------------- */

/*
 * Initialize interpolation between two keyframes.
 */
void tr_matrix_init_interpolate(tr_matrix_stack_t *ms,
                                 int16_t frac, int16_t rate);

/*
 * Apply interpolated rotation from two frames.
 * Performs SLERP on the resulting matrices (faithful to TRX).
 */
void tr_matrix_rotate_yxz_interp(tr_matrix_stack_t *ms,
                                   tr_xyz16_t rot_a,
                                   tr_xyz16_t rot_b);

/*
 * Apply interpolated translation from two frames.
 * Linear interpolation on position.
 */
void tr_matrix_translate_interp16(tr_matrix_stack_t *ms,
                                    tr_xyz16_t offset_a,
                                    tr_xyz16_t offset_b);

/* -------------------------------------------------------------------
 * API: Entity Drawing
 * ------------------------------------------------------------------- */

/*
 * Draw an animated entity (Lara, enemies, etc).
 *
 * Traverses the bone hierarchy, applies animation frame rotations,
 * and draws each mesh piece at its correct world position.
 *
 * item:     Entity placement (position, rotation, room)
 * obj:      Object definition (mesh_count, mesh_idx, bone_idx, anim_idx)
 * anim_state: Current animation playback state
 * level:    Level data (for mesh lookup)
 * ms:       Matrix stack for bone transforms
 * ts:       Transform state for screen projection
 * buf:      RDP command buffer for triangle submission
 */
void tr_anim_draw_entity(const tr_item_t *item,
                          const tr_object_def_t *obj,
                          const tr_anim_state_t *anim_state,
                          const tr_level_t *level,
                          tr_matrix_stack_t *ms,
                          const transform_state_t *ts,
                          struct rdp_cmd_buffer *buf);

/* -------------------------------------------------------------------
 * API: Animation Playback
 * ------------------------------------------------------------------- */

/*
 * Advance animation by one frame (30fps tick).
 * Handles keyframe advancement, state transitions, and
 * animation chaining — all exactly as TR1's original logic.
 */
void tr_anim_advance(tr_anim_state_t *state,
                      const tr_level_t *level,
                      const tr_object_def_t *obj);

/*
 * Get the two keyframes and interpolation fraction for the
 * current animation state. Used by the renderer.
 */
void tr_anim_get_frames(const tr_anim_state_t *state,
                         const tr_level_t *level,
                         const tr_object_def_t *obj,
                         tr_anim_frame_t *frame_a,
                         tr_anim_frame_t *frame_b,
                         int16_t *frac,
                         int16_t *rate);

#endif /* TR_ANIM_H */

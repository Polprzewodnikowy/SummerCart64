/*
 * TRX-N64: Software vertex transform pipeline
 *
 * Transforms TR1 world-space vertices to N64 screen-space.
 * All math is 32-bit fixed-point (16.16) for VR4300 performance.
 *
 * Pipeline: World → View → Projection → Clip → Screen
 *
 * TR1 coordinate system:
 *   +X = right, +Y = down, +Z = forward (into screen)
 *   1 sector = 1024 units, Lara ≈ 762 units tall
 *
 * N64 screen: 320×240, origin top-left
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef N64_TRANSFORM_H
#define N64_TRANSFORM_H

#include <stdint.h>
#include <stdbool.h>

/* 16.16 fixed point */
typedef int32_t fix16_t;

#define FIX16_ONE       (1 << 16)
#define FIX16_HALF      (1 << 15)
#define FIX16(x)        ((fix16_t)((x) * 65536.0f))

static inline fix16_t fix16_mul(fix16_t a, fix16_t b) {
    return (fix16_t)(((int64_t)a * b) >> 16);
}

static inline fix16_t fix16_div(fix16_t a, fix16_t b) {
    if (b == 0) return 0;
    return (fix16_t)(((int64_t)a << 16) / b);
}

/* -------------------------------------------------------------------
 * 4×4 Matrix (16.16 fixed-point)
 * ------------------------------------------------------------------- */

typedef struct {
    fix16_t m[4][4];
} mat4_t;

void mat4_identity(mat4_t *out);
void mat4_multiply(mat4_t *out, const mat4_t *a, const mat4_t *b);
void mat4_rotate_y(mat4_t *out, int32_t angle);  /* TR angle units */
void mat4_translate(mat4_t *out, int32_t x, int32_t y, int32_t z);

/* -------------------------------------------------------------------
 * Camera
 * ------------------------------------------------------------------- */

typedef struct {
    int32_t x, y, z;       /* World position */
    int32_t target_x, target_y, target_z;   /* Look-at target */
    int32_t rot_y;          /* Y rotation (TR angle units: 0-65535) */
    int32_t rot_x;          /* X rotation (pitch) */
    int32_t fov;            /* Field of view (TR units, ~16384 = 90°) */
} tr_camera_t;

/* -------------------------------------------------------------------
 * Transformed vertex (output of pipeline)
 * ------------------------------------------------------------------- */

typedef struct {
    /* Screen-space coordinates */
    int16_t sx, sy;         /* Pixel position */
    int16_t sz;             /* Z for depth sorting (0=near, 32767=far) */

    /* Texture coordinates (10.5 fixed for RDP) */
    int16_t u, v;

    /* Vertex color after lighting */
    uint8_t r, g, b, a;

    /* Clip flags */
    uint8_t clip;           /* 0=fully visible, nonzero=clipped */
} transformed_vertex_t;

/* Clip flag bits */
#define CLIP_LEFT   (1 << 0)
#define CLIP_RIGHT  (1 << 1)
#define CLIP_TOP    (1 << 2)
#define CLIP_BOTTOM (1 << 3)
#define CLIP_NEAR   (1 << 4)
#define CLIP_FAR    (1 << 5)

/* -------------------------------------------------------------------
 * Transform pipeline state
 * ------------------------------------------------------------------- */

typedef struct {
    mat4_t view;            /* World → View */
    mat4_t proj;            /* View → Clip */
    mat4_t vp;              /* Combined View × Projection */

    /* Screen dimensions */
    int16_t screen_w, screen_h;
    int16_t half_w, half_h;

    /* Near/far planes */
    fix16_t near_z, far_z;

    /* Camera */
    tr_camera_t camera;
} transform_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/*
 * Initialize transform pipeline with screen dimensions.
 */
void transform_init(transform_state_t *ts, int16_t width, int16_t height);

/*
 * Set the camera position and orientation.
 * Rebuilds the view and VP matrices.
 */
void transform_set_camera(transform_state_t *ts, const tr_camera_t *cam);

/*
 * Build perspective projection matrix.
 * fov_angle: TR angle units (16384 ≈ 90°)
 */
void transform_set_perspective(transform_state_t *ts,
                                int32_t fov_angle,
                                fix16_t near_z, fix16_t far_z);

/*
 * Transform a batch of room vertices from world space to screen space.
 *
 * room_x, room_z: room world origin
 * src: room-relative vertex positions
 * dst: output transformed vertices
 * count: number of vertices
 * ambient: room ambient light level
 */
void transform_room_vertices(const transform_state_t *ts,
                              int32_t room_x, int32_t room_z,
                              const void *src,  /* tr_room_vertex_t[] */
                              transformed_vertex_t *dst,
                              int count,
                              int16_t ambient);

/*
 * Transform object mesh vertices (for entities).
 * world_x/y/z: entity world position
 * rot_y: entity Y rotation (TR angle units)
 */
void transform_mesh_vertices(const transform_state_t *ts,
                              int32_t world_x, int32_t world_y, int32_t world_z,
                              int32_t rot_y,
                              const void *src,  /* tr_xyz16_t[] */
                              const int16_t *lights,
                              transformed_vertex_t *dst,
                              int count);

/* -------------------------------------------------------------------
 * Trig lookup (TR1 uses 1024 angle units per 90°)
 *
 * Full circle = 65536 angle units
 * sin/cos return 16.16 fixed-point values
 * ------------------------------------------------------------------- */

fix16_t tr_sin(int32_t angle);
fix16_t tr_cos(int32_t angle);

#endif /* N64_TRANSFORM_H */

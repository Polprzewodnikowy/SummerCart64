/*
 * TRX-N64: Chunk-based world representation for LOD + visibility
 *
 * A "chunk" is the basic unit of world visibility and LOD management.
 * Phase 1: one chunk per TR1 room.
 * Later phases: sub-room chunks, parent aggregates, baked distant reps.
 *
 * LOD strategy:
 *   NEAR  — full geometry, Z-buffered (near pass)
 *   FAR   — coarse or baked geometry, back-to-front, no Z-buffer (far pass)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TR_CHUNK_H
#define TR_CHUNK_H

#include <stdint.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------
 * LOD levels
 * --------------------------------------------------------------------- */

#define CHUNK_LOD_NEAR  0   /* Full detail — near pass, Z-buffer on       */
#define CHUNK_LOD_FAR   1   /* Coarse — far pass, back-to-front, no Z-buf */

/* Distance thresholds in TR1 world units (squared for cheap comparison).
 * 1 sector = 1024 units. ~5 sectors near, ~10 sectors = far. */
#define CHUNK_NEAR_DIST2  (5120.0f  * 5120.0f)
#define CHUNK_FAR_DIST2   (10240.0f * 10240.0f)

/* Far clip distance matching glFrustum far_p / WORLD_SCALE (350 * 32). */
#define CHUNK_FAR_WORLD   11200.0f

/* -----------------------------------------------------------------------
 * Horizontal frustum constants
 *   Vertical FOV: 60 deg  → half-angle 30 deg, tan = 0.5774
 *   Aspect: 320/240 = 4/3 → tan_h = tan_v * 4/3 = 0.7698
 *   theta_h = atan(0.7698) ≈ 37.6 deg
 * --------------------------------------------------------------------- */

#define CHUNK_TAN_H  0.7698f
#define CHUNK_SIN_H  0.6101f   /* sin(37.6 deg) */
#define CHUNK_COS_H  0.7923f   /* cos(37.6 deg) */
#define CHUNK_TAN_V  0.5774f   /* tan(30 deg) */

/* -----------------------------------------------------------------------
 * Chunk descriptor
 * --------------------------------------------------------------------- */

typedef struct {
    int   room_idx;          /* Index into tr_level_t.rooms               */

    /* Bounding sphere in TR1 world space */
    float cx, cy, cz;        /* Sphere center                             */
    float radius;            /* Sphere radius                             */

    /* AABB in TR1 world space */
    float min_x, max_x;
    float min_y, max_y;      /* TR1 Y: positive = down                    */
    float min_z, max_z;

    /* Updated per frame */
    int   lod;               /* CHUNK_LOD_NEAR or CHUNK_LOD_FAR           */
    float dist_sq;           /* Squared XZ distance from camera           */
    bool  visible;           /* Passed frustum test this frame            */
} tr_chunk_t;

#endif /* TR_CHUNK_H */

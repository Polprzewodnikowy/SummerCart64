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
    int   cluster_id;        /* Index into g_clusters (-1 = none)         */
} tr_chunk_t;

/* -----------------------------------------------------------------------
 * Cluster — groups 2-4 adjacent rooms for hierarchical LOD
 *
 * At distance, instead of drawing every face in every child room,
 * the renderer applies face decimation (skip every Nth face) to reduce
 * triangle count.  Coverage remains complete because the same rooms
 * are drawn, just at reduced face density.
 *
 * LOD tiers:
 *   NEAR  (cluster_dist < FAR_DIST2): draw all child rooms, full faces
 *   FAR   (< VERY_FAR_DIST2):         face_skip = 2 (draw every 2nd face)
 *   VFAR  (>= VERY_FAR_DIST2):        face_skip = 4 (draw every 4th face)
 * --------------------------------------------------------------------- */

#define MAX_CLUSTER_CHILDREN  4
#define MAX_CLUSTERS          64
#define CHUNK_VERY_FAR_DIST2  (15360.0f * 15360.0f)  /* ~15 sectors */

typedef struct {
    int   children[MAX_CLUSTER_CHILDREN]; /* Room indices (child chunks) */
    int   child_count;

    /* Combined bounding sphere from children */
    float cx, cy, cz;
    float radius;

    /* Per frame */
    float dist_sq;           /* Squared XZ distance from camera           */
    int   face_skip;         /* 1 = all faces, 2 = every 2nd, 4 = every 4th */
} tr_cluster_t;

#endif /* TR_CHUNK_H */

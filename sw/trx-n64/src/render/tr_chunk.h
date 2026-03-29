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

#define CHUNK_LOD_NEAR  0   /* Full detail — GL RSP T&L, no fog           */
#define CHUNK_LOD_MID   1   /* Mid detail — GL RSP T&L, with fog          */
#define CHUNK_LOD_FAR   2   /* Far detail — RDPQ CPU projection, fog      */

/* Distance thresholds in TR1 world units (squared for cheap comparison).
 * 5 tiers: near → mid → far → extra far → extreme far */
#define CHUNK_NEAR_DIST2      (4096.0f  * 4096.0f)   /* < 4 sectors: GL RGBA16 */
#define CHUNK_MID_DIST2       (7168.0f  * 7168.0f)    /* 4-7: RDPQ CI8 */
#define CHUNK_FAR_DIST2       CHUNK_MID_DIST2         /* alias */
#define CHUNK_XFAR_DIST2      (12288.0f * 12288.0f)   /* 7-12: RDPQ CI4 */
#define CHUNK_XXFAR_DIST2     (16384.0f * 16384.0f)   /* 12-16: RDPQ IA8 */
/* > 16 sectors: RDPQ IA4 (extreme fog silhouette) */

#define CHUNK_FAR_WORLD   20000.0f

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

    /* Per-room thresholds (computed once at load from room size).
     * Small rooms get tighter thresholds, large rooms get wider. */
    float max_draw_dist2;    /* Squared max draw distance                 */
    float near_dist2;        /* Threshold: NEAR → MID transition          */
    float fog_start_dist2;   /* Distance where fog begins                 */
    int   size_class;        /* 0=small, 1=medium, 2=large, 3=open        */

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

/* -----------------------------------------------------------------------
 * Texture residency / per-frame budget
 *
 * All object textures are pre-loaded in RDRAM, but binding them to
 * GL (which triggers TMEM upload on the RDP) is expensive.  The
 * budget limits how many unique texture binds occur per frame.
 *
 * Priority:  near-pass textures bind first; far-pass textures only
 * if budget remains.  If a far-pass texture can't bind, the face is
 * rendered with the most recently bound texture (avoids a stall).
 *
 * Permanent residency: the last-bound GL texture is always valid,
 * acting as a fallback. Lowest-LOD tiers draw with whatever texture
 * is currently active, which at distance is visually acceptable.
 * --------------------------------------------------------------------- */

#define TEX_BIND_BUDGET_NEAR  96   /* Max unique binds during near pass  */
#define TEX_BIND_BUDGET_FAR   32   /* Max unique binds during far pass   */

typedef struct {
    int  binds_this_frame;        /* Counter: resets each pass            */
    int  budget;                  /* Current pass budget                  */
    int  last_bound_tex;          /* Last successfully bound texture idx  */
} tex_budget_t;

#endif /* TR_CHUNK_H */

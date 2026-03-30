/*
 * TRX-N64: Tomb Raider 1 — N64 + SC64 Port
 * Libdragon boot — works in emulators + SC64 real hardware
 *
 * Uses libdragon for boot/display/input.
 * Uses SC64 registers directly for asset streaming from SD card.
 * Uses our own .PHD parser for level loading.
 */

/* Include our asset manager FIRST, then block libdragon's asset.h
 * which has a conflicting asset_load() declaration */
#include "asset_manager.h"
#define __LIBDRAGON_ASSET_H  /* Prevent libdragon's asset.h from loading */

#include <libdragon.h>
#include <malloc.h>
#include <rdpq_tri.h>
#include <rdpq_tex.h>
#include <rdpq_mode.h>
#include <mixer.h>
#include <audio.h>
#include <surface.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* Our game code */
#include "level/tr_types.h"
#include "level/tr_level_parse.h"
#include "lz4_decode.h"
#include "render/tr_chunk.h"

/* -------------------------------------------------------------------
 * Bump allocator
 * ------------------------------------------------------------------- */

#define GAME_HEAP_SIZE  (4 * 1024 * 1024)
static uint8_t game_heap[GAME_HEAP_SIZE] __attribute__((aligned(16)));
static uint32_t heap_offset = 0;

/* Parse debug positions (filled by parser, shown on pause) */
uint32_t g_parse_pos[16];
int g_parse_count = 0;

void n64_mem_init(void) { heap_offset = 0; }

void *n64_mem_alloc(uint32_t size) {
    size = (size + 15) & ~15;
    if (heap_offset + size > GAME_HEAP_SIZE) return NULL;
    void *p = &game_heap[heap_offset];
    memset(p, 0, size);
    heap_offset += size;
    return p;
}

uint32_t n64_mem_available(void) { return GAME_HEAP_SIZE - heap_offset; }

/* -------------------------------------------------------------------
 * In-memory PCM waveform for libdragon mixer
 * ------------------------------------------------------------------- */
#include <samplebuffer.h>

typedef struct {
    waveform_t wave;
    int16_t *pcm_data;
} pcm_wave_t;

static void pcm_wave_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
    pcm_wave_t *pw = (pcm_wave_t *)ctx;
    int16_t *dst = (int16_t *)samplebuffer_append(sbuf, wlen);
    int avail = pw->wave.len - wpos;
    if (avail < wlen) wlen = avail;
    if (wlen > 0)
        memcpy(dst, pw->pcm_data + wpos, wlen * 2);
}

static void pcm_wave_init(pcm_wave_t *pw, int16_t *data, int len, int freq) {
    memset(pw, 0, sizeof(*pw));
    pw->pcm_data = data;
    pw->wave.name = "tr1_sfx";
    pw->wave.bits = 16;
    pw->wave.channels = 1;
    pw->wave.frequency = (float)freq;
    pw->wave.len = len;
    pw->wave.loop_len = 0;
    pw->wave.read = pcm_wave_read;
    pw->wave.ctx = pw;
}

/* -------------------------------------------------------------------
 * SC64 unlock (needed before asset streaming)
 *
 * The SC64 registers are locked on boot. We unlock them by writing
 * the magic key sequence. This is independent of libdragon.
 * ------------------------------------------------------------------- */

/* SC64 register struct (from bootloader/src/sc64.c)
 * Physical base: 0x1FFF0000, accessed via UNCACHED (|0xA0000000) = 0xBFFF0000 */
#define SC64_REGS_BASE      0xBFFF0000UL
#define SC64_REG_SCR        (*(volatile uint32_t *)(SC64_REGS_BASE + 0x00))
#define SC64_REG_DATA0      (*(volatile uint32_t *)(SC64_REGS_BASE + 0x04))
#define SC64_REG_DATA1      (*(volatile uint32_t *)(SC64_REGS_BASE + 0x08))
#define SC64_REG_IDENTIFIER (*(volatile uint32_t *)(SC64_REGS_BASE + 0x0C))
#define SC64_REG_KEY        (*(volatile uint32_t *)(SC64_REGS_BASE + 0x10))
#define SC64_V2_IDENTIFIER  0x53437632UL  /* "SCv2" */
#define SC64_KEY_RESET  0x00000000UL
#define SC64_KEY_LOCK   0xFFFFFFFFUL
#define SC64_KEY_UNL_1  0x5F554E4CUL  /* "_UNL" */
#define SC64_KEY_UNL_2  0x4F434B5FUL  /* "OCK_" */

/* PI interface for register access (KSEG1 uncached) */
#define PI_BASE_REG     0xA4600000UL
#define PI_STATUS_REG   (*(volatile uint32_t *)(PI_BASE_REG + 0x10))

/* PI domain config for SC64 address range */
#define PI_BSD_DOM2_LAT (*(volatile uint32_t *)(PI_BASE_REG + 0x24))
#define PI_BSD_DOM2_PWD (*(volatile uint32_t *)(PI_BASE_REG + 0x28))
#define PI_BSD_DOM2_PGS (*(volatile uint32_t *)(PI_BASE_REG + 0x2C))
#define PI_BSD_DOM2_RLS (*(volatile uint32_t *)(PI_BASE_REG + 0x30))

static void pi_wait(void) {
    while (PI_STATUS_REG & 3) {}
}

static void sc64_pi_write(volatile uint32_t *addr, uint32_t value) {
    pi_wait();
    *addr = value;
    pi_wait();
}

static bool sc64_detect(void) {
    /* SC64 registers are at 0x1FFF0000 in PI address space.
     * In emulators, this address doesn't exist and causes an exception.
     * Detection: read the PI address safely using a timeout.
     * If the read returns within time, SC64 exists. */

    /* Check if the cartridge domain 2 address 1 area is accessible.
     * SC64 is at 0x1FFF0000 = cart domain 2 addr 1.
     * A safe way: check if PI doesn't hang on a read. */
    pi_wait();

    /* Try reading SC64 version register at offset 0x14.
     * On SC64: returns firmware version.
     * On no-SC64: bus error or returns 0. */
    volatile uint32_t *test_addr = (volatile uint32_t *)(0xB0000000UL + 0x1FFF0014UL - 0x10000000UL);
    /* Actually the uncached address for 0x1FFF0000 is 0xBFFF0000 */
    (void)test_addr;

    /* Simplest approach that works: just return false if we detect
     * we're likely in an emulator. The N64 reset type in DMEM can hint. */
    /* For now: use an environment flag or compile-time switch */
#ifdef N64_EMULATOR_MODE
    return false;
#else
    return true;  /* Assume SC64 on real hardware */
#endif
}

static bool sc64_unlock(void) {
    if (!sc64_detect()) return false;

    /* Configure PI domain 2 timing for SC64 register access
     * (same values as bootloader's pi_io_config) */
    pi_wait();
    PI_BSD_DOM2_LAT = 0x05;
    PI_BSD_DOM2_PWD = 0x0C;
    PI_BSD_DOM2_PGS = 0x0D;
    PI_BSD_DOM2_RLS = 0x02;
    pi_wait();

    /* The N64FlashcartMenu may have left SC64 unlocked.
     * Try reading IDENTIFIER first without re-locking. */
    pi_wait();
    uint32_t id = SC64_REG_IDENTIFIER;
    pi_wait();

    if (id == SC64_V2_IDENTIFIER) {
        return true;  /* Already unlocked by menu */
    }

    /* Not unlocked — try the unlock sequence */
    sc64_pi_write(&SC64_REG_KEY, SC64_KEY_RESET);
    sc64_pi_write(&SC64_REG_KEY, SC64_KEY_UNL_1);
    sc64_pi_write(&SC64_REG_KEY, SC64_KEY_UNL_2);

    /* Verify by reading IDENTIFIER */
    pi_wait();
    id = SC64_REG_IDENTIFIER;
    pi_wait();
    return (id == SC64_V2_IDENTIFIER);
}

/* -------------------------------------------------------------------
 * Globals
 * ------------------------------------------------------------------- */

static asset_manager_t g_asset_mgr;
static tr_level_t g_level;
static bool g_has_assets = false;

/* Chunk array — one entry per TR1 room, built once after level parse */
static tr_chunk_t  g_chunks[TR_MAX_ROOMS];
static int         g_chunk_count = 0;

/* Cluster array — groups of 2-4 rooms for hierarchical LOD */
static tr_cluster_t g_clusters[MAX_CLUSTERS];
static int          g_cluster_count = 0;

#define ARCHIVE_SECTOR  0x10000
#define CACHE_BASE      ((uint8_t *)0xB1000000)
#define CACHE_SIZE      (3 * 1024 * 1024)
#define SLOT_SIZE       (512 * 1024)
/* staging[512KB] removed — was never referenced */

/* -------------------------------------------------------------------
 * Chunk management — builds AABB/sphere for each room once at load
 * ------------------------------------------------------------------- */

/* -------------------------------------------------------------------
 * Face classification — run once after level parse.
 * Tags each face as ESSENTIAL (never skip) or DETAIL (safe to skip).
 *
 * Essential: floors, ceilings, boundary walls, portal-adjacent faces.
 * Detail: small faces, interior tessellation, decorative geometry.
 * ------------------------------------------------------------------- */
static void classify_faces(tr_level_t *level) {
    for (int ri = 0; ri < level->room_count; ri++) {
        tr_room_t *room = &level->rooms[ri];

        /* Room AABB for boundary detection (1-sector margin) */
        float rx_min = (float)room->x + 512.0f;
        float rx_max = (float)(room->x + room->sectors_x * 1024) - 512.0f;
        float rz_min = (float)room->z + 512.0f;
        float rz_max = (float)(room->z + room->sectors_z * 1024) - 512.0f;
        float ry_floor = (float)room->min_floor;
        float ry_ceil  = (float)room->max_ceiling;

        /* Collect portal vertex positions for proximity check */
        float portal_pts[64][3];
        int portal_pt_count = 0;
        for (int p = 0; p < room->portal_count && portal_pt_count < 60; p++) {
            for (int pv = 0; pv < 4 && portal_pt_count < 64; pv++) {
                portal_pts[portal_pt_count][0] = (float)(room->x + room->portals[p].vertices[pv].x);
                portal_pts[portal_pt_count][1] = (float)room->portals[p].vertices[pv].y;
                portal_pts[portal_pt_count][2] = (float)(room->z + room->portals[p].vertices[pv].z);
                portal_pt_count++;
            }
        }

        for (int f = 0; f < room->mesh.face_count; f++) {
            tr_face_t *face = &room->mesh.faces[f];
            int nv = face->vertex_count;
            if (nv < 3) { face->flags = FACE_FLAG_DETAIL; continue; }

            /* Compute face center + approximate normal + area */
            float cx = 0, cy = 0, cz = 0;
            for (int v = 0; v < nv; v++) {
                uint16_t vi = face->vertices[v];
                if (vi >= room->mesh.vertex_count) continue;
                tr_room_vertex_t *rv = &room->mesh.vertices[vi];
                cx += (float)(room->x + rv->x);
                cy += (float)rv->y;
                cz += (float)(room->z + rv->z);
            }
            cx /= nv; cy /= nv; cz /= nv;

            /* Normal from cross product of first two edges */
            tr_room_vertex_t *v0 = &room->mesh.vertices[face->vertices[0]];
            tr_room_vertex_t *v1 = &room->mesh.vertices[face->vertices[1]];
            tr_room_vertex_t *v2 = &room->mesh.vertices[face->vertices[2]];
            float e1x = v1->x - v0->x, e1y = v1->y - v0->y, e1z = v1->z - v0->z;
            float e2x = v2->x - v0->x, e2y = v2->y - v0->y, e2z = v2->z - v0->z;
            float nx = e1y * e2z - e1z * e2y;
            float ny = e1z * e2x - e1x * e2z;
            float nz = e1x * e2y - e1y * e2x;

            /* Approximate face area (half cross product magnitude) */
            float area = 0.5f * sqrtf(nx*nx + ny*ny + nz*nz);

            /* Normalize normal */
            float nlen = sqrtf(nx*nx + ny*ny + nz*nz);
            if (nlen > 0.001f) { nx /= nlen; ny /= nlen; nz /= nlen; }

            uint8_t flags = 0;

            /* Floor/ceiling: mostly horizontal (|ny| > 0.7) */
            if (ny > 0.7f || ny < -0.7f) {
                if (cy > (ry_floor - 128.0f)) {
                    flags |= FACE_FLAG_FLOOR | FACE_FLAG_ESSENTIAL;
                } else if (cy < (ry_ceil + 128.0f)) {
                    flags |= FACE_FLAG_CEILING | FACE_FLAG_ESSENTIAL;
                } else {
                    /* Interior horizontal — essential if large */
                    if (area > 100000.0f) flags |= FACE_FLAG_ESSENTIAL;
                }
            }

            /* Wall at room boundary: vertical face near AABB edge */
            if (fabsf(ny) < 0.3f) {
                if (cx < rx_min || cx > rx_max ||
                    cz < rz_min || cz > rz_max) {
                    flags |= FACE_FLAG_WALL | FACE_FLAG_ESSENTIAL;
                }
            }

            /* Portal-adjacent: face center within 512 units of any
             * portal vertex.  These define doorway/opening shapes. */
            for (int pp = 0; pp < portal_pt_count; pp++) {
                float pdx = cx - portal_pts[pp][0];
                float pdy = cy - portal_pts[pp][1];
                float pdz = cz - portal_pts[pp][2];
                if (pdx*pdx + pdy*pdy + pdz*pdz < 512.0f * 512.0f) {
                    flags |= FACE_FLAG_PORTAL_ADJ | FACE_FLAG_ESSENTIAL;
                    break;
                }
            }

            /* Small faces are detail (safe to skip) unless already
             * marked essential by position/orientation tests. */
            if (!(flags & FACE_FLAG_ESSENTIAL)) {
                if (area < 50000.0f) {
                    flags |= FACE_FLAG_DETAIL;
                } else {
                    /* Large interior face — keep it essential */
                    flags |= FACE_FLAG_ESSENTIAL;
                }
            }

            face->flags = flags;
        }
    }
}

/* Sort room faces by texture index (insertion sort — small arrays).
 * Groups same-texture faces together to minimize GL bind changes.
 * Run once after classify_faces. */
static void sort_faces_by_texture(tr_level_t *level) {
    for (int ri = 0; ri < level->room_count; ri++) {
        tr_room_t *room = &level->rooms[ri];
        int n = room->mesh.face_count;
        tr_face_t *faces = room->mesh.faces;
        if (!faces || n < 2) continue;

        /* Insertion sort by texture index — stable, in-place,
         * good for small N (TR1 rooms: ~50-400 faces) */
        for (int i = 1; i < n; i++) {
            tr_face_t key = faces[i];
            uint16_t key_tex = key.texture & 0x7FFF;
            int j = i - 1;
            while (j >= 0 && (faces[j].texture & 0x7FFF) > key_tex) {
                faces[j + 1] = faces[j];
                j--;
            }
            faces[j + 1] = key;
        }
    }
}

static void build_chunks(const tr_level_t *level) {
    g_chunk_count = 0;
    for (int i = 0; i < level->room_count && g_chunk_count < TR_MAX_ROOMS; i++) {
        const tr_room_t *r  = &level->rooms[i];
        tr_chunk_t      *c  = &g_chunks[g_chunk_count++];
        c->room_idx = i;

        /* XZ bounds from sector grid */
        c->min_x = (float)r->x;
        c->max_x = (float)(r->x + r->sectors_x * 1024);
        c->min_z = (float)r->z;
        c->max_z = (float)(r->z + r->sectors_z * 1024);

        /* Y bounds: TR1 uses Y-down convention.
         * max_ceiling is the highest point (smallest Y value).
         * min_floor   is the lowest  point (largest  Y value). */
        c->min_y = (float)r->max_ceiling;
        c->max_y = (float)r->min_floor;
        if (c->min_y > c->max_y) { float t = c->min_y; c->min_y = c->max_y; c->max_y = t; }

        /* Sphere: center + radius from AABB diagonal */
        c->cx = (c->min_x + c->max_x) * 0.5f;
        c->cy = (c->min_y + c->max_y) * 0.5f;
        c->cz = (c->min_z + c->max_z) * 0.5f;
        float dx = c->max_x - c->min_x;
        float dy = c->max_y - c->min_y;
        float dz = c->max_z - c->min_z;
        c->radius = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;

        /* Per-room max draw distance based on room size.
         * Larger rooms stay visible further. Scale the largest
         * XZ dimension by a multiplier — a room should be drawable
         * until it would be just a few pixels on screen.
         * Clamp to [g_dist_mid_sq .. g_dist_xxfar_sq]. */
        /* Room size class + scaled thresholds.
         * Computed once at load, stable for the room's lifetime.
         *
         * Classes:
         *   SMALL  (< 2 sectors): tight thresholds, fast falloff
         *   MEDIUM (2-4 sectors): normal TR1 ladder
         *   LARGE  (4-8 sectors): stretched thresholds
         *   OPEN   (> 8 sectors): full range, distant tiers matter */
        float max_dim = dx > dz ? dx : dz;
        float room_sectors = max_dim / 1024.0f;

        #define SIZE_SMALL  0
        #define SIZE_MEDIUM 1
        #define SIZE_LARGE  2
        #define SIZE_OPEN   3

        /* Openness bias: rooms with more portals have longer
         * sightlines through openings.  Pushes thresholds out
         * slightly so fog/LOD transitions happen further away. */
        float openness = 1.0f + r->portal_count * 0.15f;
        if (openness > 2.0f) openness = 2.0f;

        if (room_sectors < 2.0f) {
            c->size_class = SIZE_SMALL;
            c->near_dist2     = 2048.0f * 2048.0f * openness;
            c->fog_start_dist2 = 1536.0f * 1536.0f * openness;
        } else if (room_sectors < 4.0f) {
            c->size_class = SIZE_MEDIUM;
            c->near_dist2     = 4096.0f * 4096.0f * openness;
            c->fog_start_dist2 = 3072.0f * 3072.0f * openness;
        } else if (room_sectors < 8.0f) {
            c->size_class = SIZE_LARGE;
            c->near_dist2     = 6144.0f * 6144.0f * openness;
            c->fog_start_dist2 = 4096.0f * 4096.0f * openness;
        } else {
            c->size_class = SIZE_OPEN;
            c->near_dist2     = 8192.0f * 8192.0f * openness;
            c->fog_start_dist2 = 6144.0f * 6144.0f * openness;
        }

        /* Max draw distance from size + connectivity */
        float draw_dist = max_dim * 8.0f;
        draw_dist += r->portal_count * 1024.0f;
        if (draw_dist < 7168.0f) draw_dist = 7168.0f;
        if (draw_dist > 16384.0f) draw_dist = 16384.0f;
        c->max_draw_dist2 = draw_dist * draw_dist;

        /* Per-room fog color from ambient light + water flag.
         * Dark rooms get darker fog, water rooms get blue tint.
         * TR1 ambient: 0 = bright, 8191 = dark. */
        float amb = 1.0f - ((float)r->ambient / 8191.0f);
        if (amb < 0.0f) amb = 0.0f;
        if (amb > 1.0f) amb = 1.0f;
        bool is_water = (r->flags & 1) != 0;
        if (is_water) {
            /* Blue-green underwater haze */
            c->fog_r = 0.08f + amb * 0.10f;
            c->fog_g = 0.15f + amb * 0.15f;
            c->fog_b = 0.25f + amb * 0.20f;
        } else {
            /* Cave fog: darker rooms get darker, warmer fog */
            c->fog_r = 0.20f + amb * 0.15f;
            c->fog_g = 0.18f + amb * 0.14f;
            c->fog_b = 0.22f + amb * 0.18f;
        }

        c->lod        = CHUNK_LOD_NEAR;
        c->dist_sq    = 0.0f;
        c->visible    = false;
        c->cluster_id = -1;
    }
}

/*
 * Build spatial clusters from portal connectivity.
 * Each cluster groups up to MAX_CLUSTER_CHILDREN adjacent rooms.
 * Rooms are assigned to clusters via greedy BFS: start from an
 * unassigned room, pull in portal-connected neighbors.
 */
static void build_clusters(const tr_level_t *level) {
    g_cluster_count = 0;
    bool assigned[TR_MAX_ROOMS];
    memset(assigned, 0, sizeof(assigned));

    for (int seed = 0; seed < g_chunk_count && g_cluster_count < MAX_CLUSTERS; seed++) {
        if (assigned[seed]) continue;

        tr_cluster_t *cl = &g_clusters[g_cluster_count];
        cl->child_count = 0;

        /* Seed the cluster with this room */
        cl->children[cl->child_count++] = seed;
        assigned[seed] = true;
        g_chunks[seed].cluster_id = g_cluster_count;

        /* Pull in portal-connected neighbors up to MAX_CLUSTER_CHILDREN */
        for (int ci = 0; ci < cl->child_count && cl->child_count < MAX_CLUSTER_CHILDREN; ci++) {
            int ri = cl->children[ci];
            if (ri < 0 || ri >= level->room_count) continue;
            const tr_room_t *room = &level->rooms[ri];
            for (int p = 0; p < room->portal_count && cl->child_count < MAX_CLUSTER_CHILDREN; p++) {
                int dest = room->portals[p].room;
                if (dest >= 0 && dest < g_chunk_count && !assigned[dest]) {
                    assigned[dest] = true;
                    cl->children[cl->child_count++] = dest;
                    g_chunks[dest].cluster_id = g_cluster_count;
                }
            }
        }

        /* Compute combined bounding sphere from children */
        float min_x = 1e18f, max_x = -1e18f;
        float min_y = 1e18f, max_y = -1e18f;
        float min_z = 1e18f, max_z = -1e18f;
        for (int ci = 0; ci < cl->child_count; ci++) {
            tr_chunk_t *c = &g_chunks[cl->children[ci]];
            if (c->min_x < min_x) min_x = c->min_x;
            if (c->max_x > max_x) max_x = c->max_x;
            if (c->min_y < min_y) min_y = c->min_y;
            if (c->max_y > max_y) max_y = c->max_y;
            if (c->min_z < min_z) min_z = c->min_z;
            if (c->max_z > max_z) max_z = c->max_z;
        }
        cl->cx = (min_x + max_x) * 0.5f;
        cl->cy = (min_y + max_y) * 0.5f;
        cl->cz = (min_z + max_z) * 0.5f;
        float dx = max_x - min_x;
        float dy = max_y - min_y;
        float dz = max_z - min_z;
        cl->radius   = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
        cl->dist_sq  = 0.0f;
        cl->face_skip = 1;

        g_cluster_count++;
    }
}

/*
 * Sphere–frustum test (kept for potential future use but currently
 * unused — portal visibility replaced it).
 */
static bool __attribute__((unused)) chunk_frustum_visible(
                                  const tr_chunk_t *c,
                                  float cam_x, float cam_z,
                                  float sy, float cy_ang) {
    float dx = c->cx - cam_x;
    float dz = c->cz - cam_z;
    float r  = c->radius;

    /* Rotate to camera-space XZ */
    float rx = dx * cy_ang - dz * sy;
    float rz = dx * sy     + dz * cy_ang;

    /* Behind camera? (rz < 0 is behind; allow partial overlap with near plane) */
    if (rz < -r) return false;

    /* Beyond far clip? */
    if (rz > CHUNK_FAR_WORLD + r) return false;

    /* Outside right frustum edge?  (rx * sin_h - rz * cos_h > r) */
    if (rx * CHUNK_SIN_H - rz * CHUNK_COS_H > r) return false;

    /* Outside left frustum edge?  (-rx * sin_h - rz * cos_h > r) */
    if (-rx * CHUNK_SIN_H - rz * CHUNK_COS_H > r) return false;

    return true;
}

/* -------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------- */

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    joypad_init();

    printf("TRX-N64: Tomb Raider 1\n");
    printf("N64 + SC64 Port\n");
    printf("========================\n\n");

    n64_mem_init();

    /* Try to unlock SC64 */
    printf("Unlocking SC64...\n");
    bool sc64_ok = sc64_unlock();
    if (sc64_ok) {
        printf("  SC64 unlocked!\n");
    } else {
        printf("  SC64 not detected (emulator?)\n");
    }

    /*
     * Load assets via libdragon's filesystem.
     * Put tr1_assets.bin on the SD card as a regular file,
     * and read it with fopen/fread. No raw sector access needed.
     */
    /*
     * Load assets from ROM cartridge space via PI DMA.
     *
     * The ROM layout:
     *   0x000000 - 0x000FFF  Header + IPL3
     *   0x001000 - 0x01XXXX  Game code (ELF/binary)
     *   ASSET_ROM_OFFSET+    Asset archive (tr1_assets.bin)
     *
     * The N64 maps cartridge ROM to PI address 0x10000000.
     * So assets are at 0x10000000 + ASSET_ROM_OFFSET.
     *
     * We read via PI DMA: cart address → RDRAM.
     */
    #define ASSET_ROM_OFFSET    0x100000  /* 1 MB into ROM (plenty of room for code) */
    #define ROM_CART_BASE       0x10000000UL
    #define ASSET_CART_ADDR     (ROM_CART_BASE + ASSET_ROM_OFFSET)

    printf("Loading assets from ROM...\n");
    printf("  ROM offset: 0x%X\n", ASSET_ROM_OFFSET);

    /* Read archive header from ROM via libdragon's dma_read */
    static uint8_t header_buf[512] __attribute__((aligned(16)));
    dma_read(header_buf, ASSET_CART_ADDR, 512);

    printf("  Magic: %c%c%c%c (0x%02x%02x%02x%02x)\n",
           header_buf[0], header_buf[1], header_buf[2], header_buf[3],
           header_buf[0], header_buf[1], header_buf[2], header_buf[3]);

    /* Check if the archive magic is valid */
    uint32_t magic = header_buf[0] | (header_buf[1]<<8) | (header_buf[2]<<16) | (header_buf[3]<<24);
    if (magic == 0x53433634) {  /* "SC64" in little-endian */
        printf("  Archive found in ROM!\n");

        /* Set up a ROM-based backend for asset_init */
        /* The asset manager needs a read function — provide one that does PI DMA */
        g_has_assets = true;

        /* For now: read the whole archive header to show stats */
        uint32_t asset_count = header_buf[8] | (header_buf[9]<<8) |
                               (header_buf[10]<<16) | (header_buf[11]<<24);
        uint32_t total_sectors = header_buf[20] | (header_buf[21]<<8) |
                                 (header_buf[22]<<16) | (header_buf[23]<<24);
        printf("  Assets: %u\n", asset_count);
        printf("  Total: %u sectors (%u KB)\n", total_sectors, total_sectors / 2);
    } else {
        printf("  No archive at ROM offset 0x%X\n", ASSET_ROM_OFFSET);
        printf("  Build with: cat trx-n64.z64 padding assets > full.z64\n");
    }

    /* If we have assets in ROM, load level 1 via PI DMA */
    if (g_has_assets) {
        printf("\nLoading LEVEL1 (Caves)...\n");

        /* Parse archive index to find asset 1 */
        uint32_t index_sector = header_buf[12] | (header_buf[13]<<8) |
                                (header_buf[14]<<16) | (header_buf[15]<<24);
        uint32_t asset_count_val = header_buf[8] | (header_buf[9]<<8) |
                                   (header_buf[10]<<16) | (header_buf[11]<<24);

        /* Read index from ROM via dma_read (libdragon handles alignment) */
        uint32_t index_bytes = asset_count_val * 20;
        if (index_bytes > 4096) index_bytes = 4096;
        static uint8_t index_buf[4096] __attribute__((aligned(16)));
        uint32_t dma_len = (index_bytes + 15) & ~15;  /* Round up to 16 */
        dma_read(index_buf, ASSET_CART_ADDR + index_sector * 512, dma_len);

        /* Find asset ID 1 in index */
        uint32_t level_sector = 0;
        uint32_t level_size = 0;
        uint32_t level_compressed = 0;
        uint16_t level_flags = 0;
        for (uint32_t i = 0; i < asset_count_val && i * 20 < index_bytes; i++) {
            uint8_t *e = &index_buf[i * 20];
            uint32_t aid = e[0]|(e[1]<<8)|(e[2]<<16)|(e[3]<<24);
            if (aid == 1) {
                level_sector = e[4]|(e[5]<<8)|(e[6]<<16)|(e[7]<<24);
                level_flags = e[10]|(e[11]<<8);
                level_compressed = e[12]|(e[13]<<8)|(e[14]<<16)|(e[15]<<24);
                level_size = e[16]|(e[17]<<8)|(e[18]<<16)|(e[19]<<24);
                break;
            }
        }

        printf("  Found: sector=%u, size=%u, compressed=%u, flags=0x%x\n",
               level_sector, level_size, level_compressed, level_flags);

        /* Read level data from ROM via PI DMA.
         * If compressed (flags & 1), read compressed bytes, decompress,
         * then free the compressed buffer. */
        bool is_compressed = (level_flags & 1) && level_compressed < level_size;
        uint32_t read_size = is_compressed ? level_compressed : level_size;
        uint32_t read_bytes = (read_size + 511) & ~511;
        read_bytes = (read_bytes + 15) & ~15;  /* DMA alignment */

        n64_mem_init();
        uint8_t *level_data = NULL;

        if (is_compressed) {
            uint8_t *comp_buf = (uint8_t *)malloc(read_bytes + 16);
            level_data = (uint8_t *)n64_mem_alloc(level_size + 4096);
            printf("  Compressed: %u KB -> %u KB\n",
                   level_compressed / 1024, level_size / 1024);
            if (!comp_buf || !level_data) {
                printf("  ALLOC FAILED (comp=%p level=%p)\n", comp_buf, level_data);
                if (comp_buf) free(comp_buf);
                level_data = NULL;
            } else if (level_sector > 0) {
                uint32_t cart_addr = ASSET_CART_ADDR + level_sector * 512;
                dma_read(comp_buf, cart_addr, read_bytes);
                printf("  Loaded %u KB, first: %02x %02x %02x %02x\n",
                       read_bytes / 1024,
                       comp_buf[0], comp_buf[1], comp_buf[2], comp_buf[3]);
                int dec = lz4_decode_block(comp_buf, level_data,
                                           level_compressed, level_size);
                free(comp_buf);  /* Release compressed buffer immediately */
                if (dec < 0) {
                    printf("  LZ4 FAILED (error %d)\n", dec);
                    level_data = NULL;
                } else {
                    printf("  Decompressed %d bytes OK\n", dec);
                }
            }
        } else {
            level_data = (uint8_t *)n64_mem_alloc(read_bytes + 4096);
            printf("  Alloc %u KB... ", (read_bytes + 4096) / 1024);
            if (!level_data) {
                printf("FAILED\n");
            } else if (level_sector > 0) {
                uint32_t cart_addr = ASSET_CART_ADDR + level_sector * 512;
                dma_read(level_data, cart_addr, read_bytes);
                printf("  Loaded %u KB\n", read_bytes / 1024);
            }
        }

        if (level_data && level_sector > 0) {
            printf("  Parsing .PHD format...\n");
            tr_parse_result_t res = tr_level_parse(
                (const uint8_t *)level_data, level_size, &g_level);

            if (res == TR_PARSE_OK) {
                printf("  Parse OK!\n\n");

                /* Show level stats */
                int total_faces = 0, total_verts = 0;
                for (int i = 0; i < g_level.room_count; i++) {
                    total_faces += g_level.rooms[i].mesh.face_count;
                    total_verts += g_level.rooms[i].mesh.vertex_count;
                }

                int lara_room = -1;
                for (int i = 0; i < g_level.item_count; i++) {
                    if (g_level.items[i].object_id == 0) {
                        lara_room = g_level.items[i].room;
                        break;
                    }
                }

                printf("=== LEVEL 1: CAVES ===\n");
                printf("  Rooms:    %d\n", g_level.room_count);
                printf("  Faces:    %d\n", total_faces);
                printf("  Vertices: %d\n", total_verts);
                printf("  Items:    %d\n", (int)g_level.item_count);
                printf("  Textures: %d (%d pages)\n",
                       (int)g_level.object_texture_count,
                       (int)g_level.tex_page_count);
                printf("  Anims:    %d\n", (int)g_level.anim_count);
                printf("  Lara:     room %d\n", lara_room);
                printf("  Heap:     %u KB used\n",
                       (GAME_HEAP_SIZE - n64_mem_available()) / 1024);
                printf("  Sound:    details=%d indices=%d data=%d bytes\n",
                       (int)g_level.sound_detail_count,
                       (int)g_level.sample_index_count,
                       (int)g_level.sample_data_size);
                if (g_level.sample_data && g_level.sample_index_count > 0) {
                    int32_t off0 = g_level.sample_indices[0];
                    uint8_t *w = g_level.sample_data + off0;
                    printf("  Sample[0]: off=%d, hdr=%c%c%c%c\n",
                           off0, w[0], w[1], w[2], w[3]);
                }
                printf("Preparing textures...\n");
                console_render();

                /* Pre-convert all object textures to 16x16 RGBA16 at load time.
                 * Each tile gets its own permanent buffer — no per-frame conversion,
                 * no buffer reuse, no CI8/TLUT TMEM issues. */
                int num_tex_pages = g_level.tex_page_count;
                if (num_tex_pages > 16) num_tex_pages = 16;

                /* Build RGBA16 palette lookup */
                uint16_t pal16[256];
                for (int i = 0; i < 256; i++) {
                    uint8_t r = g_level.palette[i*3+0];
                    uint8_t g = g_level.palette[i*3+1];
                    uint8_t b = g_level.palette[i*3+2];
                    pal16[i] = ((r >> 1) << 11) | ((g >> 1) << 6) | ((b >> 1) << 1) | 1;
                    if (i == 0) pal16[i] = 0;
                }

                #define TEX_TILE_DIM 32
                #define TEX_TILE_BYTES (TEX_TILE_DIM * TEX_TILE_DIM * 2)
                int num_otex = g_level.object_texture_count;
                /* Use ALL object textures — expansion pak has plenty of RDRAM */

                printf("  Heap before tex: %u KB free\n",
                       n64_mem_available() / 1024);

                /* Allocate texture pixel data via uncached malloc (not from
                 * the bump heap — it's full with level data + parser allocs).
                 * tex_tiles is temporary — freed after CI8 generation below.
                 * tex_surfs (RGBA16 surfaces) no longer needed (all rendering
                 * uses CI8 now), so we skip that allocation entirely. */
                uint16_t *tex_tiles = (uint16_t *)malloc_uncached_aligned(
                    64, num_otex * TEX_TILE_BYTES);
                /* Per-tile UV mapping: u0, v0, scale_s, scale_t */
                float *tex_uv_map = (float *)n64_mem_alloc(num_otex * 4 * sizeof(float));

                if (tex_tiles && tex_uv_map) {
                    printf("  Converting %d textures to RGBA16 (temp)...\n", num_otex);
                    console_render();
                    for (int i = 0; i < num_otex; i++) {
                        tr_object_texture_t *otex = &g_level.object_textures[i];
                        uint16_t *tile = tex_tiles + i * TEX_TILE_DIM * TEX_TILE_DIM;

                        /* Compute UV bounding box */
                        int u0 = 255, v0 = 255, u1 = 0, v1 = 0;
                        for (int k = 0; k < 4; k++) {
                            int u = otex->uv[k].u >> 8;
                            int v = otex->uv[k].v >> 8;
                            if (u < u0) u0 = u;
                            if (v < v0) v0 = v;
                            if (u > u1) u1 = u;
                            if (v > v1) v1 = v;
                        }
                        int tw = u1 - u0 + 1;
                        int th = v1 - v0 + 1;
                        if (tw < 1) tw = 1;
                        if (th < 1) th = 1;

                        /* Store UV mapping (used by CI8 generation + render loop) */
                        tex_uv_map[i*4+0] = (float)u0;
                        tex_uv_map[i*4+1] = (float)v0;
                        tex_uv_map[i*4+2] = (float)TEX_TILE_DIM / (float)tw;
                        tex_uv_map[i*4+3] = (float)TEX_TILE_DIM / (float)th;

                        /* Convert CI8 tile to RGBA16, subsample to TEX_TILE_DIM.
                         * Writing through uncached pointer — goes straight to RDRAM. */
                        if (otex->tex_page < num_tex_pages) {
                            uint8_t *page = g_level.tex_pages + otex->tex_page * 256 * 256;
                            for (int y = 0; y < TEX_TILE_DIM; y++) {
                                int sy = v0 + y * th / TEX_TILE_DIM;
                                if (sy > 255) sy = 255;
                                for (int x = 0; x < TEX_TILE_DIM; x++) {
                                    int sx = u0 + x * tw / TEX_TILE_DIM;
                                    if (sx > 255) sx = 255;
                                    tile[y * TEX_TILE_DIM + x] = pal16[page[sy * 256 + sx]];
                                }
                            }
                        } else {
                            /* Invalid tex page — fill with gray */
                            for (int p = 0; p < TEX_TILE_DIM * TEX_TILE_DIM; p++)
                                tile[p] = 0x8421;
                        }

                    }
                }

                /* Build CI8 tiles for the RDPQ textured pass.
                 * TR1 source textures ARE already CI8 (palette-indexed), so
                 * we just read raw palette indices instead of converting to
                 * RGBA16.  Paired with the global pal16 TLUT uploaded once
                 * per frame: CI8 32×32 = 1 KB TMEM (vs 2 KB for RGBA16). */
                uint8_t *ci8_tiles = NULL;
                surface_t *ci8_surfs = NULL;
                if (num_otex > 0) {
                    ci8_tiles = (uint8_t *)malloc_uncached_aligned(
                        64, num_otex * TEX_TILE_DIM * TEX_TILE_DIM);
                    ci8_surfs = (surface_t *)n64_mem_alloc(num_otex * sizeof(surface_t));
                }
                if (ci8_tiles && ci8_surfs) {
                    for (int i = 0; i < num_otex; i++) {
                        tr_object_texture_t *otex = &g_level.object_textures[i];
                        uint8_t *ctile = ci8_tiles + i * TEX_TILE_DIM * TEX_TILE_DIM;
                        int u0 = (int)tex_uv_map[i*4+0];
                        int v0 = (int)tex_uv_map[i*4+1];
                        int tw = (int)(TEX_TILE_DIM / tex_uv_map[i*4+2] + 0.5f);
                        int th = (int)(TEX_TILE_DIM / tex_uv_map[i*4+3] + 0.5f);
                        if (tw < 1) tw = 1;
                        if (th < 1) th = 1;
                        if (otex->tex_page < num_tex_pages) {
                            uint8_t *page = g_level.tex_pages + otex->tex_page * 256 * 256;
                            for (int y = 0; y < TEX_TILE_DIM; y++) {
                                int sy = v0 + y * th / TEX_TILE_DIM;
                                if (sy > 255) sy = 255;
                                for (int x = 0; x < TEX_TILE_DIM; x++) {
                                    int sx = u0 + x * tw / TEX_TILE_DIM;
                                    if (sx > 255) sx = 255;
                                    ctile[y * TEX_TILE_DIM + x] = page[sy * 256 + sx];
                                }
                            }
                        } else {
                            memset(ctile, 0, TEX_TILE_DIM * TEX_TILE_DIM);
                        }
                        ci8_surfs[i] = surface_make_linear(ctile, FMT_CI8,
                                                           TEX_TILE_DIM, TEX_TILE_DIM);
                    }
                }
                /* Free RGBA16 tex_tiles — no longer needed (all rendering
                 * uses CI8 now). This reclaims ~1.8 MB of uncached RDRAM. */
                if (tex_tiles) {
                    free_uncached(tex_tiles);
                    tex_tiles = NULL;
                    printf("  Freed RGBA16 tex_tiles: %d KB uncached\n",
                           num_otex * TEX_TILE_BYTES / 1024);
                }

                /* level_data stays in bump heap — can't free but stable. */

                printf("  %d/%d CI8 textures ready (%d KB). Heap: %u KB left.\n",
                       num_otex, (int)g_level.object_texture_count,
                       num_otex * TEX_TILE_DIM * TEX_TILE_DIM / 1024,
                       n64_mem_available() / 1024);

                /* ---- Memory audit ---- */
                printf("\n=== MEMORY AUDIT ===\n");
                printf("  Bump heap:  %u KB used, %u KB free (of %u KB)\n",
                       (GAME_HEAP_SIZE - n64_mem_available()) / 1024,
                       n64_mem_available() / 1024,
                       GAME_HEAP_SIZE / 1024);
                printf("  CI8 textures: %d × %dx%d CI8 = %d KB (uncached)\n",
                       num_otex, TEX_TILE_DIM, TEX_TILE_DIM,
                       num_otex * TEX_TILE_DIM * TEX_TILE_DIM / 1024);
                printf("  Framebuffers: 2 × 320×240 × 16bpp = %d KB\n", 2*320*240*2/1024);
                printf("  Z-buffer: 320×240 × 16bpp = %d KB\n", 320*240*2/1024);
                printf("  Tex surfs: %d × %d = %d KB (heap)\n",
                       num_otex, (int)sizeof(surface_t),
                       (int)(num_otex * sizeof(surface_t)) / 1024);
                printf("  UV map: %d KB (heap)\n",
                       (int)(num_otex * 4 * sizeof(float)) / 1024);
                printf("====================\n\n");
                console_render();

                /* Create a 1x1 solid white texture for flat-colored faces.
                 * When uploaded, TEX0 = white, so TEX0 * SHADE = SHADE. */
                uint16_t *white_px = (uint16_t *)malloc_uncached_aligned(64, 8);
                surface_t white_surf = {0};
                if (white_px) {
                    white_px[0] = 0xFFFF; /* RGBA16 white, full alpha */
                    white_surf = surface_make_linear(white_px, FMT_RGBA16, 1, 1);
                }

                /* Debug distance-tier visualization: 5 solid-color 1×1 textures.
                 * Toggle via pause menu → global budgets page.
                 * Colors: Near=GREEN, MidFar=CYAN, Far=YELLOW, XFar=ORANGE, XXFar=RED */
                bool g_debug_dist_colors = false;
                /* Force all rooms to render as a specific tier (0=off, 1-5=tier) */
                int g_debug_force_tier = 0;
                /* Each debug pixel needs its own 8-byte aligned address
                 * for rdpq_tex_upload. Allocate 5 × 8 bytes = 40 bytes. */
                uint16_t *dbg_tier_px = (uint16_t *)malloc_uncached_aligned(8, 5 * 8);
                surface_t dbg_tier_surf[5] = {{0}};
                if (dbg_tier_px) {
                    /* RGBA16: RRRRR GGGGG BBBBB A — each at 8-byte stride */
                    dbg_tier_px[0*4] = (0<<11)|(31<<6)|(0<<1)|1;   /* near:    green */
                    dbg_tier_px[1*4] = (0<<11)|(31<<6)|(31<<1)|1;  /* mid-far: cyan */
                    dbg_tier_px[2*4] = (31<<11)|(31<<6)|(0<<1)|1;  /* far:     yellow */
                    dbg_tier_px[3*4] = (31<<11)|(16<<6)|(0<<1)|1;  /* x-far:   orange */
                    dbg_tier_px[4*4] = (31<<11)|(0<<6)|(0<<1)|1;   /* xx-far:  red */
                    for (int i = 0; i < 5; i++)
                        dbg_tier_surf[i] = surface_make_linear(&dbg_tier_px[i*4], FMT_RGBA16, 1, 1);
                }

                /* On-demand mip buffer — one reusable 16×16 RGBA16 surface.
                 * Instead of pre-allocating mips for all 880 textures,
                 * box-filter on-the-fly for the ~8 uploads per frame.
                 * Only 512 bytes uncached — negligible memory cost. */
                /* ---- Pre-baked room tiles (Lambert-style) ----
                 * Load per-room baked textures from ROM if available.
                 * Each room has 3 mip levels: 32×32, 16×16, 8×8.
                 * Baked pass uploads pre-baked tile = zero runtime filtering.
                 *
                 * Falls back to on-demand box filtering if baked data
                 * is not in ROM (for development without full pipeline). */
                #define MIP0_DIM 32
                #define MIP1_DIM 16
                #define MIP2_DIM 8
                #define BAKED_BYTES_PER_ROOM_FULL (MIP0_DIM*MIP0_DIM*2 + MIP1_DIM*MIP1_DIM*2 + MIP2_DIM*MIP2_DIM*2)
                /* Only load mip1+mip2 (skip mip0) to save uncached RDRAM.
                 * mip0 (32×32) = 2048 bytes/room — too expensive for 38 rooms.
                 * mip1+mip2 = 640 bytes/room × 38 = ~24 KB. */
                #define BAKED_MIP1_OFFSET (MIP0_DIM*MIP0_DIM*2)
                #define BAKED_MIP2_OFFSET (MIP0_DIM*MIP0_DIM*2 + MIP1_DIM*MIP1_DIM*2)
                #define BAKED_LOAD_PER_ROOM (MIP1_DIM*MIP1_DIM*2 + MIP2_DIM*MIP2_DIM*2)
                #define BAKED_HEADER_SIZE 16

                /* Baked tile offsets from ROM.
                 * Try build/ first (where build_full_rom.py generates it),
                 * then fall back to current directory. */
                #if __has_include("baked_offsets.h")
                #include "baked_offsets.h"
                #elif __has_include("../build/baked_offsets.h")
                #include "../build/baked_offsets.h"
                #else
                #define BAKED_ROM_OFFSET 0u
                #define BAKED_ROM_SIZE   0u
                #endif

                printf("  Baked ROM: offset=0x%X size=%u\n",
                       (unsigned)BAKED_ROM_OFFSET, (unsigned)BAKED_ROM_SIZE);
                console_render();

                int baked_room_count = 0;
                /* BAK3: 5 mip tiers per room — CI8, CI4, IA8, IA4 */
                uint8_t   *baked_data = NULL;
                uint16_t **bkd_m1_pal = NULL;   /* CI8 palette */
                surface_t *bkd_m1_surf = NULL;  /* 16×16 CI8 */
                uint16_t **bkd_m2_pal = NULL;   /* CI4 palette */
                surface_t *bkd_m2_surf = NULL;  /* 8×8 CI4 */
                surface_t *bkd_m3_surf = NULL;  /* 8×8 IA8 */
                surface_t *bkd_m4_surf = NULL;  /* 4×4 IA4 */

                #define BAK3_PER_ROOM   2952
                #define BAK3_MIP0_SIZE  (32*32*2)
                #define BAK3_M1PAL_SZ   (256*2)
                #define BAK3_M1IDX_SZ   (16*16)
                #define BAK3_M2PAL_SZ   (16*2)
                #define BAK3_M2DAT_SZ   (8*8/2)
                #define BAK3_M3_SZ      (8*8)
                #define BAK3_M4_SZ      (4*4/2)
                #define BAK3_SKIP       BAK3_MIP0_SIZE
                #define BAK3_LOAD_PER   (BAK3_M1PAL_SZ + BAK3_M1IDX_SZ + BAK3_M2PAL_SZ + BAK3_M2DAT_SZ + BAK3_M3_SZ + BAK3_M4_SZ)

                if (BAKED_ROM_OFFSET > 0 && BAKED_ROM_SIZE > BAKED_HEADER_SIZE) {
                    static uint8_t bake_hdr[16] __attribute__((aligned(16)));
                    dma_read(bake_hdr, ROM_CART_BASE + BAKED_ROM_OFFSET, 16);

                    if (bake_hdr[0]=='B' && bake_hdr[1]=='A' &&
                        bake_hdr[2]=='K' && bake_hdr[3]=='3') {
                        baked_room_count = bake_hdr[4] | (bake_hdr[5] << 8);
                        printf("  BAK3: %d rooms (CI8+CI4+IA8+IA4)\n",
                               baked_room_count);

                        int total_load = baked_room_count * BAK3_LOAD_PER;
                        baked_data  = (uint8_t *)n64_mem_alloc(total_load + 64);
                        bkd_m1_pal  = (uint16_t **)n64_mem_alloc(baked_room_count * sizeof(void*));
                        bkd_m1_surf = (surface_t *)n64_mem_alloc(baked_room_count * sizeof(surface_t));
                        bkd_m2_pal  = (uint16_t **)n64_mem_alloc(baked_room_count * sizeof(void*));
                        bkd_m2_surf = (surface_t *)n64_mem_alloc(baked_room_count * sizeof(surface_t));
                        bkd_m3_surf = (surface_t *)n64_mem_alloc(baked_room_count * sizeof(surface_t));
                        bkd_m4_surf = (surface_t *)n64_mem_alloc(baked_room_count * sizeof(surface_t));

                        if (baked_data && bkd_m1_surf && bkd_m4_surf) {
                            for (int bi = 0; bi < baked_room_count; bi++) {
                                uint32_t rom_off = ROM_CART_BASE +
                                    BAKED_ROM_OFFSET + BAKED_HEADER_SIZE +
                                    bi * BAK3_PER_ROOM + BAK3_SKIP;
                                uint8_t *d = baked_data + bi * BAK3_LOAD_PER;
                                dma_read(d, rom_off, BAK3_LOAD_PER);

                                int off = 0;
                                bkd_m1_pal[bi] = (uint16_t *)(d + off);
                                off += BAK3_M1PAL_SZ;
                                bkd_m1_surf[bi] = surface_make_linear(
                                    d + off, FMT_CI8, MIP1_DIM, MIP1_DIM);
                                off += BAK3_M1IDX_SZ;
                                bkd_m2_pal[bi] = (uint16_t *)(d + off);
                                off += BAK3_M2PAL_SZ;
                                bkd_m2_surf[bi] = surface_make_linear(
                                    d + off, FMT_CI4, MIP2_DIM, MIP2_DIM);
                                off += BAK3_M2DAT_SZ;
                                bkd_m3_surf[bi] = surface_make_linear(
                                    d + off, FMT_IA8, 8, 8);
                                off += BAK3_M3_SZ;
                                bkd_m4_surf[bi] = surface_make_linear(
                                    d + off, FMT_IA4, 4, 4);
                            }
                            /* Invalidate cache — DMA wrote directly to RDRAM,
                             * CPU cache has stale data. Must invalidate so
                             * rdpq_tex_upload reads the DMA'd baked tiles. */
                            data_cache_hit_writeback_invalidate(baked_data, total_load);
                            printf("  BAK3 loaded: %d KB cached\n",
                                   total_load / 1024);
                        } else {
                            printf("  BAK3 alloc failed\n");
                            baked_room_count = 0;
                        }
                    } else {
                        printf("  No BAK3 in ROM (got: %c%c%c%c)\n",
                               bake_hdr[0], bake_hdr[1], bake_hdr[2], bake_hdr[3]);
                    }
                } else {
                    printf("  No baked data in ROM\n");
                }

                /* CPU mip buffers removed — baked pass now uses baked room tiles
                 * as the default for all far rooms (no per-face mip generation). */

                /* ============================================
                 * AUDIO INIT
                 * ============================================ */
                audio_init(22050, 2);  /* 22050 Hz, 2 buffers */
                mixer_init(8);        /* 8 mixer channels */

                /* Parse TR1 WAV samples — find PCM data in each RIFF chunk */
                int16_t **sample_pcm = NULL;
                int32_t *sample_len = NULL;
                int32_t *sample_rate_arr = NULL;
                int parsed_samples = 0;
                if (g_level.sample_data && g_level.sample_indices &&
                    g_level.sample_index_count > 0) {
                    int ns = g_level.sample_index_count;
                    if (ns > 64) ns = 64;
                    sample_pcm = (int16_t **)n64_mem_alloc(ns * sizeof(int16_t *));
                    sample_len = (int32_t *)n64_mem_alloc(ns * sizeof(int32_t));
                    sample_rate_arr = (int32_t *)n64_mem_alloc(ns * sizeof(int32_t));
                    if (sample_pcm && sample_len && sample_rate_arr) {
                        for (int i = 0; i < ns; i++) {
                            sample_pcm[i] = NULL;
                            sample_len[i] = 0;
                            sample_rate_arr[i] = 11025;
                            int32_t off = g_level.sample_indices[i];
                            if (off < 0 || off >= g_level.sample_data_size) continue;
                            uint8_t *wav = g_level.sample_data + off;
                            int remain = g_level.sample_data_size - off;
                            /* Parse RIFF WAV header to find PCM data */
                            if (remain > 44 &&
                                wav[0]=='R' && wav[1]=='I' && wav[2]=='F' && wav[3]=='F') {
                                /* Find "fmt " chunk for sample rate */
                                int sr = 11025;
                                for (int p = 12; p + 8 < remain; p++) {
                                    if (wav[p]=='f'&&wav[p+1]=='m'&&wav[p+2]=='t'&&wav[p+3]==' ') {
                                        sr = wav[p+12]|(wav[p+13]<<8)|(wav[p+14]<<16)|(wav[p+15]<<24);
                                        break;
                                    }
                                }
                                /* Find "data" chunk for PCM samples */
                                for (int p = 12; p + 8 < remain; p++) {
                                    if (wav[p]=='d'&&wav[p+1]=='a'&&wav[p+2]=='t'&&wav[p+3]=='a') {
                                        int32_t dlen = wav[p+4]|(wav[p+5]<<8)|(wav[p+6]<<16)|(wav[p+7]<<24);
                                        if (dlen > 0 && p + 8 + dlen <= remain) {
                                            sample_pcm[i] = (int16_t *)(wav + p + 8);
                                            sample_len[i] = dlen / 2; /* bytes to samples */
                                            sample_rate_arr[i] = sr;
                                            parsed_samples++;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    printf("  Audio: %d/%d samples parsed\n", parsed_samples, ns);
                    console_render();
                }

                /* Pre-sort room faces by texture for batched uploads.
                 * Z-buffer handles depth, so spatial order doesn't matter.
                 * This MASSIVELY reduces texture DMA — the #1 bottleneck. */
                for (int r = 0; r < g_level.room_count; r++) {
                    tr_face_t *faces = g_level.rooms[r].mesh.faces;
                    int fc = g_level.rooms[r].mesh.face_count;
                    for (int i = 1; i < fc; i++) {
                        tr_face_t tmp = faces[i];
                        uint16_t key = tmp.texture & 0x7FFF;
                        int j = i - 1;
                        while (j >= 0 && (faces[j].texture & 0x7FFF) > key) {
                            faces[j + 1] = faces[j];
                            j--;
                        }
                        faces[j + 1] = tmp;
                    }
                }

                /* Classify faces, sort by texture, build chunks/clusters */
                classify_faces(&g_level);
                sort_faces_by_texture(&g_level);
                build_chunks(&g_level);
                build_clusters(&g_level);
                printf("  %d chunks, %d clusters built.\n",
                       g_chunk_count, g_cluster_count);

                /* Pre-compute shadow circle points (avoid sinf/cosf per frame) */
                float shadow_circ[16][2];
                {
                    float srad = 120.0f;
                    for (int p = 0; p < 16; p++) {
                        float a = (float)p * 6.28318f / 16.0f;
                        shadow_circ[p][0] = srad * sinf(a);
                        shadow_circ[p][1] = srad * cosf(a);
                    }
                }

                /* ============================================
                 * 3D RENDERING LOOP
                 * ============================================
                 * Switch from console text to RDP graphics.
                 * Free-fly camera with analog stick.
                 * Flat-shaded room triangles via rdpq.
                 */

                /* Close console, init display.  Pure RDPQ pipeline — no GL. */
                console_close();
                display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
                rdpq_init();
                bool has_zbuf = true;  /* all passes use z-buffer */

                /* Camera state */
                float cam_x = 0, cam_y = 0, cam_z = 0;
                float cam_rot_y = 0, cam_rot_x = 0;  /* yaw + pitch */

                /* Find Lara's start position */
                float spawn_x = 0, spawn_y = 0, spawn_z = 0, spawn_rot = 0;
                for (int i = 0; i < g_level.item_count; i++) {
                    if (g_level.items[i].object_id == 0) {
                        spawn_rot = (float)g_level.items[i].rotation * 3.14159f / 32768.0f;
                        spawn_x = (float)g_level.items[i].pos.x - 600.0f * sinf(spawn_rot);
                        spawn_y = (float)g_level.items[i].pos.y - 768;
                        spawn_z = (float)g_level.items[i].pos.z - 600.0f * cosf(spawn_rot);
                        break;
                    }
                }
                cam_x = spawn_x; cam_y = spawn_y; cam_z = spawn_z; cam_rot_y = spawn_rot;

                /* Sector grid accessor — used by room tracking, collision,
                 * and spawn room resolution.  TR1 layout: X-major order. */
                #define SECTOR_AT(_room, _lx, _lz) \
                    (&(_room)->sectors[(_lx) * (_room)->sectors_z + (_lz)])

                /* Keep lara_room as Lara's item room at spawn.
                 * The camera is offset (600 behind + 768 up) which may
                 * land in a different room, but portal visibility must
                 * start from Lara's actual room — matching the original
                 * TRX engine behavior.  Per-frame tracking will adjust
                 * as the camera moves. */

                /* Use proper sin/cos (Taylor approximations break at large angles) */
                #define WRAP_ANGLE(a) ((a) > 3.14159f ? (a) - 6.28318f : ((a) < -3.14159f ? (a) + 6.28318f : (a)))

                /* FPS counter */
                int fps_frames = 0;
                uint32_t fps_last = TICKS_READ();
                int fps_display = 0;
                bool collision_on = false;  /* L = tcl toggle */
                int speed_level = 1;

                /* ============================================================
                 * BUDGET SYSTEM — 2-tier: global + per-room
                 *
                 * Global budgets are the defaults (adjustable via pause menu).
                 * Per-room budgets override globals but are clamped to never
                 * exceed the global max. A per-room value of -1 means "use global."
                 * Effective budget = (per_room >= 0) ? min(per_room, global) : global
                 *
                 * Shippable: per-room budgets can be tuned in-game and exported
                 * as a per-level budget table for final builds.
                 * ============================================================ */

                /* Global budgets (runtime-adjustable, seeded from compile-time defaults) */
                int g_bkd_tri_budget      = 450;
                int g_bkd_upload_budget    = 16;
                int g_tex_tri_budget      = 550;
                int g_tex_face_budget     = 350;
                int g_tex_upload_budget    = 96;

                /* View distance thresholds (runtime-adjustable, in world units).
                 * Stored as linear distance; squared at comparison time.
                 * Tier boundaries:
                 *   Near < mid_dist → CI8 per-face textured pass
                 *   mid_dist..very_far → CI8 16×16 baked (mid-far)
                 *   very_far..xfar    → CI4 8×8 baked (far)
                 *   xfar..xxfar       → IA8 8×8 baked (extra-far)
                 *   > xxfar           → IA4 4×4 baked (extreme-far) */
                /* Strictly monotonic: mid < vfar < xfar < xxfar.
                 * Enforced in the UI — each threshold must be > the one before it. */
                float g_dist_mid     = 7168.0f;   /* near→mid-far boundary */
                float g_dist_vfar    = 10240.0f;  /* mid-far→far boundary */
                float g_dist_xfar   = 13312.0f;  /* far→extra-far boundary */
                float g_dist_xxfar  = 16384.0f;  /* extra-far→extreme-far boundary */

                /* Baked pass depth bias: push baked vertices slightly away from
                 * camera to prevent Z-fighting with textured pass geometry.
                 * Added to rz during baked pass projection. Tunable per-level. */
                float g_baked_z_bias = 8.0f;

                /* Per-face max draw distance — geometry beyond this distance
                 * from the camera is culled regardless of room visibility.
                 * Works on top of the room-based tier system. Tunable. */
                float g_max_draw_dist = 33000.0f;
                float g_max_draw_dist_sq = 33000.0f * 33000.0f;

                /* Squared versions recomputed when thresholds change */
                float g_dist_mid_sq, g_dist_vfar_sq, g_dist_xfar_sq, g_dist_xxfar_sq;
                #define RECOMPUTE_DIST_SQ() do { \
                    g_dist_mid_sq   = g_dist_mid   * g_dist_mid;   \
                    g_dist_vfar_sq  = g_dist_vfar  * g_dist_vfar;  \
                    g_dist_xfar_sq  = g_dist_xfar  * g_dist_xfar;  \
                    g_dist_xxfar_sq = g_dist_xxfar * g_dist_xxfar;  \
                } while(0)
                RECOMPUTE_DIST_SQ();

                /* Per-room budget overrides (-1 = inherit global) */
                #define ROOM_BUDGET_INHERIT (-1)
                typedef struct {
                    int16_t tex_face_budget;
                    int16_t tex_tri_budget;
                    int16_t bkd_tri_budget;
                } room_budget_t;
                static room_budget_t room_budgets[256]; /* up to 256 rooms */
                /* Initialize all to inherit */
                for (int i = 0; i < 256; i++) {
                    room_budgets[i].tex_face_budget = (int16_t)g_tex_face_budget;
                    room_budgets[i].tex_tri_budget  = (int16_t)g_tex_tri_budget;
                    room_budgets[i].bkd_tri_budget   = (int16_t)g_bkd_tri_budget;
                }

                /* ============================================================
                 * PER-ROOM TIER STATE — hysteresis + sticky residency
                 *
                 * Tiers: 0=Near, 1=MidFar, 2=Far, 3=XFar, 4=XXFar, 5=Culled
                 * Each room tracks its current rendering tier and resists
                 * switching until the new tier is stable for TIER_HOLD frames.
                 * Hysteresis band: a room must cross threshold + HYSTERESIS
                 * to promote, or threshold - HYSTERESIS to demote.
                 * ============================================================ */
                #define TIER_NEAR   0
                #define TIER_MIDFAR 1
                #define TIER_FAR    2
                #define TIER_XFAR   3
                #define TIER_XXFAR  4
                #define TIER_CULLED 5
                #define TIER_HOLD_FRAMES 4  /* frames to hold before switching */
                #define TIER_HYSTERESIS  0.15f /* 15% of threshold as dead zone */

                typedef struct {
                    uint8_t current_tier;
                    uint8_t target_tier;
                    uint8_t hold_count;   /* frames target has been stable */
                    uint8_t fallback_tier; /* cheapest valid fallback */
                } room_tier_state_t;
                static room_tier_state_t room_tiers[256];
                for (int i = 0; i < 256; i++) {
                    room_tiers[i].current_tier = TIER_NEAR;
                    room_tiers[i].target_tier = TIER_NEAR;
                    room_tiers[i].hold_count = TIER_HOLD_FRAMES;
                    room_tiers[i].fallback_tier = TIER_XXFAR;
                }

                /* Effective budget helpers — computed per frame from current room */
                #define EFF_BUDGET(per_room, global) \
                    ((per_room) >= 0 ? ((per_room) < (global) ? (per_room) : (global)) : (global))
                int eff_tex_face_budget = 600, eff_tex_tri_budget = 1400, eff_bkd_tri_budget = 200;

                /* Legacy #defines for code that still references them (pause display) */
                #define BKD_TRI_BUDGET      eff_bkd_tri_budget
                #define BKD_UPLOAD_BUDGET   g_bkd_upload_budget
                #define TEX_TRI_BUDGET     eff_tex_tri_budget
                #define TEX_FACE_BUDGET    eff_tex_face_budget
                #define TEX_UPLOAD_BUDGET  g_tex_upload_budget

                /* Per-frame perf counters (updated each frame, shown on pause) */
                /* Per-frame performance counters — reset each frame,
                 * displayed on pause screen for diagnostics. */
                int perf_bkd_rooms = 0;
                int perf_ring_counts[6] = {0}; /* rooms found per ring depth */
                int perf_bkd_tris = 0;
                int perf_bkd_uploads = 0;
                int perf_bkd_baked = 0;     /* rooms using baked tile */
                int perf_tex_rooms = 0;
                int perf_tex_tris = 0;
                int perf_tex_binds = 0;
                int perf_tex_culled = 0;   /* faces back-culled */
                int perf_tex_detail_skip = 0; /* detail faces skipped */
                int perf_tex_faces = 0;    /* total face iterations (including culled) */
                int perf_vis_total = 0;     /* total visible rooms */
                int perf_vis_ring1 = 0;     /* ring 1 rooms */
                int perf_vis_ring2 = 0;     /* ring 2 rooms */
                uint32_t perf_frame_start = 0;
                uint32_t perf_frame_us = 0; /* last frame time in µs */
                uint32_t perf_bkd_us = 0;   /* CPU time: baked pass */
                uint32_t perf_tex_us = 0;  /* CPU time: textured pass */
                uint32_t perf_rsp_us = 0;   /* RSP sync stall at frame end */
                uint32_t perf_tex_room_us = 0;  /* near sub-timing: rooms */
                uint32_t perf_tex_sm_us = 0;    /* near sub-timing: static meshes */
                uint32_t perf_tex_lara_us = 0;  /* near sub-timing: Lara */
                uint32_t perf_tex_item_us = 0;  /* near sub-timing: items */

                /* Active fog color — lerps toward current room's fog.
                 * Starts at neutral cave fog. */
                float active_fog_r = 0.35f;
                float active_fog_g = 0.32f;
                float active_fog_b = 0.40f;

                /* Render loop */
                while (1) {
                    /* Update FPS every 30 frames */
                    fps_frames++;
                    if (fps_frames >= 30) {
                        uint32_t now = TICKS_READ();
                        uint32_t delta = TICKS_DISTANCE(fps_last, now);
                        float secs = (float)delta / (float)TICKS_PER_SECOND;
                        fps_display = (int)(30.0f / secs + 0.5f);
                        fps_last = now;
                        fps_frames = 0;
                    }
                    joypad_poll();
                    joypad_inputs_t input = joypad_get_inputs(JOYPAD_PORT_1);
                    joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

                    /* Controls */
                    if (pressed.r) { speed_level++; if (speed_level > 3) speed_level = 0; }
                    if (pressed.l) collision_on = !collision_on;
                    float speed_mult[] = { 8.0f, 32.0f, 128.0f, 512.0f };
                    float move_speed = speed_mult[speed_level];
                    if (input.btn.z) move_speed *= 4.0f;

                    /* Joystick: look around (X = turn, Y = pitch up/down) */
                    if (input.stick_x > 20 || input.stick_x < -20)
                        cam_rot_y += input.stick_x * 0.0012f;
                    if (input.stick_y > 20 || input.stick_y < -20)
                        cam_rot_x -= input.stick_y * 0.0008f;
                    cam_rot_y = WRAP_ANGLE(cam_rot_y);
                    /* Clamp pitch to ~80 degrees */
                    if (cam_rot_x > 1.3f) cam_rot_x = 1.3f;
                    if (cam_rot_x < -1.3f) cam_rot_x = -1.3f;

                    float sy = sinf(cam_rot_y);
                    float cy = cosf(cam_rot_y);

                    /* Compute desired movement */
                    float dx = 0, dy = 0, dz = 0;
                    if (input.btn.c_up) { dx += move_speed * sy; dz += move_speed * cy; }
                    if (input.btn.c_down) { dx -= move_speed * sy; dz -= move_speed * cy; }
                    if (input.btn.c_left) { dx -= move_speed * cy; dz += move_speed * sy; }
                    if (input.btn.c_right) { dx += move_speed * cy; dz -= move_speed * sy; }
                    if (input.btn.d_left) { dx -= move_speed * cy; dz += move_speed * sy; }
                    if (input.btn.d_right) { dx += move_speed * cy; dz -= move_speed * sy; }
                    if (input.btn.d_up) dy -= move_speed;
                    if (input.btn.d_down) dy += move_speed;

                    /* B button: teleport back to spawn */
                    if (pressed.b) {
                        cam_x = spawn_x; cam_y = spawn_y; cam_z = spawn_z;
                        cam_rot_y = spawn_rot; cam_rot_x = 0;
                    }

                    /* ===== SECTOR COLLISION SYSTEM =====
                     * Floor/ceiling: value * 256 = world Y (positive = down) */
                    #define GET_SECTOR_HEIGHTS(wx, wz, wy, out_floor, out_ceil) do { \
                        int32_t _ix=(int32_t)(wx), _iz=(int32_t)(wz), _iy=(int32_t)(wy); \
                        int _best_room = lara_room; \
                        tr_room_t *_cr = &g_level.rooms[lara_room]; \
                        int32_t _rx = _ix - _cr->x, _rz = _iz - _cr->z; \
                        if (_rx < 0 || _rx >= _cr->sectors_x*1024 || \
                            _rz < 0 || _rz >= _cr->sectors_z*1024) { \
                            bool _found = false; \
                            for (int _p = 0; _p < _cr->portal_count && !_found; _p++) { \
                                int _d = _cr->portals[_p].room; \
                                if (_d < 0 || _d >= g_level.room_count) continue; \
                                tr_room_t *_pr = &g_level.rooms[_d]; \
                                _rx = _ix - _pr->x; _rz = _iz - _pr->z; \
                                if (_rx >= 0 && _rx < _pr->sectors_x*1024 && \
                                    _rz >= 0 && _rz < _pr->sectors_z*1024) { \
                                    _best_room = _d; _found = true; \
                                } \
                            } \
                            if (!_found) { \
                                for (int _ri = 0; _ri < g_level.room_count; _ri++) { \
                                    tr_room_t *_rm = &g_level.rooms[_ri]; \
                                    _rx = _ix - _rm->x; _rz = _iz - _rm->z; \
                                    if (_rx >= 0 && _rx < _rm->sectors_x*1024 && \
                                        _rz >= 0 && _rz < _rm->sectors_z*1024) { \
                                        _best_room = _ri; break; \
                                    } \
                                } \
                            } \
                        } \
                        tr_room_t *_r = &g_level.rooms[_best_room]; \
                        int _sx = (_ix - _r->x) >> 10, _sz = (_iz - _r->z) >> 10; \
                        if (_sx < 0) _sx = 0; \
                        if (_sx >= _r->sectors_x) _sx = _r->sectors_x-1; \
                        if (_sz < 0) _sz = 0; \
                        if (_sz >= _r->sectors_z) _sz = _r->sectors_z-1; \
                        if (_r->sectors) { \
                            tr_sector_t *_s = SECTOR_AT(_r, _sx, _sz); \
                            (out_floor) = _s->floor * 256; \
                            (out_ceil) = _s->ceiling * 256; \
                            if (_s->pit_room != 255 && _s->pit_room < g_level.room_count && \
                                _iy > (out_floor)) { \
                                tr_room_t *_bl = &g_level.rooms[_s->pit_room]; \
                                int _bx = (_ix-_bl->x)>>10, _bz = (_iz-_bl->z)>>10; \
                                if (_bx>=0 && _bx<_bl->sectors_x && _bz>=0 && _bz<_bl->sectors_z && _bl->sectors) { \
                                    (out_floor) = SECTOR_AT(_bl,_bx,_bz)->floor * 256; \
                                    (out_ceil) = SECTOR_AT(_bl,_bx,_bz)->ceiling * 256; \
                                } \
                            } \
                            if (_s->sky_room != 255 && _s->sky_room < g_level.room_count && \
                                _iy < (out_ceil)) { \
                                tr_room_t *_al = &g_level.rooms[_s->sky_room]; \
                                int _ax = (_ix-_al->x)>>10, _az = (_iz-_al->z)>>10; \
                                if (_ax>=0 && _ax<_al->sectors_x && _az>=0 && _az<_al->sectors_z && _al->sectors) { \
                                    (out_floor) = SECTOR_AT(_al,_ax,_az)->floor * 256; \
                                    (out_ceil) = SECTOR_AT(_al,_ax,_az)->ceiling * 256; \
                                } \
                            } \
                        } else { (out_floor)=0; (out_ceil)=-10000; } \
                    } while(0)

                    if (collision_on) {
                        /* TCL mode: wall + floor + ceiling collision.
                         * D-pad up/down disabled, gravity pulls to floor. */
                        dy = 0;
                        static float vy = 0;
                        vy += 16.0f;  /* gravity */

                        /* Wall collision: check destination AND a probe point
                         * 512 units ahead (prevents skipping thin wall sectors) */
                        #define IS_WALL(fx, fz, cy, cf, cc) do { \
                            GET_SECTOR_HEIGHTS(fx, fz, cy, cf, cc); \
                        } while(0)
                        #define BLOCKED(fx, fz, cfl, ccl) \
                            ((cfl - ccl) < 512 || (cfl - cur_floor) < -768)

                        if (dx != 0 || dz != 0) {
                            int32_t cur_floor, cur_ceil;
                            GET_SECTOR_HEIGHTS(cam_x, cam_z, cam_y, cur_floor, cur_ceil);

                            /* Normalize movement direction for probe */
                            float len = sqrtf(dx*dx + dz*dz);
                            float nx = dx / len, nz = dz / len;
                            /* Probe 512 units ahead of destination */
                            float probe_x = cam_x + dx + nx * 512.0f;
                            float probe_z = cam_z + dz + nz * 512.0f;

                            int32_t df, dc, pf, pc;
                            GET_SECTOR_HEIGHTS(cam_x + dx, cam_z + dz, cam_y, df, dc);
                            GET_SECTOR_HEIGHTS(probe_x, probe_z, cam_y, pf, pc);

                            bool wall = BLOCKED(cam_x+dx, cam_z+dz, df, dc) ||
                                        BLOCKED(probe_x, probe_z, pf, pc);
                            if (wall) {
                                /* Try X-only slide */
                                float px2 = cam_x + dx + nx * 512.0f;
                                GET_SECTOR_HEIGHTS(cam_x + dx, cam_z, cam_y, df, dc);
                                GET_SECTOR_HEIGHTS(px2, cam_z, cam_y, pf, pc);
                                if (BLOCKED(cam_x+dx, cam_z, df, dc) ||
                                    BLOCKED(px2, cam_z, pf, pc))
                                    dx = 0;
                                /* Try Z-only slide */
                                float pz2 = cam_z + dz + nz * 512.0f;
                                GET_SECTOR_HEIGHTS(cam_x, cam_z + dz, cam_y, df, dc);
                                GET_SECTOR_HEIGHTS(cam_x, pz2, cam_y, pf, pc);
                                if (BLOCKED(cam_x, cam_z+dz, df, dc) ||
                                    BLOCKED(cam_x, pz2, pf, pc))
                                    dz = 0;
                            }
                        }
                        #undef IS_WALL
                        #undef BLOCKED

                        cam_x += dx; cam_z += dz;
                        cam_y += vy;

                        /* Clamp to floor/ceiling after movement */
                        int32_t fl, cl;
                        GET_SECTOR_HEIGHTS(cam_x, cam_z, cam_y, fl, cl);
                        float eye_floor = (float)fl - 768.0f;
                        float eye_ceil = (float)cl + 128.0f;
                        if (cam_y > eye_floor) { cam_y = eye_floor; vy = 0; }
                        if (cam_y < eye_ceil) { cam_y = eye_ceil; vy = 0; }
                    } else {
                        /* Noclip: free fly, no restrictions */
                        cam_x += dx; cam_y += dy; cam_z += dz;
                    }

                    /* Room tracking — Y-aware using sector floor/ceiling.
                     * First find room by XZ (current → portals → 2-deep),
                     * then resolve vertically via pit_room / sky_room so
                     * overlapping rooms (e.g. cave + area above) are
                     * disambiguated correctly. */
                    {
                        int cam_room = lara_room;
                        int32_t icx = (int32_t)cam_x, icy = (int32_t)cam_y, icz = (int32_t)cam_z;
                        tr_room_t *cur = &g_level.rooms[lara_room];
                        int32_t rx = icx - cur->x, rz = icz - cur->z;
                        bool found = (rx>=0 && rx<cur->sectors_x*1024 && rz>=0 && rz<cur->sectors_z*1024);
                        if (!found) {
                            for (int p=0; p<cur->portal_count && !found; p++) {
                                int d1=cur->portals[p].room;
                                if(d1<0||d1>=g_level.room_count)continue;
                                tr_room_t *rm=&g_level.rooms[d1];
                                rx=icx-rm->x; rz=icz-rm->z;
                                if(rx>=0&&rx<rm->sectors_x*1024&&rz>=0&&rz<rm->sectors_z*1024){cam_room=d1;found=true;break;}
                                for(int q=0;q<rm->portal_count&&!found;q++){
                                    int d2=rm->portals[q].room;
                                    if(d2<0||d2>=g_level.room_count)continue;
                                    tr_room_t *rm2=&g_level.rooms[d2];
                                    rx=icx-rm2->x;rz=icz-rm2->z;
                                    if(rx>=0&&rx<rm2->sectors_x*1024&&rz>=0&&rz<rm2->sectors_z*1024){cam_room=d2;found=true;}
                                }
                            }
                        }

                        /* Resolve vertical: walk sector pit/sky portals
                         * until camera Y is between floor and ceiling. */
                        for (int iter = 0; iter < 8; iter++) {
                            tr_room_t *rm = &g_level.rooms[cam_room];
                            if (!rm->sectors) break;
                            int sx = (icx - rm->x) >> 10, sz = (icz - rm->z) >> 10;
                            if (sx < 0) { sx = 0; } if (sx >= rm->sectors_x) { sx = rm->sectors_x - 1; }
                            if (sz < 0) { sz = 0; } if (sz >= rm->sectors_z) { sz = rm->sectors_z - 1; }
                            tr_sector_t *s = SECTOR_AT(rm, sx, sz);
                            int32_t fl = s->floor * 256, cl = s->ceiling * 256;
                            if (icy > fl && s->pit_room != 255 && s->pit_room < g_level.room_count) {
                                cam_room = s->pit_room;
                            } else if (icy < cl && s->sky_room != 255 && s->sky_room < g_level.room_count) {
                                cam_room = s->sky_room;
                            } else {
                                break;
                            }
                        }

                        lara_room = cam_room;
                    }

                    /* ============================================================
                     * PAUSE MENU — Multi-page diagnostics + budget sliders
                     *
                     * Page 0: Hardware diagnostics (read-only)
                     * Page 1: Global budget sliders (adjustable)
                     * Page 2: Per-room budget sliders (adjustable, clamped to global)
                     *
                     * Navigation: L=prev page  R=next page  Start=unpause
                     * On slider pages: D-up/D-down=select  D-left/D-right=adjust
                     * ============================================================ */
                    if (pressed.start) {
                        static int pause_page = 0;
                        static int slider_sel = 0;
                        int hold_timer = 0; /* throttle held d-pad adjustments */

                        /* Snapshot current budgets — edits happen on copies,
                         * applied only on unpause */
                        int edit_g_tex_face   = g_tex_face_budget;
                        int edit_g_tex_tri    = g_tex_tri_budget;
                        int edit_g_tex_upload = g_tex_upload_budget;
                        int edit_g_bkd_tri     = g_bkd_tri_budget;
                        int edit_g_bkd_upload  = g_bkd_upload_budget;
                        bool edit_debug_dist   = g_debug_dist_colors;
                        int  edit_force_tier   = g_debug_force_tier;
                        float edit_dist_mid    = g_dist_mid;
                        float edit_dist_vfar   = g_dist_vfar;
                        float edit_dist_xfar   = g_dist_xfar;
                        float edit_dist_xxfar  = g_dist_xxfar;
                        float edit_z_bias      = g_baked_z_bias;
                        float edit_max_draw    = g_max_draw_dist;
                        room_budget_t edit_room_budgets[256];
                        memcpy(edit_room_budgets, room_budgets, sizeof(room_budgets));

                        bool paused = true;
                        while (paused) {
                            surface_t *pause_disp = display_get();
                            /* Clear entire framebuffer to black manually.
                             * graphics_fill_screen may not work on all libdragon builds,
                             * and we need every double-buffered surface cleared. */
                            {
                                uint16_t *fb = (uint16_t *)pause_disp->buffer;
                                int npx = pause_disp->width * pause_disp->height;
                                for (int px = 0; px < npx; px++) fb[px] = 0x0001;
                            }
                            graphics_set_color(0xFFFFFFFF, 0x00000000);
                            char buf[80];
                            int y = 14;
                            #define STAT(fmt, ...) do { \
                                snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
                                graphics_draw_text(pause_disp, 8, y, buf); y += 10; \
                            } while(0)
                            #define STAT_HL(sel, idx, fmt, ...) do { \
                                if ((sel) == (idx)) graphics_set_color(0x00FF00FF, 0x000000C0); \
                                snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
                                graphics_draw_text(pause_disp, 8, y, buf); y += 10; \
                                if ((sel) == (idx)) graphics_set_color(0xFFFFFFFF, 0x000000C0); \
                            } while(0)

                            /* Page header */
                            STAT("=== PAUSE [L<] Page %d/4 [>R] ===", pause_page);

                            if (pause_page == 0) {
                                /* ---- PAGE 0: DIAGNOSTICS ---- */
                                STAT("FPS:%d Frame:%uus (%.0f%%)",
                                     fps_display, perf_frame_us,
                                     (float)perf_frame_us / 33333.0f * 100.0f);
                                {
                                    uint32_t other_us = perf_frame_us > perf_bkd_us + perf_tex_us
                                        ? perf_frame_us - perf_bkd_us - perf_tex_us : 0;
                                    STAT("Far:%uus Near:%uus Misc:%uus",
                                         perf_bkd_us, perf_tex_us, other_us);
                                }
                                STAT("RSP stall:%uus (%s)", perf_rsp_us,
                                     perf_rsp_us > 4000 ? "RDP" :
                                     perf_rsp_us > 1000 ? "RSP" : "CPU");
                                STAT("Room:%d Vis:%d r1:%d ext:%d",
                                     lara_room, perf_vis_total,
                                     perf_vis_ring1, perf_vis_ring2);
                                STAT("Rings:%d/%d/%d/%d/%d",
                                     perf_ring_counts[0],
                                     perf_ring_counts[1], perf_ring_counts[2],
                                     perf_ring_counts[3], perf_ring_counts[4]);
                                /* Show actual distances for debugging tier thresholds */
                                {
                                    /* Find nearest non-lara visible room's dist_sq */
                                    float min_d = 999999999.0f, max_d = 0;
                                    for (int ri = 0; ri < g_chunk_count; ri++) {
                                        if (ri == lara_room || !g_chunks[ri].visible) continue;
                                        float d = g_chunks[ri].dist_sq;
                                        if (d < min_d) min_d = d;
                                        if (d > max_d) max_d = d;
                                    }
                                    STAT("VisDist:%.0f-%.0f", sqrtf(min_d), sqrtf(max_d));
                                    STAT("mid_th:%.0f mid_sq:%.0f", g_dist_mid, g_dist_mid_sq);
                                }
                                STAT("--- TEXTURED (rooms:%d) ---", perf_tex_rooms);
                                STAT("Tris:%d/%d Faces:%d/%d",
                                     perf_tex_tris, TEX_TRI_BUDGET,
                                     perf_tex_faces, TEX_FACE_BUDGET);
                                STAT("Binds:%d Cull:%d DSkip:%d",
                                     perf_tex_binds, perf_tex_culled,
                                     perf_tex_detail_skip);
                                STAT("Rm:%uus SM:%uus La:%uus It:%uus",
                                     perf_tex_room_us, perf_tex_sm_us,
                                     perf_tex_lara_us, perf_tex_item_us);
                                STAT("--- BAKED ---");
                                STAT("Rooms:%d Bk:%d/%d Tri:%d/%d",
                                     perf_bkd_rooms, perf_bkd_baked,
                                     baked_room_count,
                                     perf_bkd_tris, BKD_TRI_BUDGET);
                                STAT("--- MEM ---");
                                {
                                    uint32_t hf = n64_mem_available();
                                    STAT("Heap:%uKB/%uKB free:%uKB",
                                         (GAME_HEAP_SIZE-hf)/1024,
                                         GAME_HEAP_SIZE/1024, hf/1024);
                                }
                                STAT("Pos:%.0f %.0f %.0f", cam_x, cam_y, cam_z);

                            } else if (pause_page == 1) {
                                /* ---- PAGE 1: GLOBAL BUDGET SLIDERS ---- */
                                STAT("=== GLOBAL BUDGETS ===");
                                STAT("D-pad Up/Down=select Left/Right=adjust");
                                STAT("");
                                STAT_HL(slider_sel, 0, "Tex Face Budget: %d", edit_g_tex_face);
                                STAT_HL(slider_sel, 1, "Tex Tri Budget:  %d", edit_g_tex_tri);
                                STAT_HL(slider_sel, 2, "Tex Upload Cap:  %d", edit_g_tex_upload);
                                STAT_HL(slider_sel, 3, "Bkd Tri Budget:   %d", edit_g_bkd_tri);
                                STAT_HL(slider_sel, 4, "Bkd Upload Cap:   %d", edit_g_bkd_upload);
                                STAT("");
                                STAT_HL(slider_sel, 5, "Dist Debug Colors: %s",
                                        edit_debug_dist ? "ON" : "OFF");
                                {
                                    static const char *tier_names[] = {
                                        "OFF", "Near", "MidFar", "Far", "XFar", "XXFar"
                                    };
                                    STAT_HL(slider_sel, 6, "Force All Tier: %s",
                                            tier_names[edit_force_tier]);
                                }
                                if (edit_debug_dist || edit_force_tier) {
                                    STAT(" GREEN=Near CYAN=MidFar");
                                    STAT(" YELLOW=Far ORANGE=XFar RED=XXFar");
                                }
                                STAT("");
                                STAT("(applied on unpause)");

                            } else if (pause_page == 2) {
                                /* ---- PAGE 2: PER-ROOM BUDGET SLIDERS ---- */
                                int ri = (lara_room >= 0 && lara_room < 256) ? lara_room : 0;
                                STAT("=== ROOM %d BUDGETS ===", ri);
                                STAT("D-pad Up/Down=select Left/Right=adjust");
                                STAT("(-1 = inherit global)");
                                STAT("");
                                STAT_HL(slider_sel, 0, "Tex Face: %d (global:%d)",
                                        (int)edit_room_budgets[ri].tex_face_budget, edit_g_tex_face);
                                STAT_HL(slider_sel, 1, "Tex Tri:  %d (global:%d)",
                                        (int)edit_room_budgets[ri].tex_tri_budget, edit_g_tex_tri);
                                STAT_HL(slider_sel, 2, "Bkd Tri:   %d (global:%d)",
                                        (int)edit_room_budgets[ri].bkd_tri_budget, edit_g_bkd_tri);
                                STAT("");
                                STAT("Effective:");
                                STAT("  Face:   %d",
                                     EFF_BUDGET(edit_room_budgets[ri].tex_face_budget, edit_g_tex_face));
                                STAT("  Tri:    %d",
                                     EFF_BUDGET(edit_room_budgets[ri].tex_tri_budget, edit_g_tex_tri));
                                STAT("  FarTri: %d",
                                     EFF_BUDGET(edit_room_budgets[ri].bkd_tri_budget, edit_g_bkd_tri));
                                STAT("");
                                STAT("Room size: %s",
                                     ri < g_chunk_count ?
                                     (g_chunks[ri].size_class == 0 ? "small" :
                                      g_chunks[ri].size_class == 1 ? "medium" :
                                      g_chunks[ri].size_class == 2 ? "large" : "open") : "?");
                            } else if (pause_page == 3) {
                                /* ---- PAGE 3: VIEW DISTANCE THRESHOLDS ---- */
                                STAT("=== VIEW DISTANCE TIERS ===");
                                STAT("D-pad Up/Down=select Left/Right=adjust");
                                STAT("(units = TR1 world units)");
                                STAT("");
                                STAT_HL(slider_sel, 0, "Near->MidFar: %.0f", edit_dist_mid);
                                STAT_HL(slider_sel, 1, "MidFar->Far:  %.0f", edit_dist_vfar);
                                STAT_HL(slider_sel, 2, "Far->XFar:    %.0f", edit_dist_xfar);
                                STAT_HL(slider_sel, 3, "XFar->XXFar:  %.0f", edit_dist_xxfar);
                                STAT("");
                                STAT_HL(slider_sel, 4, "Baked Z-Bias: %.0f", edit_z_bias);
                                STAT_HL(slider_sel, 5, "Max Draw Dist: %.0f", edit_max_draw);
                                STAT("");
                                STAT("Tier colors (debug):");
                                STAT(" <%.0f        GREEN  Near", edit_dist_mid);
                                STAT(" %.0f-%.0f   CYAN   MidFar", edit_dist_mid, edit_dist_vfar);
                                STAT(" %.0f-%.0f  YELLOW Far", edit_dist_vfar, edit_dist_xfar);
                                STAT(" %.0f-%.0f  ORANGE XFar", edit_dist_xfar, edit_dist_xxfar);
                                STAT(" >%.0f       RED    XXFar", edit_dist_xxfar);

                            } else if (pause_page == 4) {
                                /* ---- PAGE 4: ROOM VISIBILITY DUMP ---- */
                                STAT("=== VISIBLE ROOMS ===");
                                STAT("Tex:%d Bkd:%d Total:%d",
                                     perf_tex_rooms, perf_bkd_rooms, perf_vis_total);
                                STAT("T=tex B=bkd *=lara");
                                for (int ri = 0; ri < g_chunk_count && y < 230; ri++) {
                                    if (!g_chunks[ri].visible) continue;
                                    float dist = sqrtf(g_chunks[ri].dist_sq);
                                    const char *pass = (g_chunks[ri].lod == CHUNK_LOD_NEAR) ? "T" : "B";
                                    const char *mark = (ri == lara_room) ? "*" : " ";
                                    STAT("%s%s R%d d:%.0f", mark, pass, ri, dist);
                                }
                            }

                            #undef STAT
                            #undef STAT_HL
                            display_show(pause_disp);

                            /* Input handling */
                            joypad_poll();
                            joypad_buttons_t bp = joypad_get_buttons_pressed(JOYPAD_PORT_1);
                            joypad_buttons_t bh = joypad_get_buttons(JOYPAD_PORT_1);

                            /* Throttle held d-pad: only adjust every 6 frames (~200ms at 30fps) */
                            bool hold_tick = false;
                            if (bh.d_left || bh.d_right) {
                                hold_timer++;
                                if (hold_timer >= 6) { hold_tick = true; hold_timer = 0; }
                            } else {
                                hold_timer = 0;
                            }
                            /* First press always triggers immediately */
                            if (bp.d_left || bp.d_right) hold_tick = true;

                            /* Page navigation */
                            if (bp.r) { pause_page++; slider_sel = 0; if (pause_page > 4) pause_page = 4; }
                            if (bp.l) { pause_page--; slider_sel = 0; if (pause_page < 0) pause_page = 0; }
                            if (bp.start) { paused = false; }

                            /* Slider navigation + adjustment (edits copies only) */
                            if (pause_page == 1) {
                                int max_sel = 6; /* 0-4=budgets, 5=debug colors, 6=force tier */
                                if (bp.d_up && slider_sel > 0) slider_sel--;
                                if (bp.d_down && slider_sel < max_sel) slider_sel++;

                                /* Toggle for debug dist colors */
                                if (slider_sel == 5 && (bp.d_left || bp.d_right))
                                    edit_debug_dist = !edit_debug_dist;
                                /* Cycle force tier: OFF→Near→MidFar→Far→XFar→XXFar→OFF */
                                if (slider_sel == 6 && bp.d_right)
                                    { edit_force_tier++; if (edit_force_tier > 5) edit_force_tier = 0; }
                                if (slider_sel == 6 && bp.d_left)
                                    { edit_force_tier--; if (edit_force_tier < 0) edit_force_tier = 5; }

                                int delta = 0;
                                if (hold_tick && bh.d_right) delta = 50;
                                if (hold_tick && bh.d_left)  delta = -50;

                                if (delta) {
                                    switch (slider_sel) {
                                        case 0: edit_g_tex_face += delta;
                                                if (edit_g_tex_face < 50)   edit_g_tex_face = 50;
                                                if (edit_g_tex_face > 5000) edit_g_tex_face = 5000;
                                                break;
                                        case 1: edit_g_tex_tri += delta;
                                                if (edit_g_tex_tri < 50)    edit_g_tex_tri = 50;
                                                if (edit_g_tex_tri > 5000)  edit_g_tex_tri = 5000;
                                                break;
                                        case 2: edit_g_tex_upload += delta / 10;
                                                if (edit_g_tex_upload < 4)  edit_g_tex_upload = 4;
                                                if (edit_g_tex_upload > 128) edit_g_tex_upload = 128;
                                                break;
                                        case 3: edit_g_bkd_tri += delta;
                                                if (edit_g_bkd_tri < 50)     edit_g_bkd_tri = 50;
                                                if (edit_g_bkd_tri > 2000)   edit_g_bkd_tri = 2000;
                                                break;
                                        case 4: edit_g_bkd_upload += delta / 10;
                                                if (edit_g_bkd_upload < 2)   edit_g_bkd_upload = 2;
                                                if (edit_g_bkd_upload > 32)  edit_g_bkd_upload = 32;
                                                break;
                                    }
                                }
                            } else if (pause_page == 2) {
                                int ri = (lara_room >= 0 && lara_room < 256) ? lara_room : 0;
                                int max_sel = 2;
                                if (bp.d_up && slider_sel > 0) slider_sel--;
                                if (bp.d_down && slider_sel < max_sel) slider_sel++;

                                int delta = 0;
                                if (hold_tick && bh.d_right) delta = 50;
                                if (hold_tick && bh.d_left)  delta = -50;

                                if (delta) {
                                    int16_t *target = NULL;
                                    int gmax = 0;
                                    switch (slider_sel) {
                                        case 0: target = &edit_room_budgets[ri].tex_face_budget;
                                                gmax = edit_g_tex_face; break;
                                        case 1: target = &edit_room_budgets[ri].tex_tri_budget;
                                                gmax = edit_g_tex_tri; break;
                                        case 2: target = &edit_room_budgets[ri].bkd_tri_budget;
                                                gmax = edit_g_bkd_tri; break;
                                    }
                                    if (target) {
                                        int val = (int)*target + delta;
                                        if (val < -1) val = -1;
                                        if (val > gmax) val = gmax;
                                        if (val >= 0 && val < 50) val = 50;
                                        *target = (int16_t)val;
                                    }
                                }
                            } else if (pause_page == 3) {
                                int max_sel = 5; /* 0-3=distances, 4=z-bias, 5=draw dist */
                                if (bp.d_up && slider_sel > 0) slider_sel--;
                                if (bp.d_down && slider_sel < max_sel) slider_sel++;

                                /* Z-bias adjustment (slider 4) */
                                if (slider_sel == 4) {
                                    if (hold_tick && bh.d_right) edit_z_bias += 2.0f;
                                    if (hold_tick && bh.d_left)  edit_z_bias -= 2.0f;
                                    if (edit_z_bias < 0.0f) edit_z_bias = 0.0f;
                                    if (edit_z_bias > 128.0f) edit_z_bias = 128.0f;
                                }
                                /* Max draw distance (slider 5) */
                                if (slider_sel == 5 && hold_tick) {
                                    float step = (edit_max_draw < 2000) ? 100.0f :
                                                 (edit_max_draw < 8000) ? 500.0f : 1000.0f;
                                    if (bh.d_right) edit_max_draw += step;
                                    if (bh.d_left)  edit_max_draw -= step;
                                    if (edit_max_draw < 500.0f) edit_max_draw = 500.0f;
                                    if (edit_max_draw > 65536.0f) edit_max_draw = 65536.0f;
                                }

                                /* Step scales with current value for fine control at low ranges */
                                float fdelta = 0;
                                if (hold_tick && slider_sel < 4) {
                                    float *t[] = { &edit_dist_mid, &edit_dist_vfar,
                                                   &edit_dist_xfar, &edit_dist_xxfar };
                                    float cur = *t[slider_sel];
                                    float step = (cur < 1024) ? 64.0f : (cur < 4096) ? 128.0f : 256.0f;
                                    if (bh.d_right) fdelta = step;
                                    if (bh.d_left)  fdelta = -step;
                                }

                                if (fdelta != 0 && slider_sel < 4) {
                                    float *tt[] = { &edit_dist_mid, &edit_dist_vfar,
                                                    &edit_dist_xfar, &edit_dist_xxfar };
                                    *tt[slider_sel] += fdelta;
                                    /* Clamp: min 1 (essentially zero), max 65536 */
                                    if (*tt[slider_sel] < 1.0f)  *tt[slider_sel] = 1.0f;
                                    if (*tt[slider_sel] > 65536.0f) *tt[slider_sel] = 65536.0f;
                                    /* Enforce monotonic: mid < vfar < xfar < xxfar.
                                     * Push neighbors apart by 256 minimum gap. */
                                    for (int k = slider_sel; k < 3; k++)
                                        if (*tt[k+1] <= *tt[k]) *tt[k+1] = *tt[k] + 128.0f;
                                    for (int k = slider_sel; k > 0; k--)
                                        if (*tt[k-1] >= *tt[k]) *tt[k-1] = *tt[k] - 128.0f;
                                    /* Re-clamp after push */
                                    for (int k = 0; k < 4; k++) {
                                        if (*tt[k] < 1.0f)  *tt[k] = 1.0f;
                                        if (*tt[k] > 65536.0f) *tt[k] = 65536.0f;
                                    }
                                }
                            }
                        }

                        /* Apply edits on unpause */
                        g_tex_face_budget   = edit_g_tex_face;
                        g_tex_tri_budget    = edit_g_tex_tri;
                        g_tex_upload_budget = edit_g_tex_upload;
                        g_bkd_tri_budget     = edit_g_bkd_tri;
                        g_bkd_upload_budget  = edit_g_bkd_upload;
                        g_debug_dist_colors  = edit_debug_dist;
                        g_debug_force_tier   = edit_force_tier;
                        g_dist_mid   = edit_dist_mid;
                        g_dist_vfar  = edit_dist_vfar;
                        g_dist_xfar  = edit_dist_xfar;
                        g_dist_xxfar = edit_dist_xxfar;
                        g_baked_z_bias = edit_z_bias;
                        g_max_draw_dist = edit_max_draw;
                        g_max_draw_dist_sq = edit_max_draw * edit_max_draw;
                        RECOMPUTE_DIST_SQ();
                        memcpy(room_budgets, edit_room_budgets, sizeof(room_budgets));
                        continue;
                    }

                    /* ===== BEGIN FRAME =====
                     * Frame order:
                     *   1. rdpq_attach (framebuffer + zbuf)
                     *   2. rdpq_clear (color + depth)
                     *   3. RDPQ baked pass  (CI8/IA4 baked tiles, no zbuf)
                     *   4. RDPQ textured pass (CI8 per-face, zbuf on)
                     *   5. rdpq_detach_show()
                     *
                     * All geometry uses pure RDPQ.  No GL. */
                    /* Compute effective budgets for this frame's room */
                    {
                        int ri = (lara_room >= 0 && lara_room < 256) ? lara_room : 0;
                        eff_tex_face_budget = EFF_BUDGET(room_budgets[ri].tex_face_budget, g_tex_face_budget);
                        eff_tex_tri_budget  = EFF_BUDGET(room_budgets[ri].tex_tri_budget,  g_tex_tri_budget);
                        eff_bkd_tri_budget   = EFF_BUDGET(room_budgets[ri].bkd_tri_budget,   g_bkd_tri_budget);
                    }

                    /* Reset per-frame counters + start frame timer */
                    perf_bkd_rooms = 0; perf_bkd_tris = 0;
                    perf_bkd_uploads = 0; perf_bkd_baked = 0;
                    perf_tex_rooms = 0; perf_tex_tris = 0;
                    perf_tex_binds = 0; perf_tex_culled = 0;
                    perf_tex_detail_skip = 0; perf_tex_faces = 0;
                    perf_frame_start = TICKS_READ();

                    surface_t *disp = display_get();
                    rdpq_attach(disp, display_get_zbuf());

                    /* Clear color + depth via proper rdpq API.
                     * Using rdpq_clear instead of manual set_mode_fill +
                     * fill_rectangle to avoid RDP mode tracking conflicts
                     * with the subsequent set_mode_standard for the baked pass. */
                    /* Clear to fog color (not black) so sub-pixel gaps
                     * between triangles blend in instead of showing stark
                     * black seams.  Matches the far-pass fog target color. */
                    /* Clear to fog color so gaps/undrawn areas blend in */
                    /* Lerp active fog toward current room's fog color.
                     * Slow blend (0.08/frame) prevents harsh snapping. */
                    if (lara_room >= 0 && lara_room < g_chunk_count) {
                        float lr = 0.08f; /* lerp rate */
                        active_fog_r += (g_chunks[lara_room].fog_r - active_fog_r) * lr;
                        active_fog_g += (g_chunks[lara_room].fog_g - active_fog_g) * lr;
                        active_fog_b += (g_chunks[lara_room].fog_b - active_fog_b) * lr;
                    }

                    /* Clear to active fog color so void matches the fog */
                    rdpq_clear(RGBA32(
                        (uint8_t)(active_fog_r * 255.0f),
                        (uint8_t)(active_fog_g * 255.0f),
                        (uint8_t)(active_fog_b * 255.0f), 0xFF));
                    rdpq_clear_z(0xFFFC);

                    /* Precompute pitch sin/cos for CPU projection */
                    float sp = sinf(cam_rot_x);
                    float cp = cosf(cam_rot_x);

                    /* =====================================================
                     * VISIBILITY + STREAMING
                     *
                     * Render policy (authoritative — never drops geometry):
                     *   1. Current room: ALWAYS visible.
                     *   2. Connected rooms: ALWAYS visible.
                     *
                     * Streaming policy (portal-driven priority hints):
                     *   Portal on screen / near center → HIGH priority
                     *   Portal near frustum edge       → MEDIUM
                     *   Portal behind camera            → LOW
                     *   Used for texture tier prewarming, not visibility.
                     *
                     * LOD policy: separate from both.
                     * =================================================== */
                    #define MAX_DRAW_ROOMS 32
                    #define STREAM_PRI_HIGH   2
                    #define STREAM_PRI_MED    1
                    #define STREAM_PRI_LOW    0
                    int vis_rooms[MAX_DRAW_ROOMS];
                    int vis_count = 0;
                    int vis_stream_pri[MAX_DRAW_ROOMS]; /* streaming priority hints (future use) */
                    (void)vis_stream_pri; /* suppresses set-but-unused warning */

                    /* Rule 1: current room — always, highest priority */
                    if (lara_room >= 0 && lara_room < g_chunk_count) {
                        g_chunks[lara_room].dist_sq = 0.0f;
                        g_chunks[lara_room].visible = true;
                        vis_rooms[vis_count] = lara_room;
                        vis_stream_pri[vis_count] = STREAM_PRI_HIGH;
                        vis_count++;
                    }

                    /* Rule 2: connected rooms — always visible.
                     * Portal check sets streaming priority only. */
                    if (lara_room >= 0 && lara_room < g_level.room_count) {
                        tr_room_t *lr = &g_level.rooms[lara_room];
                        for (int p = 0; p < lr->portal_count && vis_count < MAX_DRAW_ROOMS; p++) {
                            tr_portal_t *portal = &lr->portals[p];
                            int dest = portal->room;
                            if (dest < 0 || dest >= g_chunk_count) continue;

                            /* Avoid duplicates */
                            bool dup = false;
                            for (int vi = 0; vi < vis_count; vi++) {
                                if (vis_rooms[vi] == dest) { dup = true; break; }
                            }
                            if (dup) continue;

                            /* Streaming hint: check portal camera visibility
                             * to set priority (HIGH/MED/LOW). This does NOT
                             * affect whether the room renders. */
                            int priority = STREAM_PRI_LOW;
                            int verts_in_front = 0;
                            float pmin_sx = 320, pmax_sx = 0;

                            for (int pv = 0; pv < 4; pv++) {
                                float px = (float)(lr->x + portal->vertices[pv].x) - cam_x;
                                float pz = (float)(lr->z + portal->vertices[pv].z) - cam_z;
                                float rz_p = px * sy + pz * cy;

                                if (rz_p > -256.0f) {
                                    verts_in_front++;
                                    if (rz_p < 1.0f) rz_p = 1.0f;
                                    float rx_p = px * cy - pz * sy;
                                    float sx_p = 160.0f + rx_p * 256.0f / rz_p;
                                    if (sx_p < pmin_sx) pmin_sx = sx_p;
                                    if (sx_p > pmax_sx) pmax_sx = sx_p;
                                }
                            }

                            if (verts_in_front > 0) {
                                if (verts_in_front < 4) {
                                    /* Crosses near plane — very close */
                                    priority = STREAM_PRI_HIGH;
                                } else if (pmax_sx > -32 && pmin_sx < 352) {
                                    /* On screen */
                                    priority = STREAM_PRI_HIGH;
                                } else if (pmax_sx > -128 && pmin_sx < 448) {
                                    /* Near screen edge */
                                    priority = STREAM_PRI_MED;
                                }
                            }

                            tr_chunk_t *c = &g_chunks[dest];
                            float ddx = c->cx - cam_x;
                            float ddz = c->cz - cam_z;
                            c->dist_sq = ddx * ddx + ddz * ddz;
                            c->visible = true;
                            vis_rooms[vis_count] = dest;
                            vis_stream_pri[vis_count] = priority;
                            vis_count++;
                        }
                    }

                    /* Extended ring: recurse through portals up to N hops deep.
                     * Each hop adds rooms connected to the previous ring.
                     * Only adds rooms whose portal has at least 1 vertex
                     * in front of the camera. Fills the void between near
                     * and the farthest visible rooms. */
                    int ring1_end = vis_count;
                    #define MAX_RING_DEPTH 5
                    int ring_start = 0;
                    perf_ring_counts[0] = vis_count; /* ring 0 = current + ring1 */
                    for (int ring = 0; ring < MAX_RING_DEPTH && vis_count < MAX_DRAW_ROOMS; ring++) {
                        int ring_end = vis_count;
                        for (int vi = ring_start; vi < ring_end && vis_count < MAX_DRAW_ROOMS; vi++) {
                            int r1 = vis_rooms[vi];
                            if (r1 < 0 || r1 >= g_level.room_count) continue;
                            tr_room_t *r1r = &g_level.rooms[r1];
                            for (int p = 0; p < r1r->portal_count && vis_count < MAX_DRAW_ROOMS; p++) {
                                tr_portal_t *portal = &r1r->portals[p];
                                int dest = portal->room;
                                if (dest < 0 || dest >= g_chunk_count) continue;
                                bool dup = false;
                                for (int vi2 = 0; vi2 < vis_count; vi2++) {
                                    if (vis_rooms[vi2] == dest) { dup = true; break; }
                                }
                                if (dup) continue;

                                /* Portal frustum check: project portal vertices to screen.
                                 * Only add room if portal overlaps the screen area.
                                 * This prevents adding rooms behind/beside the camera. */
                                int front = 0;
                                float pmin_sx = 320, pmax_sx = 0;
                                float pmin_sy = 240, pmax_sy = 0;
                                for (int pv = 0; pv < 4; pv++) {
                                    float px = (float)(r1r->x + portal->vertices[pv].x) - cam_x;
                                    float py = (float)(portal->vertices[pv].y) - cam_y;
                                    float pz = (float)(r1r->z + portal->vertices[pv].z) - cam_z;
                                    float rx_p = px * cy - pz * sy;
                                    float rz_yaw = px * sy + pz * cy;
                                    float ry_p = py * cp - rz_yaw * sp;
                                    float rz_p = py * sp + rz_yaw * cp;
                                    if (rz_p > 1.0f) {
                                        front++;
                                        float inv = 256.0f / rz_p;
                                        float sx = 160.0f + rx_p * inv;
                                        float sy_p = 120.0f + ry_p * inv;
                                        if (sx < pmin_sx) pmin_sx = sx;
                                        if (sx > pmax_sx) pmax_sx = sx;
                                        if (sy_p < pmin_sy) pmin_sy = sy_p;
                                        if (sy_p > pmax_sy) pmax_sy = sy_p;
                                    }
                                }
                                /* Skip if entirely behind camera */
                                if (front == 0) continue;
                                /* Skip if portal projects entirely off-screen
                                 * (with generous margin for partially visible portals) */
                                if (front == 4 && (pmax_sx < -100 || pmin_sx > 420 ||
                                    pmax_sy < -100 || pmin_sy > 340)) continue;

                                tr_chunk_t *c = &g_chunks[dest];
                                float ddx = c->cx - cam_x;
                                float ddz = c->cz - cam_z;
                                c->dist_sq = ddx * ddx + ddz * ddz;
                                c->visible = true;
                                vis_rooms[vis_count] = dest;
                                vis_stream_pri[vis_count] = STREAM_PRI_LOW;
                                vis_count++;
                            }
                        }
                        ring_start = ring_end;
                        perf_ring_counts[ring+1] = vis_count - ring_end; /* new rooms in this ring */
                    }

                    /* Track visibility for diagnostics */
                    perf_vis_ring1 = ring1_end;
                    perf_vis_ring2 = vis_count - ring1_end;
                    /* ring2 now includes rings 2-5 */
                    perf_vis_total = vis_count;

                    /* LOD classification with HYSTERESIS.
                     * Each room tracks current/target tier. Tier only switches
                     * after target is stable for TIER_HOLD_FRAMES. Hysteresis
                     * band prevents flapping at boundaries. */
                    int tex_rooms[MAX_DRAW_ROOMS], tex_count = 0;
                    int bkd_rooms [MAX_DRAW_ROOMS], bkd_count  = 0;

                    /* Threshold array for tier classification */
                    float tier_thresholds_sq[4] = {
                        g_dist_mid_sq, g_dist_vfar_sq,
                        g_dist_xfar_sq, g_dist_xxfar_sq
                    };

                    for (int vi = 0; vi < vis_count; vi++) {
                        int ri = vis_rooms[vi];
                        tr_chunk_t *c = &g_chunks[ri];

                        /* Force tier override */
                        if (g_debug_force_tier == 1) {
                            c->lod = CHUNK_LOD_NEAR;
                            if (tex_count < MAX_DRAW_ROOMS)
                                tex_rooms[tex_count++] = ri;
                            continue;
                        } else if (g_debug_force_tier >= 2) {
                            c->lod = CHUNK_LOD_FAR;
                            if (bkd_count < MAX_DRAW_ROOMS)
                                bkd_rooms[bkd_count++] = ri;
                            continue;
                        }

                        /* Current room always Near */
                        if (ri == lara_room) {
                            room_tiers[ri].current_tier = TIER_NEAR;
                            room_tiers[ri].target_tier = TIER_NEAR;
                            room_tiers[ri].hold_count = TIER_HOLD_FRAMES;
                            c->lod = CHUNK_LOD_NEAR;
                            if (tex_count < MAX_DRAW_ROOMS)
                                tex_rooms[tex_count++] = ri;
                            continue;
                        }

                        /* Determine raw target tier from distance */
                        float dist = sqrtf(c->dist_sq);
                        int raw_tier = TIER_NEAR;
                        float thresholds[4] = {
                            g_dist_mid, g_dist_vfar, g_dist_xfar, g_dist_xxfar
                        };
                        for (int t = 0; t < 4; t++) {
                            /* Hysteresis: need to cross threshold + band to promote,
                             * threshold - band to demote */
                            float band = thresholds[t] * TIER_HYSTERESIS;
                            float promote_at = thresholds[t] + band;
                            float demote_at  = thresholds[t] - band;
                            if (room_tiers[ri].current_tier <= t && dist > promote_at)
                                raw_tier = t + 1;
                            else if (room_tiers[ri].current_tier > t && dist > demote_at)
                                raw_tier = t + 1;
                        }
                        /* Max draw distance cull */
                        if (dist > g_max_draw_dist) raw_tier = TIER_CULLED;

                        /* Update target + hold counter */
                        if (raw_tier != room_tiers[ri].target_tier) {
                            room_tiers[ri].target_tier = raw_tier;
                            room_tiers[ri].hold_count = 0;
                        } else if (room_tiers[ri].hold_count < TIER_HOLD_FRAMES) {
                            room_tiers[ri].hold_count++;
                        }

                        /* Switch current tier only when target is stable */
                        if (room_tiers[ri].hold_count >= TIER_HOLD_FRAMES) {
                            room_tiers[ri].current_tier = room_tiers[ri].target_tier;
                        }

                        int tier = room_tiers[ri].current_tier;

                        /* Culled rooms don't render */
                        if (tier >= TIER_CULLED) continue;

                        /* Near (tier 0) goes to textured pass, all others to baked */
                        if (tier == TIER_NEAR) {
                            c->lod = CHUNK_LOD_NEAR;
                            if (tex_count < MAX_DRAW_ROOMS)
                                tex_rooms[tex_count++] = ri;
                        } else {
                            c->lod = CHUNK_LOD_FAR;
                            if (bkd_count < MAX_DRAW_ROOMS)
                                bkd_rooms[bkd_count++] = ri;
                        }
                    }

                    /* Update cluster face_skip based on cluster-center distance.
                     * All children in a cluster share the same face_skip. */
                    for (int ci = 0; ci < g_cluster_count; ci++) {
                        tr_cluster_t *cl = &g_clusters[ci];
                        float cdx = cl->cx - cam_x;
                        float cdz = cl->cz - cam_z;
                        cl->dist_sq = cdx*cdx + cdz*cdz;
                        if (cl->dist_sq < CHUNK_FAR_DIST2)
                            cl->face_skip = 1;       /* near: all faces */
                        else if (cl->dist_sq < g_dist_vfar_sq)
                            cl->face_skip = 2;       /* far: every 2nd face */
                        else
                            cl->face_skip = 4;       /* very far: every 4th */
                    }

                    /* Sort baked rooms front-to-back (closest first).
                     * Ensures close rooms render before budget fires.
                     * Z-fighting possible without zbuf but acceptable. */
                    for (int i = 1; i < bkd_count; i++) {
                        int key = bkd_rooms[i];
                        float key_d = g_chunks[key].dist_sq;
                        int j = i - 1;
                        while (j >= 0 && g_chunks[bkd_rooms[j]].dist_sq > key_d) {
                            bkd_rooms[j + 1] = bkd_rooms[j];
                            j--;
                        }
                        bkd_rooms[j + 1] = key;
                    }

                    /* Merge: near rooms first, then far rooms */
                    int rooms_to_draw[MAX_DRAW_ROOMS * 2];
                    int num_draw = 0;
                    for (int i = 0; i < tex_count; i++)
                        rooms_to_draw[num_draw++] = tex_rooms[i];
                    for (int i = 0; i < bkd_count; i++)
                        rooms_to_draw[num_draw++] = bkd_rooms[i];

                    /* Shared vertex cache — used by both far and textured passes.
                     * Transform all room vertices once per room, reuse for faces. */
                    #define VTX_CACHE_MAX 600
                    static struct { float rx, ry, rz, r, g, b; } vtx_cache[VTX_CACHE_MAX];

                    /* Guard band for screen-space rejection.
                     * Must be tight enough for RDP fixed-point precision
                     * but wide enough that near-edge faces don't vanish.
                     * Screen is 320×240; allow ~500px margin each side. */
                    #define GUARD_LO_X (-500.0f)
                    #define GUARD_HI_X (820.0f)
                    #define GUARD_LO_Y (-380.0f)
                    #define GUARD_HI_Y (620.0f)

                    uint32_t perf_bkd_t0 = TICKS_READ(); /* start baked pass timer */

                    /* ===== RDPQ BAKED PASS (performance-optimized) =====
                     * CPU-projected, textured + shaded triangles via rdpq.
                     * Key perf choices:
                     *   - rspq_flush (non-blocking) not rspq_wait (blocking)
                     *   - Flush every 128 tris to reduce sync overhead
                     *   - Per-face centroid back-cull before vertex projection
                     *   - Baked tile is default; no per-face RGBA16 fallback
                     *   - Room origin pre-computed as float outside face loop */
                    {
                        rdpq_set_mode_standard();
                        rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
                        rdpq_mode_blender(0);
                        rdpq_mode_filter(FILTER_BILINEAR);

                        #define NEAR_Z 350.0f  /* near clip plane */
                        int bkd_last_tex = -1;
                        int bkd_tris = 0;
                        int bkd_rooms_max = bkd_count; /* no room cap — tri budget limits work */
                        /* BKD_TRI_BUDGET / BKD_UPLOAD_BUDGET defined at function scope */

                        for (int fi = 0; fi < bkd_rooms_max; fi++) {
                            int room_idx = bkd_rooms[fi];
                            if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                            tr_room_t *room = &g_level.rooms[room_idx];

                            /* ---- Per-room constants (hoisted out of face loop) ---- */
                            float room_d = (room_idx < g_chunk_count)
                                ? g_chunks[room_idx].dist_sq : 0.0f;

                            /* Distance-based fog — smooth ramp, no forced minimum.
                             * Close baked rooms get light fog, distant get heavy. */
                            float room_dist = sqrtf(room_d);
                            float fog = (room_dist - g_dist_mid) / (g_dist_xxfar - g_dist_mid);
                            if (fog < 0.0f) fog = 0.0f;
                            if (fog > 0.85f) fog = 0.85f;
                            float fog_scale  = 1.0f - fog;
                            float fog_add_r  = active_fog_r * fog;
                            float fog_add_g  = active_fog_g * fog;
                            float fog_add_b  = active_fog_b * fog;

                            /* (room_uv_scale removed — UV now computed from room footprint) */

                            /* Baked room tile — DEFAULT for all far rooms.
                             * Tier chosen by distance.  Ring 2 always gets IA4.
                             * No per-face uploads happen when room_baked=true. */
                            bool room_baked = false;
                            /* Use actual room distance for tier selection.
                             * (is_ring2 override removed — distance-based tiers now) */
                            float rd = room_d;

                            /* Use the room's assigned tier from hysteresis system */
                            int bkd_tier;
                            if (g_debug_force_tier >= 2)
                                bkd_tier = g_debug_force_tier - 1;
                            else if (room_idx < 256)
                                bkd_tier = room_tiers[room_idx].current_tier;
                            else
                                bkd_tier = TIER_MIDFAR;

                            /* Debug colors: apply to ALL far rooms */
                            if (g_debug_dist_colors && dbg_tier_px) {
                                rspq_flush();
                                rdpq_mode_tlut(TLUT_NONE);
                                rdpq_tex_upload(TILE0, &dbg_tier_surf[bkd_tier], NULL);
                                room_baked = true;
                                bkd_last_tex = -99;
                            } else if (bkd_tier == TIER_MIDFAR) {
                                /* MidFar: CI8 per-face textures (same as textured pass) */
                                rdpq_mode_tlut(TLUT_RGBA16);
                                rdpq_tex_upload_tlut((uint16_t *)pal16, 0, 256);
                                /* room_baked stays false → face loop uses per-face CI8 uploads */
                                bkd_last_tex = -99;
                            } else if (baked_room_count > 0 && room_idx < baked_room_count) {
                                /* Far/XFar/XXFar: use baked tiles */
                                rspq_flush();
                                if (bkd_tier >= TIER_XXFAR && bkd_m4_surf) {
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &bkd_m4_surf[room_idx], NULL);
                                } else if (bkd_tier >= TIER_XFAR && bkd_m3_surf) {
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &bkd_m3_surf[room_idx], NULL);
                                } else if (bkd_tier >= TIER_FAR && bkd_m2_surf) {
                                    rdpq_mode_tlut(TLUT_RGBA16);
                                    rdpq_tex_upload_tlut(bkd_m2_pal[room_idx], 0, 16);
                                    rdpq_tex_upload(TILE0, &bkd_m2_surf[room_idx], NULL);
                                } else if (bkd_m1_surf) {
                                    rdpq_mode_tlut(TLUT_RGBA16);
                                    rdpq_tex_upload_tlut(bkd_m1_pal[room_idx], 0, 256);
                                    rdpq_tex_upload(TILE0, &bkd_m1_surf[room_idx], NULL);
                                }
                                room_baked = true;
                                bkd_last_tex = -99;
                                perf_bkd_uploads++;
                            }
                            /* Non-baked fallback: flat faces use white_surf (uploaded
                             * on demand below); textured faces use last-bound tile. */

                            /* (sphere-in-front test removed — always clip when any_behind) */

                            bool is_water = (room->flags & 1) != 0;
                            perf_bkd_rooms++;
                            if (room_baked) perf_bkd_baked++;

                            /* ---- Baked pass vertex cache ---- */
                            float room_ox = (float)room->x - cam_x;
                            float room_oz = (float)room->z - cam_z;

                            int bkd_nv = room->mesh.vertex_count;
                            if (bkd_nv > VTX_CACHE_MAX) bkd_nv = VTX_CACHE_MAX;
                            for (int vi = 0; vi < bkd_nv; vi++) {
                                tr_room_vertex_t *rv = &room->mesh.vertices[vi];
                                float cx_v = room_ox + rv->x;
                                float cy_v = (float)rv->y - cam_y;
                                float cz_v = room_oz + rv->z;
                                float rx = cx_v * cy - cz_v * sy;
                                float rz_yaw = cx_v * sy + cz_v * cy;
                                vtx_cache[vi].rx = rx;
                                vtx_cache[vi].ry = cy_v * cp - rz_yaw * sp;
                                vtx_cache[vi].rz = cy_v * sp + rz_yaw * cp;

                                float bright = 1.0f - ((float)rv->light / 8191.0f);
                                if (bright < 0.0f) bright = 0.0f;
                                if (bright > 1.0f) bright = 1.0f;
                                bright = 0.4f + bright * 0.6f;
                                vtx_cache[vi].r = (is_water ? bright * 0.4f : bright) * fog_scale + fog_add_r;
                                vtx_cache[vi].g = (is_water ? bright * 0.6f : bright) * fog_scale + fog_add_g;
                                vtx_cache[vi].b = (is_water ? bright * 1.0f : bright) * fog_scale + fog_add_b;
                            }

                            for (int f = 0; f < room->mesh.face_count; f++) {
                                if (!g_debug_force_tier && bkd_tris >= BKD_TRI_BUDGET) goto bkd_pass_done;
                                /* (face skip removed — vertex cache makes per-face cost cheap enough) */
                                tr_face_t *face = &room->mesh.faces[f];
                                int nv = face->vertex_count;
                                if (nv < 3) continue;

                                /* Back-cull + distance cull from cache */
                                {
                                    uint16_t v0i = face->vertices[0];
                                    if (v0i < bkd_nv) {
                                        if (vtx_cache[v0i].rz < -512.0f) continue;
                                        float dx = vtx_cache[v0i].rx;
                                        float dz = vtx_cache[v0i].rz;
                                        if (dx*dx + dz*dz > g_max_draw_dist_sq) continue;
                                    }
                                }

                                uint16_t tex_idx = face->texture & 0x7FFF;
                                bool is_flat = (face->texture & 0x8000) != 0;
                                if (!room_baked) {
                                    /* MidFar / non-baked: CI8 per-face textures with fog */
                                    if (!is_flat && tex_idx < num_otex && ci8_surfs) {
                                        if ((int)tex_idx != bkd_last_tex) {
                                            if (bkd_last_tex == -2) rdpq_mode_tlut(TLUT_RGBA16);
                                            rdpq_tex_upload(TILE0, &ci8_surfs[tex_idx], NULL);
                                            bkd_last_tex = (int)tex_idx;
                                            perf_bkd_uploads++;
                                        }
                                    } else if (is_flat) {
                                        if (white_px && bkd_last_tex != -2) {
                                            rdpq_mode_tlut(TLUT_NONE);
                                            rdpq_tex_upload(TILE0, &white_surf, NULL);
                                            bkd_last_tex = -2;
                                            perf_bkd_uploads++;
                                        }
                                    } else continue;
                                }

                                tr_object_texture_t *otex = (!is_flat && !room_baked &&
                                    tex_idx < g_level.object_texture_count)
                                    ? &g_level.object_textures[tex_idx] : NULL;

                                /* Look up cached vertices */
                                float cv[4][8];
                                bool visible = true;
                                bool any_behind = false, all_behind = true;

                                for (int v = 0; v < nv; v++) {
                                    uint16_t vi = face->vertices[v];
                                    if (vi >= bkd_nv) { visible = false; break; }

                                    cv[v][0] = vtx_cache[vi].rx;
                                    cv[v][1] = vtx_cache[vi].ry;
                                    cv[v][2] = vtx_cache[vi].rz;

                                    if (cv[v][2] < NEAR_Z) any_behind = true;
                                    else all_behind = false;

                                    cv[v][3] = vtx_cache[vi].r;
                                    cv[v][4] = vtx_cache[vi].g;
                                    cv[v][5] = vtx_cache[vi].b;

                                    float ts = 0, tt = 0;
                                    if (room_baked && room_idx < g_chunk_count) {
                                        /* Map baked tile across room footprint.
                                         * UV from vertex world XZ position within room bounds. */
                                        tr_room_vertex_t *rv = &room->mesh.vertices[vi];
                                        float wx = (float)(room->x + rv->x);
                                        float wz = (float)(room->z + rv->z);
                                        tr_chunk_t *ck = &g_chunks[room_idx];
                                        float room_w = ck->max_x - ck->min_x;
                                        float room_d = ck->max_z - ck->min_z;
                                        if (room_w < 1.0f) room_w = 1.0f;
                                        if (room_d < 1.0f) room_d = 1.0f;
                                        float tile_dim = (bkd_tier >= TIER_XXFAR) ? 4.0f :
                                                         (bkd_tier >= TIER_XFAR) ? 8.0f :
                                                         (bkd_tier >= TIER_FAR) ? 8.0f : 16.0f;
                                        ts = ((wx - ck->min_x) / room_w) * tile_dim;
                                        tt = ((wz - ck->min_z) / room_d) * tile_dim;
                                        /* Clamp to valid tile range */
                                        if (ts < 0) ts = 0;
                                        if (ts > tile_dim) ts = tile_dim;
                                        if (tt < 0) tt = 0;
                                        if (tt > tile_dim) tt = tile_dim;
                                    } else if (otex && tex_uv_map) {
                                        float tu = (float)(otex->uv[v].u >> 8);
                                        float tv = (float)(otex->uv[v].v >> 8);
                                        ts = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                        tt = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                    }
                                    cv[v][6] = ts; cv[v][7] = tt;
                                }
                                if (!visible || all_behind) continue;

                                /* Near-plane clip (Sutherland-Hodgman, z=NEAR_Z).
                                 * Skipped for rooms whose sphere is fully in front. */
                                float clipped[6][8];
                                int clip_nv = nv;
                                if (any_behind) {
                                    clip_nv = 0;
                                    for (int i = 0; i < nv; i++) {
                                        int j = (i + 1) % nv;
                                        float zi = cv[i][2], zj = cv[j][2];
                                        bool i_in = (zi >= NEAR_Z), j_in = (zj >= NEAR_Z);
                                        if (i_in) {
                                            for (int k = 0; k < 8; k++)
                                                clipped[clip_nv][k] = cv[i][k];
                                            clip_nv++;
                                        }
                                        if (i_in != j_in) {
                                            float t = (NEAR_Z - zi) / (zj - zi);
                                            for (int k = 0; k < 8; k++)
                                                clipped[clip_nv][k] = cv[i][k] + t * (cv[j][k] - cv[i][k]);
                                            clip_nv++;
                                        }
                                        if (clip_nv >= 6) break;
                                    }
                                    if (clip_nv < 3) continue;
                                } else {
                                    for (int v = 0; v < nv; v++)
                                        for (int k = 0; k < 8; k++)
                                            clipped[v][k] = cv[v][k];
                                }

                                /* Project to screen space (with Z-bias to prevent
                                 * Z-fighting against textured pass at same depth) */
                                float sv_raw[8][9];
                                bool any_outside = false;
                                for (int v = 0; v < clip_nv; v++) {
                                    float rz = clipped[v][2] + g_baked_z_bias;
                                    float inv_z = 256.0f / rz;
                                    float scx = 160.0f + clipped[v][0] * inv_z;
                                    float scy = 120.0f + clipped[v][1] * inv_z;
                                    if (scx < GUARD_LO_X || scx > GUARD_HI_X ||
                                        scy < GUARD_LO_Y || scy > GUARD_HI_Y)
                                        any_outside = true;
                                    sv_raw[v][0] = scx;  sv_raw[v][1] = scy;
                                    sv_raw[v][2] = clipped[v][3]; sv_raw[v][3] = clipped[v][4];
                                    sv_raw[v][4] = clipped[v][5]; sv_raw[v][5] = 1.0f;
                                    sv_raw[v][6] = clipped[v][6]; sv_raw[v][7] = clipped[v][7];
                                    sv_raw[v][8] = 1.0f; /* affine */
                                }

                                /* Screen-space S-H clip against guard band (4 edges) */
                                float (*sv_src)[9] = sv_raw;
                                int sv_nv = clip_nv;
                                float sv_t1[8][9], sv_t2[8][9];
                                if (any_outside) {
                                    /* Left */
                                    int n1 = 0;
                                    for (int i = 0; i < sv_nv; i++) {
                                        int j = (i+1) % sv_nv;
                                        bool ii = sv_src[i][0] >= GUARD_LO_X, ji = sv_src[j][0] >= GUARD_LO_X;
                                        if (ii) { for(int k=0;k<9;k++) sv_t1[n1][k]=sv_src[i][k]; n1++; }
                                        if (ii!=ji) { float t=(GUARD_LO_X-sv_src[i][0])/(sv_src[j][0]-sv_src[i][0]); for(int k=0;k<9;k++) sv_t1[n1][k]=sv_src[i][k]+t*(sv_src[j][k]-sv_src[i][k]); n1++; }
                                        if (n1>=8) break;
                                    }
                                    /* Right */
                                    int n2 = 0;
                                    for (int i = 0; i < n1; i++) {
                                        int j = (i+1) % n1;
                                        bool ii = sv_t1[i][0] <= GUARD_HI_X, ji = sv_t1[j][0] <= GUARD_HI_X;
                                        if (ii) { for(int k=0;k<9;k++) sv_t2[n2][k]=sv_t1[i][k]; n2++; }
                                        if (ii!=ji) { float t=(GUARD_HI_X-sv_t1[i][0])/(sv_t1[j][0]-sv_t1[i][0]); for(int k=0;k<9;k++) sv_t2[n2][k]=sv_t1[i][k]+t*(sv_t1[j][k]-sv_t1[i][k]); n2++; }
                                        if (n2>=8) break;
                                    }
                                    /* Top */
                                    n1 = 0;
                                    for (int i = 0; i < n2; i++) {
                                        int j = (i+1) % n2;
                                        bool ii = sv_t2[i][1] >= GUARD_LO_Y, ji = sv_t2[j][1] >= GUARD_LO_Y;
                                        if (ii) { for(int k=0;k<9;k++) sv_t1[n1][k]=sv_t2[i][k]; n1++; }
                                        if (ii!=ji) { float t=(GUARD_LO_Y-sv_t2[i][1])/(sv_t2[j][1]-sv_t2[i][1]); for(int k=0;k<9;k++) sv_t1[n1][k]=sv_t2[i][k]+t*(sv_t2[j][k]-sv_t2[i][k]); n1++; }
                                        if (n1>=8) break;
                                    }
                                    /* Bottom */
                                    n2 = 0;
                                    for (int i = 0; i < n1; i++) {
                                        int j = (i+1) % n1;
                                        bool ii = sv_t1[i][1] <= GUARD_HI_Y, ji = sv_t1[j][1] <= GUARD_HI_Y;
                                        if (ii) { for(int k=0;k<9;k++) sv_t2[n2][k]=sv_t1[i][k]; n2++; }
                                        if (ii!=ji) { float t=(GUARD_HI_Y-sv_t1[i][1])/(sv_t1[j][1]-sv_t1[i][1]); for(int k=0;k<9;k++) sv_t2[n2][k]=sv_t1[i][k]+t*(sv_t1[j][k]-sv_t1[i][k]); n2++; }
                                        if (n2>=8) break;
                                    }
                                    sv_src = sv_t2;
                                    sv_nv = n2;
                                    if (sv_nv < 3) continue;
                                }

                                int num_tris = sv_nv - 2;
                                for (int tri = 0; tri < num_tris; tri++) {
                                    rdpq_triangle(&TRIFMT_SHADE_TEX,
                                        sv_src[0], sv_src[tri+1], sv_src[tri+2]);
                                    bkd_tris++;
                                    perf_bkd_tris = bkd_tris;
                                    if ((bkd_tris & 7) == 0)
                                        rspq_flush();
                                    if (!g_debug_force_tier && bkd_tris >= BKD_TRI_BUDGET)
                                        goto bkd_pass_done;
                                }
                            }
                        }
                    }
                    bkd_pass_done:

                    /* Record baked pass CPU time */
                    {
                        uint32_t d = TICKS_DISTANCE(perf_bkd_t0, TICKS_READ());
                        perf_bkd_us = (uint32_t)((float)d / (float)TICKS_PER_SECOND * 1000000.0f);
                    }

                    /* Flush baked pass + reset TLUT before textured pass.
                     * Baked pass uses CI4/CI8 baked tiles with TLUT;
                     * textured pass re-enables it with the global pal16. */
                    rdpq_mode_tlut(TLUT_NONE);
                    rspq_flush();

                    uint32_t perf_tex_t0 = TICKS_READ(); /* start textured pass timer */

                    /* ===== RDPQ TEXTURED PASS — ring 1, CI8 textures, z-buffered =====
                     * Pure CPU transform + rdpq_triangle, same paradigm as baked pass. */

                    /* RDPQ textured pass state:
                     *  - CI8 + global pal16 TLUT (1 KB/tex vs 2 KB RGBA16)
                     *  - TLUT uploaded once per frame; only tile re-uploads per change
                     *  - Z-buffer: compare + write (depth clear already done above)
                     *  - Bilinear filter for textured faces */
                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
                    rdpq_mode_blender(0);
                    rdpq_mode_filter(FILTER_BILINEAR);
                    rdpq_mode_zbuf(true, true);

                    /* Upload the global CI8 palette once for the whole pass */
                    rdpq_mode_tlut(TLUT_RGBA16);
                    rdpq_tex_upload_tlut((uint16_t *)pal16, 0, 256);

                    int tex_last_tex = -99; /* force first upload */
                    int tex_uploads = 0;

                    /* Debug: override textured pass with solid green */
                    if (g_debug_dist_colors && dbg_tier_px) {
                        rdpq_mode_tlut(TLUT_NONE);
                        rdpq_tex_upload(TILE0, &dbg_tier_surf[0], NULL); /* green = near */
                        tex_last_tex = -99;
                    }

                    /* ===== TEXTURED PASS: ROOM FACES ===== */
                    { uint32_t _nr_t0 = TICKS_READ();
                    /* CPU transform + near-plane clip + project → rdpq_triangle.
                     * CI8 tiles (1 KB each) with shared pal16 TLUT.
                     * Z-buffer already cleared above. */
                    /* Fallback state persists across rooms — once budget fires,
                     * ALL remaining near rooms use baked tiles instead of CI8. */
                    bool tex_fallback_active = false;

                    for (int ri = 0; ri < tex_count; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];
                        bool is_water = (room->flags & 1) != 0;
                        perf_tex_rooms++;

                        bool do_decimate = false;
                        if (room_idx != lara_room && room_idx < g_chunk_count) {
                            tr_chunk_t *c = &g_chunks[room_idx];
                            if (c->dist_sq > c->near_dist2) do_decimate = true;
                        }

                        /* If already in fallback mode, upload this room's baked tile */
                        if (tex_fallback_active &&
                            baked_room_count > 0 && room_idx < baked_room_count) {
                            float rd = (room_idx < g_chunk_count) ? g_chunks[room_idx].dist_sq : 0;
                            rdpq_mode_tlut(TLUT_NONE);
                            if (rd > g_dist_xxfar_sq && bkd_m4_surf) {
                                rdpq_tex_upload(TILE0, &bkd_m4_surf[room_idx], NULL);
                            } else if (rd > g_dist_xfar_sq && bkd_m3_surf) {
                                rdpq_tex_upload(TILE0, &bkd_m3_surf[room_idx], NULL);
                            } else if (rd > g_dist_vfar_sq && bkd_m2_surf) {
                                rdpq_mode_tlut(TLUT_RGBA16);
                                rdpq_tex_upload_tlut(bkd_m2_pal[room_idx], 0, 16);
                                rdpq_tex_upload(TILE0, &bkd_m2_surf[room_idx], NULL);
                            } else if (bkd_m1_surf) {
                                rdpq_mode_tlut(TLUT_RGBA16);
                                rdpq_tex_upload_tlut(bkd_m1_pal[room_idx], 0, 256);
                                rdpq_tex_upload(TILE0, &bkd_m1_surf[room_idx], NULL);
                            }
                            tex_last_tex = -99;
                        }

                        float room_ox = (float)room->x - cam_x;
                        float room_oz = (float)room->z - cam_z;

                        /* ---- Vertex cache: transform all room vertices once ---- */
                        int nv_room = room->mesh.vertex_count;
                        if (nv_room > VTX_CACHE_MAX) nv_room = VTX_CACHE_MAX;

                        /* Per-room fog constants (hoisted from per-vertex) */
                        bool room_has_fog = (room_idx != lara_room &&
                            room_idx < g_chunk_count &&
                            g_chunks[room_idx].lod >= CHUNK_LOD_MID);
                        float fog_start = 0, fog_inv_range = 0;
                        if (room_has_fog) {
                            fog_start = g_chunks[room_idx].fog_start_dist2;
                            float fog_range = g_chunks[room_idx].max_draw_dist2 - fog_start;
                            if (fog_range < 1.0f) fog_range = 1.0f;
                            fog_inv_range = 1.0f / fog_range;
                        }

                        for (int vi = 0; vi < nv_room; vi++) {
                            tr_room_vertex_t *rv = &room->mesh.vertices[vi];
                            float cx_v = room_ox + rv->x;
                            float cy_v = (float)rv->y - cam_y;
                            float cz_v = room_oz + rv->z;
                            float rx = cx_v * cy - cz_v * sy;
                            float rz_yaw = cx_v * sy + cz_v * cy;
                            vtx_cache[vi].rx = rx;
                            vtx_cache[vi].ry = cy_v * cp - rz_yaw * sp;
                            vtx_cache[vi].rz = cy_v * sp + rz_yaw * cp;

                            float bright = 1.0f - ((float)rv->light / 8191.0f);
                            if (bright < 0.0f) bright = 0.0f;
                            if (bright > 1.0f) bright = 1.0f;
                            bright = 0.4f + bright * 0.6f;
                            float r = is_water ? bright * 0.4f : bright;
                            float g = is_water ? bright * 0.6f : bright;
                            float b = is_water ? bright * 1.0f : bright;

                            if (room_has_fog) {
                                float vx_w = room->x + rv->x;
                                float vz_w = room->z + rv->z;
                                float d2 = (vx_w-cam_x)*(vx_w-cam_x) + (vz_w-cam_z)*(vz_w-cam_z);
                                float fog = (d2 - fog_start) * fog_inv_range;
                                if (fog < 0.0f) fog = 0.0f;
                                if (fog > 0.85f) fog = 0.85f;
                                float inv_fog = 1.0f - fog;
                                r = r * inv_fog + active_fog_r * fog;
                                g = g * inv_fog + active_fog_g * fog;
                                b = b * inv_fog + active_fog_b * fog;
                            }
                            vtx_cache[vi].r = r;
                            vtx_cache[vi].g = g;
                            vtx_cache[vi].b = b;
                        }

                        /* ---- Face loop: index into cached vertices ---- */
                        for (int f = 0; f < room->mesh.face_count; f++) {
                            /* Check budget — switch to fallback instead of goto */
                            if (!tex_fallback_active &&
                                (perf_tex_tris >= TEX_TRI_BUDGET ||
                                 perf_tex_faces >= TEX_FACE_BUDGET)) {
                                tex_fallback_active = true;
                                /* Upload this room's baked tile as fallback.
                                 * Pick tier by room distance. */
                                if (baked_room_count > 0 && room_idx < baked_room_count) {
                                    float rd = g_chunks[room_idx].dist_sq;
                                    rdpq_mode_tlut(TLUT_NONE);
                                    if (rd > g_dist_xxfar_sq && bkd_m4_surf) {
                                        rdpq_tex_upload(TILE0, &bkd_m4_surf[room_idx], NULL);
                                    } else if (rd > g_dist_xfar_sq && bkd_m3_surf) {
                                        rdpq_tex_upload(TILE0, &bkd_m3_surf[room_idx], NULL);
                                    } else if (rd > g_dist_vfar_sq && bkd_m2_surf) {
                                        rdpq_mode_tlut(TLUT_RGBA16);
                                        rdpq_tex_upload_tlut(bkd_m2_pal[room_idx], 0, 16);
                                        rdpq_tex_upload(TILE0, &bkd_m2_surf[room_idx], NULL);
                                    } else if (bkd_m1_surf) {
                                        rdpq_mode_tlut(TLUT_RGBA16);
                                        rdpq_tex_upload_tlut(bkd_m1_pal[room_idx], 0, 256);
                                        rdpq_tex_upload(TILE0, &bkd_m1_surf[room_idx], NULL);
                                    } else if (white_px) {
                                        rdpq_tex_upload(TILE0, &white_surf, NULL);
                                    }
                                    tex_last_tex = -99;
                                } else if (white_px) {
                                    /* No baked data — use white as last resort */
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &white_surf, NULL);
                                    tex_last_tex = -2;
                                }
                            }

                            perf_tex_faces++;
                            tr_face_t *face = &room->mesh.faces[f];
                            int nv = face->vertex_count;
                            if (nv < 3) continue;

                            if (do_decimate && (face->flags & FACE_FLAG_DETAIL)) {
                                perf_tex_detail_skip++;
                                continue;
                            }

                            /* Back-cull + distance cull from cache */
                            {
                                uint16_t v0i = face->vertices[0];
                                if (v0i < nv_room) {
                                    if (vtx_cache[v0i].rz < -1024.0f) {
                                        perf_tex_culled++; continue;
                                    }
                                    float dx = vtx_cache[v0i].rx;
                                    float dz = vtx_cache[v0i].rz;
                                    if (dx*dx + dz*dz > g_max_draw_dist_sq) continue;
                                }
                            }

                            uint16_t tex_idx = face->texture & 0x7FFF;
                            bool is_flat = (face->texture & 0x8000) != 0;

                            /* Texture upload on change.
                             * In fallback mode: skip per-face uploads, use baked tile.
                             * In debug mode: keep solid green. */
                            if (tex_fallback_active || g_debug_dist_colors) {
                                /* fallback/debug: use whatever tile is already bound */
                            } else if (!is_flat && tex_idx < num_otex && ci8_surfs) {
                                if ((int)tex_idx != tex_last_tex &&
                                    tex_uploads < TEX_UPLOAD_BUDGET) {
                                    if (tex_last_tex == -2) rdpq_mode_tlut(TLUT_RGBA16);
                                    rdpq_tex_upload(TILE0, &ci8_surfs[tex_idx], NULL);
                                    tex_last_tex = (int)tex_idx;
                                    tex_uploads++;
                                    perf_tex_binds++;
                                }
                            } else if (is_flat) {
                                if (white_px && tex_last_tex != -2 &&
                                    tex_uploads < TEX_UPLOAD_BUDGET) {
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &white_surf, NULL);
                                    tex_last_tex = -2;
                                    tex_uploads++;
                                }
                            } else continue;

                            tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                ? &g_level.object_textures[tex_idx] : NULL;

                            /* Look up cached vertices — no per-face transform */
                            float cv[4][8];
                            bool any_behind = false, all_behind = true;
                            bool ok = true;
                            for (int v = 0; v < nv; v++) {
                                uint16_t vi = face->vertices[v];
                                if (vi >= nv_room) { ok = false; break; }

                                cv[v][0] = vtx_cache[vi].rx;
                                cv[v][1] = vtx_cache[vi].ry;
                                cv[v][2] = vtx_cache[vi].rz;

                                if (cv[v][2] < NEAR_Z) any_behind = true;
                                else all_behind = false;

                                /* Color from cache (already lit + fogged) */
                                float r = vtx_cache[vi].r;
                                float g = vtx_cache[vi].g;
                                float b = vtx_cache[vi].b;

                                /* UV computation */
                                float ts = 0, tt = 0;
                                if (tex_fallback_active && room_idx < g_chunk_count) {
                                    /* Fallback: map baked tile across room footprint */
                                    tr_room_vertex_t *rv = &room->mesh.vertices[vi];
                                    float wx = (float)(room->x + rv->x);
                                    float wz = (float)(room->z + rv->z);
                                    tr_chunk_t *ck = &g_chunks[room_idx];
                                    float rw = ck->max_x - ck->min_x;
                                    float rd = ck->max_z - ck->min_z;
                                    if (rw < 1.0f) rw = 1.0f;
                                    if (rd < 1.0f) rd = 1.0f;
                                    ts = ((wx - ck->min_x) / rw) * 16.0f;
                                    tt = ((wz - ck->min_z) / rd) * 16.0f;
                                    if (ts < 0) ts = 0;
                                    if (ts > 16.0f) ts = 16.0f;
                                    if (tt < 0) tt = 0;
                                    if (tt > 16.0f) tt = 16.0f;
                                } else if (otex && tex_uv_map) {
                                    float tu = (float)(otex->uv[v].u >> 8);
                                    float tv = (float)(otex->uv[v].v >> 8);
                                    ts = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                    tt = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                } else if (is_flat) {
                                    uint8_t ci = tex_idx & 0xFF;
                                    r = (float)g_level.palette[ci*3+0] / 63.0f;
                                    g = (float)g_level.palette[ci*3+1] / 63.0f;
                                    b = (float)g_level.palette[ci*3+2] / 63.0f;
                                }

                                cv[v][0] = vtx_cache[vi].rx;
                                cv[v][1] = vtx_cache[vi].ry;
                                cv[v][2] = vtx_cache[vi].rz;
                                cv[v][3] = r; cv[v][4] = g; cv[v][5] = b;
                                cv[v][6] = ts; cv[v][7] = tt;
                            }
                            if (!ok || all_behind) continue;

                            /* Near-plane clip — ALWAYS clip if any vertex is behind,
                             * even if the sphere test said the room is in front.
                             * The sphere is approximate; individual vertices can
                             * still be behind NEAR_Z, causing wild perspective
                             * distortion (texture swimming) if not clipped. */
                            float clipped[6][8];
                            int clip_nv = nv;
                            if (any_behind) {
                                clip_nv = 0;
                                for (int i = 0; i < nv; i++) {
                                    int j = (i + 1) % nv;
                                    float zi = cv[i][2], zj = cv[j][2];
                                    bool i_in = (zi >= NEAR_Z), j_in = (zj >= NEAR_Z);
                                    if (i_in) {
                                        for (int k=0;k<8;k++) clipped[clip_nv][k]=cv[i][k];
                                        clip_nv++;
                                    }
                                    if (i_in != j_in) {
                                        float t = (NEAR_Z - zi) / (zj - zi);
                                        for (int k=0;k<8;k++)
                                            clipped[clip_nv][k]=cv[i][k]+t*(cv[j][k]-cv[i][k]);
                                        clip_nv++;
                                    }
                                }
                                if (clip_nv < 3) continue;
                            } else {
                                for (int i=0;i<nv;i++)
                                    for (int k=0;k<8;k++) clipped[i][k]=cv[i][k];
                            }

                            /* Project to screen space */
                            float sv_raw[8][10];
                            bool any_outside = false;
                            for (int v = 0; v < clip_nv; v++) {
                                float rz = clipped[v][2];
                                float inv_z = 256.0f / rz;
                                float scx = 160.0f + clipped[v][0] * inv_z;
                                float scy = 120.0f + clipped[v][1] * inv_z;
                                if (scx < GUARD_LO_X || scx > GUARD_HI_X ||
                                    scy < GUARD_LO_Y || scy > GUARD_HI_Y)
                                    any_outside = true;
                                sv_raw[v][0]=scx;   sv_raw[v][1]=scy;
                                sv_raw[v][2]=rz/32768.0f; if(sv_raw[v][2]>1.0f) sv_raw[v][2]=1.0f;
                                sv_raw[v][3]=clipped[v][3]; sv_raw[v][4]=clipped[v][4];
                                sv_raw[v][5]=clipped[v][5]; sv_raw[v][6]=1.0f;
                                sv_raw[v][7]=clipped[v][6]; sv_raw[v][8]=clipped[v][7];
                                sv_raw[v][9]=1.0f; /* affine texturing (PS1 style) — stable, no swimming */
                            }

                            /* Screen-space S-H clip against guard band (4 edges).
                             * Only runs for faces with vertices outside the band. */
                            float (*sv_src)[10] = sv_raw;
                            int sv_nv = clip_nv;
                            float sv_tmp1[8][10], sv_tmp2[8][10];
                            if (any_outside) {
                                /* Clip left (x >= GUARD_LO_X) */
                                int out_nv = 0;
                                for (int i = 0; i < sv_nv; i++) {
                                    int j = (i+1) % sv_nv;
                                    bool ii = sv_src[i][0] >= GUARD_LO_X;
                                    bool ji = sv_src[j][0] >= GUARD_LO_X;
                                    if (ii) { for(int k=0;k<10;k++) sv_tmp1[out_nv][k]=sv_src[i][k]; out_nv++; }
                                    if (ii!=ji) {
                                        float t=(GUARD_LO_X-sv_src[i][0])/(sv_src[j][0]-sv_src[i][0]);
                                        for(int k=0;k<10;k++) sv_tmp1[out_nv][k]=sv_src[i][k]+t*(sv_src[j][k]-sv_src[i][k]);
                                        out_nv++;
                                    }
                                    if (out_nv >= 8) break;
                                }
                                /* Clip right (x <= GUARD_HI_X) */
                                int out2 = 0;
                                for (int i = 0; i < out_nv; i++) {
                                    int j = (i+1) % out_nv;
                                    bool ii = sv_tmp1[i][0] <= GUARD_HI_X;
                                    bool ji = sv_tmp1[j][0] <= GUARD_HI_X;
                                    if (ii) { for(int k=0;k<10;k++) sv_tmp2[out2][k]=sv_tmp1[i][k]; out2++; }
                                    if (ii!=ji) {
                                        float t=(GUARD_HI_X-sv_tmp1[i][0])/(sv_tmp1[j][0]-sv_tmp1[i][0]);
                                        for(int k=0;k<10;k++) sv_tmp2[out2][k]=sv_tmp1[i][k]+t*(sv_tmp1[j][k]-sv_tmp1[i][k]);
                                        out2++;
                                    }
                                    if (out2 >= 8) break;
                                }
                                /* Clip top (y >= GUARD_LO_Y) */
                                out_nv = 0;
                                for (int i = 0; i < out2; i++) {
                                    int j = (i+1) % out2;
                                    bool ii = sv_tmp2[i][1] >= GUARD_LO_Y;
                                    bool ji = sv_tmp2[j][1] >= GUARD_LO_Y;
                                    if (ii) { for(int k=0;k<10;k++) sv_tmp1[out_nv][k]=sv_tmp2[i][k]; out_nv++; }
                                    if (ii!=ji) {
                                        float t=(GUARD_LO_Y-sv_tmp2[i][1])/(sv_tmp2[j][1]-sv_tmp2[i][1]);
                                        for(int k=0;k<10;k++) sv_tmp1[out_nv][k]=sv_tmp2[i][k]+t*(sv_tmp2[j][k]-sv_tmp2[i][k]);
                                        out_nv++;
                                    }
                                    if (out_nv >= 8) break;
                                }
                                /* Clip bottom (y <= GUARD_HI_Y) */
                                out2 = 0;
                                for (int i = 0; i < out_nv; i++) {
                                    int j = (i+1) % out_nv;
                                    bool ii = sv_tmp1[i][1] <= GUARD_HI_Y;
                                    bool ji = sv_tmp1[j][1] <= GUARD_HI_Y;
                                    if (ii) { for(int k=0;k<10;k++) sv_tmp2[out2][k]=sv_tmp1[i][k]; out2++; }
                                    if (ii!=ji) {
                                        float t=(GUARD_HI_Y-sv_tmp1[i][1])/(sv_tmp1[j][1]-sv_tmp1[i][1]);
                                        for(int k=0;k<10;k++) sv_tmp2[out2][k]=sv_tmp1[i][k]+t*(sv_tmp1[j][k]-sv_tmp1[i][k]);
                                        out2++;
                                    }
                                    if (out2 >= 8) break;
                                }
                                sv_src = sv_tmp2;
                                sv_nv = out2;
                                if (sv_nv < 3) continue;
                            }

                            int num_tris = sv_nv - 2;
                            for (int tri = 0; tri < num_tris; tri++) {
                                rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, sv_src[0], sv_src[tri+1], sv_src[tri+2]);
                                perf_tex_tris++;
                                if ((perf_tex_tris & 7) == 0) rspq_flush();
                            }
                        }
                    }

                    perf_tex_room_us = (uint32_t)((float)TICKS_DISTANCE(_nr_t0, TICKS_READ()) / (float)TICKS_PER_SECOND * 1000000.0f);
                    } /* end room faces timing */

                    /* ===== TEXTURED PASS: STATIC MESHES ===== */
                    { uint32_t _sm_t0 = TICKS_READ();
                    /* Z-bias: pull mesh slightly toward camera to win Z-test
                     * against coplanar room walls (no glPolygonOffset in RDPQ). */
                    #define SM_Z_BIAS 0.997f
                    for (int ri = 0; ri < tex_count; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];

                        for (int si = 0; si < room->static_mesh_count; si++) {
                            tr_room_static_mesh_t *sm = &room->static_meshes[si];

                            float sdx = (float)sm->pos.x - cam_x;
                            float sdz = (float)sm->pos.z - cam_z;
                            float sm_rz = sdx * sy + sdz * cy;
                            if (sm_rz < -1024) continue;
                            float sm_rx = sdx * cy - sdz * sy;
                            if (sm_rz > 1024) { float l=sm_rz*3.0f; if(sm_rx>l||sm_rx<-l) continue; }

                            int mesh_idx = -1;
                            for (int oi = 0; oi < g_level.static_object_count; oi++) {
                                if (g_level.static_objects[oi].object_id == sm->static_mesh_id) {
                                    if (!(g_level.static_objects[oi].flags & 2)) break;
                                    mesh_idx = g_level.static_objects[oi].mesh_idx;
                                    break;
                                }
                            }
                            if (mesh_idx < 0 || mesh_idx >= g_level.mesh_count) continue;
                            tr_object_mesh_t *mesh = &g_level.meshes[mesh_idx];

                            float sm_bright = 1.0f - (float)sm->shade / 8191.0f;
                            if (sm_bright < 0.1f) sm_bright = 0.1f;
                            if (sm_bright > 1.0f) sm_bright = 1.0f;
                            float sm_yaw = (float)sm->rotation * 3.14159f / 32768.0f;
                            float sm_sy_r = sinf(sm_yaw), sm_cy_r = cosf(sm_yaw);

                            for (int f = 0; f < mesh->face_count && f < 100; f++) {
                                if (perf_tex_tris >= TEX_TRI_BUDGET ||
                                    perf_tex_faces >= TEX_FACE_BUDGET) goto near_pass_done;
                                perf_tex_faces++;
                                tr_face_t *face = &mesh->faces[f];
                                int nv = face->vertex_count;
                                if (nv < 3) continue;

                                uint16_t tex_idx = face->texture & 0x7FFF;
                                bool is_flat = (face->texture & 0x8000) != 0;
                                float sm_fr, sm_fg, sm_fb;

                                if (is_flat) {
                                    uint8_t ci = tex_idx & 0xFF;
                                    sm_fr = (float)g_level.palette[ci*3+0] / 63.0f * sm_bright;
                                    sm_fg = (float)g_level.palette[ci*3+1] / 63.0f * sm_bright;
                                    sm_fb = (float)g_level.palette[ci*3+2] / 63.0f * sm_bright;
                                    if (white_px && tex_last_tex != -2) {
                                        rdpq_mode_tlut(TLUT_NONE);
                                        rdpq_tex_upload(TILE0, &white_surf, NULL);
                                        tex_last_tex = -2;
                                    }
                                } else if (tex_idx < num_otex && ci8_surfs) {
                                    sm_fr = sm_fg = sm_fb = sm_bright;
                                    if ((int)tex_idx != tex_last_tex) {
                                        if (tex_last_tex == -2) rdpq_mode_tlut(TLUT_RGBA16);
                                        rdpq_tex_upload(TILE0, &ci8_surfs[tex_idx], NULL);
                                        tex_last_tex = (int)tex_idx;
                                    }
                                } else continue;

                                tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                    ? &g_level.object_textures[tex_idx] : NULL;

                                float sv[4][10];
                                bool ok = true;
                                float sm_cv[4][8];
                                bool sm_any_behind = false, sm_all_behind = true;
                                for (int v = 0; v < nv; v++) {
                                    uint16_t vi = face->vertices[v];
                                    if (vi >= mesh->vertex_count) { ok = false; break; }
                                    float lx = (float)mesh->vertices[vi].x;
                                    float lz = (float)mesh->vertices[vi].z;
                                    float wx = (float)sm->pos.x + lx*sm_cy_r - lz*sm_sy_r;
                                    float wy = (float)sm->pos.y + (float)mesh->vertices[vi].y;
                                    float wz = (float)sm->pos.z + lx*sm_sy_r + lz*sm_cy_r;
                                    /* Z-bias: shrink toward camera */
                                    wx = cam_x + (wx - cam_x) * SM_Z_BIAS;
                                    wy = cam_y + (wy - cam_y) * SM_Z_BIAS;
                                    wz = cam_z + (wz - cam_z) * SM_Z_BIAS;
                                    float cx_v=wx-cam_x, cy_v=wy-cam_y, cz_v=wz-cam_z;
                                    float rx=cx_v*cy-cz_v*sy;
                                    float rz_yaw=cx_v*sy+cz_v*cy;
                                    float ry=cy_v*cp-rz_yaw*sp;
                                    float rz=cy_v*sp+rz_yaw*cp;
                                    if (rz < NEAR_Z) sm_any_behind=true; else sm_all_behind=false;
                                    float ts=0, tt=0;
                                    if (otex && tex_uv_map) {
                                        float tu=(float)(otex->uv[v].u>>8);
                                        float tv=(float)(otex->uv[v].v>>8);
                                        ts=(tu-tex_uv_map[tex_idx*4+0])*tex_uv_map[tex_idx*4+2];
                                        tt=(tv-tex_uv_map[tex_idx*4+1])*tex_uv_map[tex_idx*4+3];
                                    }
                                    sm_cv[v][0]=rx; sm_cv[v][1]=ry; sm_cv[v][2]=rz;
                                    sm_cv[v][3]=sm_fr; sm_cv[v][4]=sm_fg; sm_cv[v][5]=sm_fb;
                                    sm_cv[v][6]=ts; sm_cv[v][7]=tt;
                                }
                                if (!ok || sm_all_behind) continue;

                                float sm_cl[6][8]; int sm_cnv = nv;
                                if (sm_any_behind) {
                                    sm_cnv = 0;
                                    for (int i=0;i<nv;i++) {
                                        int j=(i+1)%nv;
                                        float zi=sm_cv[i][2],zj=sm_cv[j][2];
                                        bool ii=(zi>=NEAR_Z),ji=(zj>=NEAR_Z);
                                        if (ii){for(int k=0;k<8;k++)sm_cl[sm_cnv][k]=sm_cv[i][k];sm_cnv++;}
                                        if(ii!=ji){float t=(NEAR_Z-zi)/(zj-zi);for(int k=0;k<8;k++)sm_cl[sm_cnv][k]=sm_cv[i][k]+t*(sm_cv[j][k]-sm_cv[i][k]);sm_cnv++;}
                                    }
                                    if(sm_cnv<3) continue;
                                } else { for(int i=0;i<nv;i++) for(int k=0;k<8;k++) sm_cl[i][k]=sm_cv[i][k]; }

                                bool sm_guard_ok = true;
                                for (int v=0;v<sm_cnv;v++) {
                                    float rz=sm_cl[v][2];
                                    float inv_z=256.0f/rz;
                                    float scx=160.0f+sm_cl[v][0]*inv_z;
                                    float scy=120.0f+sm_cl[v][1]*inv_z;
                                    if (scx < GUARD_LO_X || scx > GUARD_HI_X ||
                                        scy < GUARD_LO_Y || scy > GUARD_HI_Y) {
                                        sm_guard_ok = false; break;
                                    }
                                    sv[v][0]=scx; sv[v][1]=scy;
                                    sv[v][2]=rz/32768.0f; if(sv[v][2]>1.0f)sv[v][2]=1.0f;
                                    sv[v][3]=sm_cl[v][3]; sv[v][4]=sm_cl[v][4]; sv[v][5]=sm_cl[v][5];
                                    sv[v][6]=1.0f;
                                    sv[v][7]=sm_cl[v][6]; sv[v][8]=sm_cl[v][7];
                                    sv[v][9]=1.0f; /* affine */
                                }
                                if (!sm_guard_ok) continue;
                                int num_tris = sm_cnv - 2;
                                for (int tri=0;tri<num_tris;tri++) {
                                    rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, sv[0], sv[tri+1], sv[tri+2]);
                                    perf_tex_tris++;
                                    if ((perf_tex_tris & 7) == 0) rspq_flush();
                                }
                            }
                        }
                    }
                    #undef SM_Z_BIAS
                    perf_tex_sm_us = (uint32_t)((float)TICKS_DISTANCE(_sm_t0, TICKS_READ()) / (float)TICKS_PER_SECOND * 1000000.0f);
                    } /* end static meshes timing */

                    /* ===== TEXTURED PASS: LARA ===== */
                    { uint32_t _la_t0 = TICKS_READ();
                    {
                        static tr_object_def_t *lara_obj = NULL;
                        static tr_item_t *lara_item = NULL;
                        static bool lara_found = false;
                        if (!lara_found) {
                            for (int i = 0; i < g_level.object_count; i++)
                                if (g_level.objects[i].object_id == 0) { lara_obj = &g_level.objects[i]; break; }
                            for (int i = 0; i < g_level.item_count; i++)
                                if (g_level.items[i].object_id == 0) { lara_item = &g_level.items[i]; break; }
                            lara_found = true;
                        }

                        if (lara_obj && lara_item && lara_obj->mesh_count > 0) {
                            int mc = lara_obj->mesh_count;
                            if (mc > 15) mc = 15;
                            int frame_size = 9 + mc * 2;
                            int base_off = lara_obj->frame_offset / 2;
                            int16_t *lfr = (g_level.anim_frames && base_off + frame_size <= g_level.anim_frame_count)
                                         ? &g_level.anim_frames[base_off] : NULL;

                            float ps[8][3]; int psp = 0;
                            float bx = (float)lara_item->pos.x;
                            float by = (float)lara_item->pos.y;
                            float bz = (float)lara_item->pos.z;
                            float lyaw = (float)lara_item->rotation * 3.14159f / 32768.0f;
                            float lsy_r = sinf(lyaw), lcy_r = cosf(lyaw);

                            if (lfr) {
                                float ox = (float)lfr[6], oz = (float)lfr[8];
                                bx += ox * lcy_r - oz * lsy_r;
                                by += (float)lfr[7];
                                bz += ox * lsy_r + oz * lcy_r;
                            }

                            /* Dynamic lighting from room */
                            float lara_shade = 1.0f;
                            if (lara_room >= 0 && lara_room < g_level.room_count) {
                                tr_room_t *rm = &g_level.rooms[lara_room];
                                float amb = 1.0f - (float)rm->ambient / 8191.0f;
                                if (amb < 0) amb = 0;
                                if (amb > 1) amb = 1;
                                float light_add = 0;
                                for (int li = 0; li < rm->light_count; li++) {
                                    float ldx = bx - (float)rm->lights[li].pos.x;
                                    float ldy = by - (float)rm->lights[li].pos.y;
                                    float ldz = bz - (float)rm->lights[li].pos.z;
                                    float ld2 = ldx*ldx + ldy*ldy + ldz*ldz;
                                    float lfo = (float)rm->lights[li].falloff;
                                    float lfo2 = lfo * lfo;
                                    if (lfo > 0 && ld2 < lfo2)
                                        light_add += ((float)rm->lights[li].intensity / 8191.0f) * (1.0f - ld2 / lfo2);
                                }
                                lara_shade = amb + light_add;
                                if (lara_shade > 1.0f) lara_shade = 1.0f;
                                if (lara_shade < 0.1f) lara_shade = 0.1f;
                            }
                            uint8_t lshade = (uint8_t)(lara_shade * 255.0f);

                            for (int mi = 0; mi < mc; mi++) {
                                if (mi > 0) {
                                    int bi = lara_obj->bone_idx + (mi - 1);
                                    if (bi >= 0 && bi < g_level.anim_bone_count) {
                                        tr_anim_bone_t *bone = &g_level.anim_bones[bi];
                                        if (bone->flags & 1) { if (psp>0) { psp--; bx=ps[psp][0]; by=ps[psp][1]; bz=ps[psp][2]; } }
                                        if (bone->flags & 2) { if (psp<7) { ps[psp][0]=bx; ps[psp][1]=by; ps[psp][2]=bz; psp++; } }
                                        float ox = (float)bone->x, oz = (float)bone->z;
                                        bx += ox*lcy_r - oz*lsy_r;
                                        by += (float)bone->y;
                                        bz += ox*lsy_r + oz*lcy_r;
                                    }
                                }

                                int gmi = lara_obj->mesh_idx + mi;
                                if (gmi >= g_level.mesh_count) continue;
                                tr_object_mesh_t *mesh = &g_level.meshes[gmi];

                                for (int f = 0; f < mesh->face_count; f++) {
                                    /* Lara exempt from face budget — she's fixed cost
                                     * (~6ms) and must always render completely. */
                                    perf_tex_faces++;
                                    tr_face_t *face = &mesh->faces[f];
                                    int nv = face->vertex_count;
                                    if (nv < 3) continue;

                                    uint16_t tex_idx = face->texture & 0x7FFF;
                                    bool is_flat = (face->texture & 0x8000) != 0;
                                    float l_fr = lara_shade, l_fg = lara_shade, l_fb = lara_shade;

                                    if (is_flat) {
                                        uint8_t ci = tex_idx & 0xFF;
                                        l_fr = (float)g_level.palette[ci*3+0] / 63.0f * lara_shade;
                                        l_fg = (float)g_level.palette[ci*3+1] / 63.0f * lara_shade;
                                        l_fb = (float)g_level.palette[ci*3+2] / 63.0f * lara_shade;
                                        if (white_px && tex_last_tex != -2) {
                                            rdpq_mode_tlut(TLUT_NONE);
                                            rdpq_tex_upload(TILE0, &white_surf, NULL);
                                            tex_last_tex = -2;
                                        }
                                    } else if (tex_idx < num_otex && ci8_surfs) {
                                        if ((int)tex_idx != tex_last_tex) {
                                            if (tex_last_tex == -2) rdpq_mode_tlut(TLUT_RGBA16);
                                            rdpq_tex_upload(TILE0, &ci8_surfs[tex_idx], NULL);
                                            tex_last_tex = (int)tex_idx;
                                        }
                                    } else continue;

                                    tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                        ? &g_level.object_textures[tex_idx] : NULL;

                                    float sv[4][10]; bool vis = true;
                                    for (int v = 0; v < nv; v++) {
                                        uint16_t vi = face->vertices[v];
                                        if (vi >= mesh->vertex_count) { vis = false; break; }
                                        float vx_l = (float)mesh->vertices[vi].x;
                                        float vz_l = (float)mesh->vertices[vi].z;
                                        float wx = bx + vx_l*lcy_r - vz_l*lsy_r;
                                        float wy = by + (float)mesh->vertices[vi].y;
                                        float wz = bz + vx_l*lsy_r + vz_l*lcy_r;

                                        float cx_v=wx-cam_x, cy_v=wy-cam_y, cz_v=wz-cam_z;
                                        float rx=cx_v*cy-cz_v*sy;
                                        float rz_yaw=cx_v*sy+cz_v*cy;
                                        float ry=cy_v*cp-rz_yaw*sp;
                                        float rz=cy_v*sp+rz_yaw*cp;
                                        /* Clamp z instead of reject — prevents
                                         * head/close mesh faces from vanishing */
                                        if (rz < 4.0f) rz = 4.0f;

                                        float inv_z=256.0f/rz;
                                        float scx2=160.0f+rx*inv_z, scy2=120.0f+ry*inv_z;
                                        if(scx2<-2048) scx2=-2048;
                                        if(scx2>2048)  scx2=2048;
                                        if(scy2<-2048) scy2=-2048;
                                        if(scy2>2048)  scy2=2048;
                                        sv[v][0]=scx2; sv[v][1]=scy2;
                                        sv[v][2]=rz/32768.0f; if(sv[v][2]>1.0f)sv[v][2]=1.0f;
                                        sv[v][3]=l_fr; sv[v][4]=l_fg; sv[v][5]=l_fb;
                                        sv[v][6]=1.0f;
                                        if (!is_flat && otex && tex_uv_map) {
                                            float tu=(float)(otex->uv[v].u>>8);
                                            float tv=(float)(otex->uv[v].v>>8);
                                            sv[v][7]=(tu-tex_uv_map[tex_idx*4+0])*tex_uv_map[tex_idx*4+2];
                                            sv[v][8]=(tv-tex_uv_map[tex_idx*4+1])*tex_uv_map[tex_idx*4+3];
                                        } else { sv[v][7]=0; sv[v][8]=0; }
                                        sv[v][9]=1.0f; /* affine */
                                    }
                                    if (!vis) continue;
                                    rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, sv[0], sv[1], sv[2]);
                                    perf_tex_tris++;
                                    if (nv == 4) {
                                        rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, sv[0], sv[2], sv[3]);
                                        perf_tex_tris++;
                                    }
                                    if ((perf_tex_tris & 7) == 0) rspq_flush();
                                }
                            }
                        }
                    }

#if 0 /* ROOM SPRITES — disabled (out-of-scope loop variable) */
                        /* ===== ROOM SPRITES (billboarded quads) ===== */
                        for (int si = 0; si < room->mesh.sprite_count && si < 20; si++) {
                            uint16_t svi = room->mesh.sprite_vertices[si];
                            uint16_t sti = room->mesh.sprite_textures[si];
                            if (svi >= room->mesh.vertex_count) continue;
                            if (sti >= g_level.sprite_texture_count) continue;

                            tr_room_vertex_t *rv = &room->mesh.vertices[svi];
                            tr_sprite_texture_t *spt = &g_level.sprite_textures[sti];

                            /* World position */
                            float wx = room->x + rv->x;
                            float wy = (float)rv->y;
                            float wz = room->z + rv->z;

                            /* Camera transform */
                            float cx_s = wx - cam_x, cy_s = wy - cam_y, cz_s = wz - cam_z;
                            float rx_s = cx_s * cy - cz_s * sy;
                            float rz_y = cx_s * sy + cz_s * cy;
                            float ry_s = cy_s * cp - rz_y * sp;
                            float rz_s = cy_s * sp + rz_y * cp;
                            if (rz_s < 4.0f) continue;

                            float inv_z = 256.0f / rz_s;
                            float scx = 160.0f + rx_s * inv_z;
                            float scy = 120.0f + ry_s * inv_z;

                            /* Sprite size in screen pixels */
                            float sw = (float)spt->width * inv_z * 0.5f;
                            float sh = (float)spt->height * inv_z * 0.5f;
                            if (sw < 0.5f || sh < 0.5f) continue;

                            /* Sprite texture from texture page */
                            uint16_t sp_page = spt->tex_page;
                            uint8_t sp_x = spt->offset & 0xFF;
                            uint8_t sp_y = (spt->offset >> 8) & 0xFF;

                            /* Find a matching object texture for this sprite region */
                            /* For now, render as a lit colored quad (sprite tex lookup is complex) */
                            float spr_bright = 1.0f - (float)rv->light / 8191.0f;
                            if (spr_bright < 0) spr_bright = 0;
                            if (spr_bright > 1) spr_bright = 1;
                            spr_bright *= (1.0f - rz_s / 16384.0f);
                            if (spr_bright < 0) spr_bright = 0;

                            if (white_px && tex_last_tex != -2) {
                                rdpq_tex_upload(TILE0, &white_surf, NULL);
                                tex_last_tex = -2;
                            }

                            float sv[4][10];
                            float zn = rz_s / 32768.0f;
                            if (zn > 1.0f) zn = 1.0f;
                            sv[0][0]=scx-sw; sv[0][1]=scy-sh; sv[0][2]=zn;
                            sv[1][0]=scx+sw; sv[1][1]=scy-sh; sv[1][2]=zn;
                            sv[2][0]=scx+sw; sv[2][1]=scy+sh; sv[2][2]=zn;
                            sv[3][0]=scx-sw; sv[3][1]=scy+sh; sv[3][2]=zn;
                            for (int v = 0; v < 4; v++) {
                                sv[v][3]=spr_bright; sv[v][4]=spr_bright*0.9f;
                                sv[v][5]=spr_bright*0.7f; sv[v][6]=1.0f;
                                sv[v][7]=0; sv[v][8]=0; sv[v][9]=1.0f; /* affine */
                            }
                            rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                            rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                        }
                    }

#endif /* ROOM SPRITES disabled */

#if 0 /* DRAW STATIC OBJECTS — superseded by TEXTURED PASS: STATIC MESHES (CI8, z-bias) */
                    for (int ri = 0; ri < num_draw; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];

                        for (int si = 0; si < room->static_mesh_count; si++) {
                            tr_room_static_mesh_t *sm = &room->static_meshes[si];

                            /* Quick frustum cull for static mesh */
                            float sdx = (float)sm->pos.x - cam_x;
                            float sdz = (float)sm->pos.z - cam_z;
                            float sm_rz = sdx * sy + sdz * cy;
                            if (sm_rz < -1024) continue; /* Behind camera */
                            float sm_rx = sdx * cy - sdz * sy;
                            if (sm_rz > 0) { float l=sm_rz*3.0f; if(sm_rx>l||sm_rx<-l) continue; }

                            /* Find the static object definition to get mesh_idx */
                            int mesh_idx = -1;
                            for (int oi = 0; oi < g_level.static_object_count; oi++) {
                                if (g_level.static_objects[oi].object_id == sm->static_mesh_id) {
                                    if (!(g_level.static_objects[oi].flags & 2)) break; /* not visible */
                                    mesh_idx = g_level.static_objects[oi].mesh_idx;
                                    break;
                                }
                            }
                            if (mesh_idx < 0 || mesh_idx >= g_level.mesh_count) continue;
                            tr_object_mesh_t *mesh = &g_level.meshes[mesh_idx];

                            /* Static mesh shade */
                            float sm_bright = 1.0f - (float)sm->shade / 8191.0f;
                            if (sm_bright < 0.1f) sm_bright = 0.1f;
                            if (sm_bright > 1.0f) sm_bright = 1.0f;

                            /* Y rotation for static mesh */
                            float sm_yaw = (float)sm->rotation * 3.14159f / 32768.0f;
                            float sm_sy = sinf(sm_yaw), sm_cy = cosf(sm_yaw);

                            for (int f = 0; f < mesh->face_count && f < 100; f++) {
                                tr_face_t *face = &mesh->faces[f];
                                if (face->vertex_count < 3) continue;

                                bool is_flat = (face->texture & 0x8000) != 0;
                                uint16_t tex_idx = face->texture & 0x7FFF;
                                tr_object_texture_t *otex = NULL;
                                float fr_r = sm_bright, fr_g = sm_bright, fr_b = sm_bright;

                                if (is_flat) {
                                    uint8_t ci = tex_idx & 0xFF;
                                    fr_r = (float)g_level.palette[ci*3+0] / 63.0f * sm_bright;
                                    fr_g = (float)g_level.palette[ci*3+1] / 63.0f * sm_bright;
                                    fr_b = (float)g_level.palette[ci*3+2] / 63.0f * sm_bright;
                                    if (white_px && tex_last_tex != -2) {
                                        rdpq_tex_upload(TILE0, &white_surf, NULL);
                                        tex_last_tex = -2;
                                    }
                                } else if (tex_idx < num_otex) {
                                    otex = &g_level.object_textures[tex_idx];
                                    if ((int)tex_idx != tex_last_tex) {
                                        rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                        tex_last_tex = (int)tex_idx;
                                    }
                                } else continue;

                                float sv[4][10];
                                bool visible = true;
                                for (int v = 0; v < face->vertex_count; v++) {
                                    uint16_t vi = face->vertices[v];
                                    if (vi >= mesh->vertex_count) { visible = false; break; }

                                    /* Rotate vertex by static mesh yaw */
                                    float lx = (float)mesh->vertices[vi].x;
                                    float lz = (float)mesh->vertices[vi].z;
                                    float wx = (float)sm->pos.x + lx * sm_cy - lz * sm_sy;
                                    float wy = (float)sm->pos.y + (float)mesh->vertices[vi].y;
                                    float wz = (float)sm->pos.z + lx * sm_sy + lz * sm_cy;

                                    float cx = wx - cam_x;
                                    float cy_val = wy - cam_y;
                                    float cz = wz - cam_z;
                                    float rx = cx * cy - cz * sy;
                                    float rz_yaw = cx * sy + cz * cy;
                                    float ry = cy_val * cp - rz_yaw * sp;
                                    float rz = cy_val * sp + rz_yaw * cp;
                                    if (rz < 32.0f) { visible = false; break; }

                                    float inv_z = 256.0f / rz;
                                    float fog = 1.0f - rz / 16384.0f;
                                    if (fog < 0) fog = 0;
                                    float scx = 160.0f + rx * inv_z;
                                    float scy = 120.0f + ry * inv_z;
                                    if (scx < -2048) scx = -2048;
                                    if (scx > 2048) scx = 2048;
                                    if (scy < -2048) scy = -2048;
                                    if (scy > 2048) scy = 2048;
                                    sv[v][0] = scx;
                                    sv[v][1] = scy;
                                    sv[v][2] = rz / 32768.0f;
                                    if (sv[v][2] > 1.0f) sv[v][2] = 1.0f;

                                    if (is_flat) {
                                        sv[v][3] = fr_r * fog; sv[v][4] = fr_g * fog;
                                        sv[v][5] = fr_b * fog; sv[v][6] = 1.0f;
                                        sv[v][7] = 0; sv[v][8] = 0;
                                    } else {
                                        sv[v][3] = sm_bright * fog; sv[v][4] = sm_bright * fog;
                                        sv[v][5] = sm_bright * fog; sv[v][6] = 1.0f;
                                        float tu = (float)(otex->uv[v].u >> 8);
                                        float tv = (float)(otex->uv[v].v >> 8);
                                        sv[v][7] = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                        sv[v][8] = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                    }
                                    sv[v][9] = 1.0f / rz;
                                }
                                if (!visible) continue;
                                rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                                if (face->vertex_count == 4)
                                    rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                            }
                        }
                    }
#endif /* DRAW STATIC OBJECTS disabled */

                    perf_tex_lara_us = (uint32_t)((float)TICKS_DISTANCE(_la_t0, TICKS_READ()) / (float)TICKS_PER_SECOND * 1000000.0f);
                    } /* end Lara timing */

                    /* ===== DRAW PLACED ITEMS (pickups, enemies, doors) ===== */
                    { uint32_t _it_t0 = TICKS_READ();
                    for (int ii = 0; ii < g_level.item_count && ii < 30; ii++) {
                        tr_item_t *item = &g_level.items[ii];
                        if (item->object_id == 0) continue; /* Skip Lara */
                        /* Only draw if item is in a visible room */
                        bool item_vis = false;
                        for (int ri = 0; ri < num_draw; ri++)
                            if (rooms_to_draw[ri] == item->room) { item_vis = true; break; }
                        if (!item_vis) continue;

                        /* Frustum cull item */
                        float idx = (float)item->pos.x - cam_x;
                        float idz = (float)item->pos.z - cam_z;
                        float item_rz = idx * sy + idz * cy;
                        if (item_rz < -2048) continue;
                        float item_rx = idx * cy - idz * sy;
                        if (item_rz > 0) { float l=item_rz*3.0f; if(item_rx>l||item_rx<-l) continue; }

                        /* Find object definition for this item */
                        tr_object_def_t *iobj = NULL;
                        for (int oi = 0; oi < g_level.object_count; oi++)
                            if (g_level.objects[oi].object_id == item->object_id) { iobj = &g_level.objects[oi]; break; }
                        if (!iobj || iobj->mesh_count < 1) continue;

                        /* Render first mesh only (no bone tree for items) */
                        int gmi = iobj->mesh_idx;
                        if (gmi >= g_level.mesh_count) continue;
                        tr_object_mesh_t *mesh = &g_level.meshes[gmi];

                        /* shade=0 means use room ambient, not fully dark */
                        float item_shade;
                        if (item->shade == 0 || item->shade == -1) {
                            tr_room_t *irm = &g_level.rooms[item->room];
                            item_shade = 1.0f - (float)irm->ambient / 8191.0f;
                        } else {
                            item_shade = 1.0f - (float)item->shade / 8191.0f;
                        }
                        if (item_shade < 0.1f) item_shade = 0.1f;
                        if (item_shade > 1.0f) item_shade = 1.0f;
                        float iyaw = (float)item->rotation * 3.14159f / 32768.0f;
                        float isy = sinf(iyaw), icy = cosf(iyaw);

                        for (int f = 0; f < mesh->face_count && f < 50; f++) {
                            if (perf_tex_tris >= TEX_TRI_BUDGET ||
                                perf_tex_faces >= TEX_FACE_BUDGET) goto near_pass_done;
                            perf_tex_faces++;
                            tr_face_t *face = &mesh->faces[f];
                            if (face->vertex_count < 3) continue;
                            bool is_flat = (face->texture & 0x8000) != 0;
                            uint16_t tex_idx = face->texture & 0x7FFF;
                            tr_object_texture_t *otex = NULL;
                            float ifr = item_shade, ifg = item_shade, ifb = item_shade;

                            if (is_flat) {
                                uint8_t ci = tex_idx & 0xFF;
                                ifr = (float)g_level.palette[ci*3+0]/63.0f*item_shade;
                                ifg = (float)g_level.palette[ci*3+1]/63.0f*item_shade;
                                ifb = (float)g_level.palette[ci*3+2]/63.0f*item_shade;
                                if (white_px && tex_last_tex != -2) {
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &white_surf, NULL);
                                    tex_last_tex = -2;
                                }
                            } else if (tex_idx < num_otex && ci8_surfs) {
                                otex = &g_level.object_textures[tex_idx];
                                if ((int)tex_idx != tex_last_tex) {
                                    if (tex_last_tex == -2) rdpq_mode_tlut(TLUT_RGBA16);
                                    rdpq_tex_upload(TILE0, &ci8_surfs[tex_idx], NULL);
                                    tex_last_tex = (int)tex_idx;
                                }
                            } else continue;

                            float sv[4][10]; bool vis = true;
                            for (int v = 0; v < face->vertex_count; v++) {
                                uint16_t vi = face->vertices[v];
                                if (vi >= mesh->vertex_count) { vis = false; break; }
                                float lx = (float)mesh->vertices[vi].x, lz_v = (float)mesh->vertices[vi].z;
                                float wx = (float)item->pos.x + lx*icy - lz_v*isy;
                                float wy = (float)item->pos.y + (float)mesh->vertices[vi].y;
                                float wz = (float)item->pos.z + lx*isy + lz_v*icy;
                                float cx_v = wx-cam_x, cy_v = wy-cam_y, cz_v = wz-cam_z;
                                float rx = cx_v*cy - cz_v*sy;
                                float rz_y = cx_v*sy + cz_v*cy;
                                float ry = cy_v*cp - rz_y*sp;
                                float rz = cy_v*sp + rz_y*cp;
                                if (rz < 32.0f) { vis = false; break; }
                                float inv_z = 256.0f / rz;
                                float fog = 1.0f - rz/16384.0f; if (fog < 0) fog = 0;
                                float scx2 = 160.0f + rx*inv_z, scy2 = 120.0f + ry*inv_z;
                                if (scx2<-2048) scx2=-2048;
                                if (scx2>2048) scx2=2048;
                                if (scy2<-2048) scy2=-2048;
                                if (scy2>2048) scy2=2048;
                                sv[v][0] = scx2; sv[v][1] = scy2;
                                sv[v][2] = rz/32768.0f; if (sv[v][2]>1) sv[v][2]=1;
                                if (is_flat) { sv[v][3]=ifr*fog; sv[v][4]=ifg*fog; sv[v][5]=ifb*fog; }
                                else { sv[v][3]=item_shade*fog; sv[v][4]=item_shade*fog; sv[v][5]=item_shade*fog; }
                                sv[v][6]=1.0f;
                                if (!is_flat && otex) {
                                    float tu=(float)(otex->uv[v].u>>8), tv=(float)(otex->uv[v].v>>8);
                                    sv[v][7]=(tu-tex_uv_map[tex_idx*4+0])*tex_uv_map[tex_idx*4+2];
                                    sv[v][8]=(tv-tex_uv_map[tex_idx*4+1])*tex_uv_map[tex_idx*4+3];
                                } else { sv[v][7]=0; sv[v][8]=0; }
                                sv[v][9] = 1.0f/rz;
                            }
                            if (!vis) continue;
                            rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, sv[0], sv[1], sv[2]);
                            perf_tex_tris++;
                            if (face->vertex_count == 4) {
                                rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, sv[0], sv[2], sv[3]);
                                perf_tex_tris++;
                            }
                            if ((perf_tex_tris & 7) == 0) rspq_flush();
                        }
                    }
                    perf_tex_item_us = (uint32_t)((float)TICKS_DISTANCE(_it_t0, TICKS_READ()) / (float)TICKS_PER_SECOND * 1000000.0f);
                    } /* end items timing */

#if 0 /* DRAW LARA — superseded by TEXTURED PASS: LARA (CI8 textures above) */
                    {
                        /* Find Lara's object + item (cached after first frame) */
                        static tr_object_def_t *lara_obj = NULL;
                        static tr_item_t *lara_item = NULL;
                        static bool lara_found = false;
                        if (!lara_found) {
                            for (int i = 0; i < g_level.object_count; i++)
                                if (g_level.objects[i].object_id == 0) { lara_obj = &g_level.objects[i]; break; }
                            for (int i = 0; i < g_level.item_count; i++)
                                if (g_level.items[i].object_id == 0) { lara_item = &g_level.items[i]; break; }
                            lara_found = true;
                        }

                        if (lara_obj && lara_item && lara_obj->mesh_count > 0) {
                            /* Static frame 0 pose (animation needs accumulated
                             * rotations to look right, which we don't have yet) */
                            int mc = lara_obj->mesh_count;
                            if (mc > 15) mc = 15;
                            int frame_size = 9 + mc * 2;
                            int base_off = lara_obj->frame_offset / 2;
                            int16_t *fr = (g_level.anim_frames && base_off + frame_size <= g_level.anim_frame_count)
                                        ? &g_level.anim_frames[base_off] : NULL;

                            /* Position stack (proven working) + per-mesh R matrix */
                            float ps[8][3]; int psp = 0;
                            float bx = (float)lara_item->pos.x;
                            float by = (float)lara_item->pos.y;
                            float bz = (float)lara_item->pos.z;
                            float lyaw = (float)lara_item->rotation * 3.14159f / 32768.0f;
                            float lsy = sinf(lyaw), lcy = cosf(lyaw);

                            /* Root offset from frame */
                            if (fr) {
                                float ox = (float)fr[6], oz = (float)fr[8];
                                bx += ox * lcy - oz * lsy;
                                by += (float)fr[7];
                                bz += ox * lsy + oz * lcy;
                            }

                            /* Dynamic lighting from room */
                            float lara_shade = 1.0f;
                            if (lara_room >= 0 && lara_room < g_level.room_count) {
                                tr_room_t *rm = &g_level.rooms[lara_room];
                                float amb = 1.0f - (float)rm->ambient / 8191.0f;
                                if (amb < 0) amb = 0;
                                if (amb > 1) amb = 1;
                                float light_add = 0;
                                for (int li = 0; li < rm->light_count; li++) {
                                    float dx = bx - (float)rm->lights[li].pos.x;
                                    float dy = by - (float)rm->lights[li].pos.y;
                                    float dz = bz - (float)rm->lights[li].pos.z;
                                    float dist_sq = dx*dx + dy*dy + dz*dz;
                                    float falloff = (float)rm->lights[li].falloff;
                                    float falloff_sq = falloff * falloff;
                                    if (falloff > 0 && dist_sq < falloff_sq)
                                        light_add += ((float)rm->lights[li].intensity / 8191.0f) * (1.0f - dist_sq / falloff_sq);
                                }
                                lara_shade = amb + light_add;
                                if (lara_shade > 1.0f) lara_shade = 1.0f;
                                if (lara_shade < 0.1f) lara_shade = 0.1f;
                            }

                            for (int mi = 0; mi < mc; mi++) {
                                if (mi > 0) {
                                    int bi = lara_obj->bone_idx + (mi - 1);
                                    if (bi >= 0 && bi < g_level.anim_bone_count) {
                                        tr_anim_bone_t *bone = &g_level.anim_bones[bi];
                                        if (bone->flags & 1) { if(psp>0){psp--;bx=ps[psp][0];by=ps[psp][1];bz=ps[psp][2];}}
                                        if (bone->flags & 2) { if(psp<7){ps[psp][0]=bx;ps[psp][1]=by;ps[psp][2]=bz;psp++;}}
                                        float ox=(float)bone->x, oz=(float)bone->z;
                                        bx += ox*lcy - oz*lsy;
                                        by += (float)bone->y;
                                        bz += ox*lsy + oz*lcy;
                                    }
                                }
                                /* Position-only rendering (no per-mesh rotation) */

                                int gmi = lara_obj->mesh_idx + mi;
                                if (gmi >= g_level.mesh_count) continue;
                                tr_object_mesh_t *mesh = &g_level.meshes[gmi];

                                for (int f = 0; f < mesh->face_count; f++) {
                                    tr_face_t *face = &mesh->faces[f];
                                    if (face->vertex_count < 3) continue;

                                    bool is_flat = (face->texture & 0x8000) != 0;
                                    uint16_t tex_idx = face->texture & 0x7FFF;
                                    tr_object_texture_t *otex = NULL;
                                    float fr = lara_shade, fg = lara_shade, fb = lara_shade;

                                    if (is_flat) {
                                        uint8_t ci = tex_idx & 0xFF;
                                        fr = (float)g_level.palette[ci*3+0] / 63.0f * lara_shade;
                                        fg = (float)g_level.palette[ci*3+1] / 63.0f * lara_shade;
                                        fb = (float)g_level.palette[ci*3+2] / 63.0f * lara_shade;
                                        if (white_px && tex_last_tex != -2) {
                                            rdpq_tex_upload(TILE0, &white_surf, NULL);
                                            tex_last_tex = -2;
                                        }
                                    } else if (tex_idx < num_otex) {
                                        otex = &g_level.object_textures[tex_idx];
                                        if ((int)tex_idx != tex_last_tex) {
                                            rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                            tex_last_tex = (int)tex_idx;
                                        }
                                    } else continue;

                                    float sv[4][10];
                                    bool visible = true;
                                    for (int v = 0; v < face->vertex_count; v++) {
                                        uint16_t vi = face->vertices[v];
                                        if (vi >= mesh->vertex_count) { visible = false; break; }
                                        float vx = (float)mesh->vertices[vi].x;
                                        float vz = (float)mesh->vertices[vi].z;
                                        float wx = bx + vx*lcy - vz*lsy;
                                        float wy = by + (float)mesh->vertices[vi].y;
                                        float wz = bz + vx*lsy + vz*lcy;

                                        float cx = wx - cam_x, cy_val = wy - cam_y, cz = wz - cam_z;
                                        float rx = cx * cy - cz * sy;
                                        float rz_yaw = cx * sy + cz * cy;
                                        float ry = cy_val * cp - rz_yaw * sp;
                                        float rz = cy_val * sp + rz_yaw * cp;
                                        if (rz < 32.0f) { visible = false; break; }

                                        float inv_z = 256.0f / rz;
                                        float lsx = 160.0f + rx * inv_z;
                                        float lsy2 = 120.0f + ry * inv_z;
                                        if (lsx<-2048) lsx=-2048;
                                        if (lsx>2048) lsx=2048;
                                        if (lsy2<-2048) lsy2=-2048;
                                        if (lsy2>2048) lsy2=2048;
                                        sv[v][0] = lsx;
                                        sv[v][1] = lsy2;
                                        sv[v][2] = rz / 32768.0f;
                                        if (sv[v][2] > 1.0f) sv[v][2] = 1.0f;
                                        if (is_flat) {
                                            sv[v][3]=fr; sv[v][4]=fg; sv[v][5]=fb;
                                        } else {
                                            sv[v][3]=lara_shade; sv[v][4]=lara_shade; sv[v][5]=lara_shade;
                                        }
                                        sv[v][6] = 1.0f;
                                        if (!is_flat && otex) {
                                            float tu = (float)(otex->uv[v].u >> 8);
                                            float tv = (float)(otex->uv[v].v >> 8);
                                            sv[v][7] = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                            sv[v][8] = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                        } else { sv[v][7] = 0; sv[v][8] = 0; }
                                        sv[v][9] = 1.0f / rz;
                                    }
                                    if (!visible) continue;
                                    rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                                    if (face->vertex_count == 4)
                                        rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                                }
                            }
                        }
                    }
#endif /* DRAW LARA disabled */

                    /* ===== SHADOW CIRCLE under Lara ===== */
                    {
                        tr_item_t *shadow_item = NULL;
                        for (int i = 0; i < g_level.item_count; i++)
                            if (g_level.items[i].object_id == 0) { shadow_item = &g_level.items[i]; break; }
                    if (shadow_item) {
                        float sx_pos = (float)shadow_item->pos.x;
                        float sy_pos = (float)shadow_item->pos.y + 10.0f;
                        float sz_pos = (float)shadow_item->pos.z;

                        if (white_px && tex_last_tex != -2) {
                            rdpq_tex_upload(TILE0, &white_surf, NULL);
                            tex_last_tex = -2;
                        }

                        /* Enable semi-transparent blending for shadow */
                        rdpq_mode_push();
                        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

                        /* 16-point circle (pre-computed) */
                        float sv_s[16][10];
                        bool vis = true;
                        for (int v = 0; v < 16; v++) {
                            float wx = sx_pos + shadow_circ[v][0];
                            float wy = sy_pos;
                            float wz = sz_pos + shadow_circ[v][1];
                            float cx_v=wx-cam_x, cy_v=wy-cam_y, cz_v=wz-cam_z;
                            float rx=cx_v*cy-cz_v*sy;
                            float rz_y=cx_v*sy+cz_v*cy;
                            float ry=cy_v*cp-rz_y*sp;
                            float rz=cy_v*sp+rz_y*cp;
                            if (rz < 32.0f) { vis = false; break; }
                            float inv_z=256.0f/rz;
                            float shx=160.0f+rx*inv_z, shy=120.0f+ry*inv_z;
                            if(shx<-2048)shx=-2048;
                            if(shx>2048)shx=2048;
                            if(shy<-2048)shy=-2048;
                            if(shy>2048)shy=2048;
                            sv_s[v][0]=shx;
                            sv_s[v][1]=shy;
                            sv_s[v][2]=rz/32768.0f-0.001f;
                            if(sv_s[v][2]>1)sv_s[v][2]=1;
                            if(sv_s[v][2]<0)sv_s[v][2]=0;
                            /* Dark with 40% opacity via alpha */
                            sv_s[v][3]=0; sv_s[v][4]=0;
                            sv_s[v][5]=0; sv_s[v][6]=0.4f;
                            sv_s[v][7]=0; sv_s[v][8]=0;
                            sv_s[v][9]=1.0f/rz;
                        }
                        if (vis) {
                            for (int t = 1; t < 15; t++)
                                rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX,
                                    sv_s[0], sv_s[t], sv_s[t+1]);
                        }
                        rdpq_mode_pop();
                    }}
                    near_pass_done:
                    /* End textured pass — record CPU time, measure RSP stall, detach */
                    rdpq_mode_tlut(TLUT_NONE);
                    {
                        uint32_t d = TICKS_DISTANCE(perf_tex_t0, TICKS_READ());
                        perf_tex_us = (uint32_t)((float)d / (float)TICKS_PER_SECOND * 1000000.0f);
                    }
                    /* Measure how long CPU stalls waiting for RSP/RDP to drain.
                     * Large value = RSP/RDP is the bottleneck.
                     * Near zero = CPU is the bottleneck (RSP always idle). */
                    {
                        uint32_t rsp_t0 = TICKS_READ();
                        rspq_wait();
                        uint32_t d = TICKS_DISTANCE(rsp_t0, TICKS_READ());
                        perf_rsp_us = (uint32_t)((float)d / (float)TICKS_PER_SECOND * 1000000.0f);
                    }

                    rdpq_detach_show();

                    /* Capture frame time */
                    {
                        uint32_t frame_end = TICKS_READ();
                        uint32_t delta = TICKS_DISTANCE(perf_frame_start, frame_end);
                        perf_frame_us = (uint32_t)((float)delta / (float)TICKS_PER_SECOND * 1000000.0f);
                    }

                    /* Audio: mix and submit to AI */
                    if (audio_can_write()) {
                        short *buf = audio_write_begin();
                        mixer_poll(buf, audio_get_buffer_length());
                        audio_write_end();
                    }

                    /* Play ambient cave sounds periodically */
                    static int ambient_timer = 0;
                    static int next_sample = 0;
                    static pcm_wave_t ambient_wave;
                    if (parsed_samples > 0 && ++ambient_timer > 200) {
                        ambient_timer = 0;
                        /* Find next valid sample */
                        for (int tries = 0; tries < 64; tries++) {
                            next_sample = (next_sample + 1) % g_level.sample_index_count;
                            if (next_sample < 64 && sample_pcm[next_sample] &&
                                sample_len[next_sample] > 200) break;
                        }
                        if (next_sample < 64 && sample_pcm[next_sample] &&
                            sample_len[next_sample] > 200) {
                            /* Flush sample data from cache for DMA */
                            data_cache_hit_writeback(sample_pcm[next_sample],
                                                     sample_len[next_sample] * 2);
                            pcm_wave_init(&ambient_wave, sample_pcm[next_sample],
                                          sample_len[next_sample],
                                          sample_rate_arr[next_sample]);
                            /* Play on channel 0 */
                            if (!mixer_ch_playing(0)) {
                                mixer_ch_play(0, &ambient_wave.wave);
                                mixer_ch_set_vol(0, 0.5f, 0.5f);
                            }
                        }
                    }
                }

            } else {
                printf("  Parse FAILED: %s\n", tr_parse_error_string(res));
            }
        } else {
            printf("  Failed to load from ROM!\n");
        }
    } else {
        printf("\nNo assets available.\n");
        printf("Embed assets: build_full_rom.py\n");
    }

    console_render();
    while (1) {}

    return 0;
}

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
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>
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
static uint8_t staging[512 * 1024] __attribute__((aligned(16)));

/* -------------------------------------------------------------------
 * Chunk management — builds AABB/sphere for each room once at load
 * ------------------------------------------------------------------- */

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
         * Clamp to [CHUNK_MID_DIST2 .. CHUNK_XXFAR_DIST2]. */
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
            /* Use system malloc for temp compressed buffer (freed after decompress) */
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
                 * Requires expansion pak (8MB) for the extra RDRAM. */
                uint16_t *tex_tiles = (uint16_t *)malloc_uncached_aligned(
                    64, num_otex * TEX_TILE_BYTES);
                surface_t *tex_surfs = (surface_t *)n64_mem_alloc(num_otex * sizeof(surface_t));
                /* Per-tile UV mapping: u0, v0, scale_s, scale_t */
                float *tex_uv_map = (float *)n64_mem_alloc(num_otex * 4 * sizeof(float));

                if (tex_tiles && tex_surfs && tex_uv_map) {
                    printf("  Converting %d textures to 16x16 RGBA16...\n", num_otex);
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

                        /* Store UV mapping */
                        tex_uv_map[i*4+0] = (float)u0;
                        tex_uv_map[i*4+1] = (float)v0;
                        tex_uv_map[i*4+2] = (float)TEX_TILE_DIM / (float)tw;
                        tex_uv_map[i*4+3] = (float)TEX_TILE_DIM / (float)th;

                        /* Convert CI8 tile to RGBA16, subsample to 16x16.
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

                        /* Uncached memory — no flush needed */
                        tex_surfs[i] = surface_make_linear(tile, FMT_RGBA16,
                                                           TEX_TILE_DIM, TEX_TILE_DIM);
                    }
                }
                printf("  %d/%d textures ready (%d KB). Heap: %u KB left.\n",
                       num_otex, (int)g_level.object_texture_count,
                       num_otex * TEX_TILE_BYTES / 1024,
                       n64_mem_available() / 1024);

                /* ---- Memory audit ----
                 * Print allocation totals so we can track what's consuming
                 * RDRAM.  Mip textures are NOT allocated — 560 KB uncached
                 * for all ~880 textures doesn't fit alongside the base
                 * textures + framebuffers + RSPQ blocks. */
                printf("\n=== MEMORY AUDIT ===\n");
                printf("  Bump heap:  %u KB used, %u KB free (of %u KB)\n",
                       (GAME_HEAP_SIZE - n64_mem_available()) / 1024,
                       n64_mem_available() / 1024,
                       GAME_HEAP_SIZE / 1024);
                printf("  Base textures: %d × %dx%d RGBA16 = %d KB (uncached)\n",
                       num_otex, TEX_TILE_DIM, TEX_TILE_DIM,
                       num_otex * TEX_TILE_BYTES / 1024);
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

                /* On-demand mip buffer — one reusable 16×16 RGBA16 surface.
                 * Instead of pre-allocating mips for all 880 textures,
                 * box-filter on-the-fly for the ~8 uploads per frame.
                 * Only 512 bytes uncached — negligible memory cost. */
                /* ---- Pre-baked room tiles (Lambert-style) ----
                 * Load per-room baked textures from ROM if available.
                 * Each room has 3 mip levels: 32×32, 16×16, 8×8.
                 * Far pass uploads pre-baked tile = zero runtime filtering.
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

                /* Baked RGBA16 tile offsets from ROM.
                 * Pixels loaded into CACHED heap (not uncached) to avoid
                 * competing with RSPQ block memory. Cache written back
                 * once after DMA so rdpq_tex_upload reads correct data. */
                #if __has_include("baked_offsets.h")
                #include "baked_offsets.h"
                #else
                #define BAKED_ROM_OFFSET 0u
                #define BAKED_ROM_SIZE   0u
                #endif

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
                            data_cache_hit_writeback(baked_data, total_load);
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

                /* Fallback: on-demand mip buffers (used when no baked data) */
                uint16_t *mip1_buf = NULL, *mip2_buf = NULL;
                surface_t mip1_surf = {0}, mip2_surf = {0};
                if (baked_room_count == 0) {
                    mip1_buf = (uint16_t *)malloc_uncached_aligned(
                        64, MIP1_DIM * MIP1_DIM * 2);
                    mip2_buf = (uint16_t *)malloc_uncached_aligned(
                        64, MIP2_DIM * MIP2_DIM * 2);
                    if (mip1_buf)
                        mip1_surf = surface_make_linear(mip1_buf, FMT_RGBA16, MIP1_DIM, MIP1_DIM);
                    if (mip2_buf)
                        mip2_surf = surface_make_linear(mip2_buf, FMT_RGBA16, MIP2_DIM, MIP2_DIM);
                }

                /* GL texture objects — created after gl_init() */
                GLuint *gl_textures = NULL;
                GLuint gl_white_tex = 0;
                if (num_otex > 0 && tex_surfs) {
                    gl_textures = (GLuint *)n64_mem_alloc(num_otex * sizeof(GLuint));
                }

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

                /* Build chunk array (one per room) for frustum/LOD queries */
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

                /* Close console, init display + OpenGL for RSP T&L */
                console_close();
                display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
                rdpq_init();
                gl_init();
                bool has_zbuf = true;  /* GL manages zbuf internally */
                bool gl_setup_done = false;  /* Defer GL state to first frame */

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

                /* Per-frame perf counters (updated each frame, shown on pause) */
                int perf_far_rooms = 0;
                int perf_far_faces __attribute__((unused)) = 0;
                int perf_far_tris = 0;
                int perf_far_uploads = 0;
                int perf_near_rooms = 0;
                int perf_near_faces __attribute__((unused)) = 0;
                int perf_near_binds = 0;

                /* Active fog color — lerps toward current room's fog.
                 * Starts at neutral cave fog. */
                float active_fog_r = 0.35f;
                float active_fog_g = 0.32f;
                float active_fog_b = 0.40f;

                /* Render loop */
                render_loop:
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

                    /* Start: pause with debug overlay */
                    if (pressed.start) {
                        /* End current GL context so we can draw text */
                        gl_context_end();

                        /* Get the framebuffer we just rendered to and draw text on it */
                        surface_t *pause_disp = display_get();
                        graphics_set_color(0xFFFFFFFF, 0x000000C0);

                        char buf[64];
                        graphics_draw_text(pause_disp, 8, 8, "PAUSED - Press START");
                        snprintf(buf, sizeof(buf), "Cam: %.0f, %.0f, %.0f", cam_x, cam_y, cam_z);
                        graphics_draw_text(pause_disp, 8, 20, buf);
                        snprintf(buf, sizeof(buf), "FPS: %d  Room: %d", fps_display, lara_room);
                        graphics_draw_text(pause_disp, 8, 32, buf);
                        snprintf(buf, sizeof(buf), "Coll: %s", collision_on ? "ON" : "OFF");
                        graphics_draw_text(pause_disp, 8, 44, buf);
                        snprintf(buf, sizeof(buf), "Audio: %d samples", parsed_samples);
                        graphics_draw_text(pause_disp, 8, 56, buf);
                        {
                            int32_t pfl, pcl;
                            GET_SECTOR_HEIGHTS(cam_x, cam_z, cam_y, pfl, pcl);
                            snprintf(buf, sizeof(buf), "Floor: %d  Ceil: %d", pfl, pcl);
                            graphics_draw_text(pause_disp, 8, 68, buf);
                        }
                        snprintf(buf, sizeof(buf), "Far: %dr %dt %du",
                                 perf_far_rooms, perf_far_tris, perf_far_uploads);
                        graphics_draw_text(pause_disp, 8, 80, buf);
                        snprintf(buf, sizeof(buf), "Near: %dr %db",
                                 perf_near_rooms, perf_near_binds);
                        graphics_draw_text(pause_disp, 8, 92, buf);
                        snprintf(buf, sizeof(buf), "Heap: %uKB free",
                                 n64_mem_available() / 1024);
                        graphics_draw_text(pause_disp, 8, 104, buf);

                        display_show(pause_disp);

                        /* Wait for START to unpause */
                        while (1) {
                            joypad_poll();
                            if (joypad_get_buttons_pressed(JOYPAD_PORT_1).start) break;
                        }
                        /* Resume — next iteration will call display_get + gl_context_begin */
                        continue;
                    }

                    /* ===== BEGIN FRAME =====
                     * Frame order:
                     *   1. rdpq_attach (framebuffer + zbuf)
                     *   2. rdpq_clear (color + depth)
                     *   3. RDPQ far pass (outside GL — future, empty stub now)
                     *   4. gl_context_begin()
                     *   5. GL near pass (existing RSP T&L pipeline)
                     *   6. gl_context_end()
                     *   7. rdpq_detach_show()
                     *
                     * This split lets the far pass use raw RDPQ texture
                     * uploads without competing for GL object/RSPQ memory. */
                    /* Reset per-frame perf counters */
                    perf_far_rooms = 0; perf_far_faces = 0;
                    perf_far_tris = 0;  perf_far_uploads = 0;
                    perf_near_rooms = 0; perf_near_faces = 0;
                    perf_near_binds = 0;

                    surface_t *disp = display_get();
                    rdpq_attach(disp, display_get_zbuf());

                    /* Clear color + depth via proper rdpq API.
                     * Using rdpq_clear instead of manual set_mode_fill +
                     * fill_rectangle to avoid RDP mode tracking conflicts
                     * with the subsequent set_mode_standard for the far pass. */
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
                    #define MAX_DRAW_ROOMS 16
                    #define STREAM_PRI_HIGH   2
                    #define STREAM_PRI_MED    1
                    #define STREAM_PRI_LOW    0
                    int vis_rooms[MAX_DRAW_ROOMS];
                    int vis_count = 0;
                    int vis_stream_pri[MAX_DRAW_ROOMS]; /* per-room streaming priority */

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

                    /* Ring 2: rooms connected to connected rooms.
                     * Only added if the portal leading to them is
                     * visible to the camera — no point fogging rooms
                     * the player can't see. */
                    int ring1_end = vis_count;
                    for (int vi = 0; vi < ring1_end && vis_count < MAX_DRAW_ROOMS; vi++) {
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

                            /* Only add if portal is in camera view */
                            int front = 0;
                            for (int pv = 0; pv < 4; pv++) {
                                float px = (float)(r1r->x + portal->vertices[pv].x) - cam_x;
                                float pz = (float)(r1r->z + portal->vertices[pv].z) - cam_z;
                                float rz_p = px * sy + pz * cy;
                                if (rz_p > -256.0f) front++;
                            }
                            if (front == 0) continue; /* entirely behind camera */

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

                    /* LOD classification.
                     * Ring 1 (current + connected): GL near pass, full textures.
                     * Ring 2 (connected-to-connected): RDPQ far pass, IA4 + fog.
                     * Rule: never see void — ring 2 fills the gaps. */
                    int near_rooms[MAX_DRAW_ROOMS], near_count = 0;
                    int far_rooms [MAX_DRAW_ROOMS], far_count  = 0;

                    for (int vi = 0; vi < vis_count; vi++) {
                        int ri = vis_rooms[vi];
                        tr_chunk_t *c = &g_chunks[ri];

                        if (vi < ring1_end) {
                            /* Ring 1: GL near pass with proper textures.
                             * Use per-room near threshold — small rooms
                             * transition to fog sooner, large rooms later.
                             * If too far for safe decimation (>3x near),
                             * demote to ring 2 IA4 fog instead. */
                            if (ri != lara_room && c->dist_sq > c->near_dist2 * 3.0f) {
                                /* Too far for GL — fall back to IA4 fog */
                                c->lod = CHUNK_LOD_FAR;
                                if (far_count < MAX_DRAW_ROOMS)
                                    far_rooms[far_count++] = ri;
                            } else if (c->dist_sq < c->near_dist2) {
                                c->lod = CHUNK_LOD_NEAR;
                                if (near_count < MAX_DRAW_ROOMS)
                                    near_rooms[near_count++] = ri;
                            } else {
                                c->lod = CHUNK_LOD_MID;
                                if (near_count < MAX_DRAW_ROOMS)
                                    near_rooms[near_count++] = ri;
                            }
                        } else {
                            /* Ring 2: RDPQ far pass, IA4 fog fallback */
                            c->lod = CHUNK_LOD_FAR;
                            if (far_count < MAX_DRAW_ROOMS)
                                far_rooms[far_count++] = ri;
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
                        else if (cl->dist_sq < CHUNK_VERY_FAR_DIST2)
                            cl->face_skip = 2;       /* far: every 2nd face */
                        else
                            cl->face_skip = 4;       /* very far: every 4th */
                    }

                    /* Bubble-sort far rooms back-to-front (prep for Phase 2 far pass).
                     * Small list so insertion sort is fine. */
                    for (int i = 1; i < far_count; i++) {
                        int key = far_rooms[i];
                        float key_d = g_chunks[key].dist_sq;
                        int j = i - 1;
                        while (j >= 0 && g_chunks[far_rooms[j]].dist_sq < key_d) {
                            far_rooms[j + 1] = far_rooms[j];
                            j--;
                        }
                        far_rooms[j + 1] = key;
                    }

                    /* Merge: near rooms first, then far rooms */
                    int rooms_to_draw[MAX_DRAW_ROOMS * 2];
                    int num_draw = 0;
                    for (int i = 0; i < near_count; i++)
                        rooms_to_draw[num_draw++] = near_rooms[i];
                    for (int i = 0; i < far_count; i++)
                        rooms_to_draw[num_draw++] = far_rooms[i];

                    /* ===== RDPQ FAR PASS (performance-optimized) =====
                     * CPU-projected, textured + shaded triangles via rdpq.
                     * Key perf choices:
                     *   - rspq_flush (non-blocking) not rspq_wait (blocking)
                     *   - Flush every 80 tris (not 20) to reduce sync overhead
                     *   - Per-face centroid back-cull before vertex projection
                     *   - Cap far rooms at 16 to limit total far-pass work
                     *   - Room origin pre-computed as float outside face loop */
                    {
                        rdpq_set_mode_standard();
                        rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
                        rdpq_mode_blender(0);
                        rdpq_mode_filter(FILTER_BILINEAR);

                        #define NEAR_Z 128.0f
                        int far_last_tex = -1;
                        int far_tris = 0;
                        int far_rooms_max = far_count; /* no room cap — tri budget limits work */
                        #define FAR_TRI_BUDGET   300
                        #define FAR_UPLOAD_BUDGET  8

                        for (int fi = 0; fi < far_rooms_max; fi++) {
                            int room_idx = far_rooms[fi];
                            if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                            tr_room_t *room = &g_level.rooms[room_idx];

                            /* Pre-baked room tile: upload once per room.
                             * Choose mip level by distance.  When baked data
                             * is available, this replaces ALL per-face uploads
                             * for this room — one upload instead of many. */
                            bool room_baked = false;
                            float room_uv_scale = 1.0f;
                            if (baked_room_count > 0 && room_idx < baked_room_count) {
                                float rd = (room_idx < g_chunk_count)
                                    ? g_chunks[room_idx].dist_sq : 0.0f;
                                bool is_ring2 = (room_idx < g_chunk_count &&
                                    g_chunks[room_idx].lod == CHUNK_LOD_FAR);
                                /* Ring 2 rooms always use baked IA4 as void fallback.
                                 * Other rooms: only bake in FAR zone. */
                                if (!is_ring2 && rd < CHUNK_MID_DIST2) goto skip_baked;
                                /* Ring 2: force IA4 tier (cheapest, fog-blended) */
                                if (is_ring2) rd = CHUNK_XXFAR_DIST2 + 1.0f;
                                rspq_flush();
                                if (rd > CHUNK_XXFAR_DIST2 && bkd_m4_surf) {
                                    /* IA4 extreme far: 4×4 silhouette, no TLUT */
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &bkd_m4_surf[room_idx], NULL);
                                    room_uv_scale = 4.0f / 32.0f;
                                } else if (rd > CHUNK_XFAR_DIST2 && bkd_m3_surf) {
                                    /* IA8 extra far: 8×8 grayscale, no TLUT */
                                    rdpq_mode_tlut(TLUT_NONE);
                                    rdpq_tex_upload(TILE0, &bkd_m3_surf[room_idx], NULL);
                                    room_uv_scale = 8.0f / 32.0f;
                                } else if (rd > CHUNK_VERY_FAR_DIST2 && bkd_m2_surf) {
                                    /* CI4 far: 8×8 + 16-color TLUT */
                                    rdpq_mode_tlut(TLUT_RGBA16);
                                    rdpq_tex_upload_tlut(bkd_m2_pal[room_idx], 0, 16);
                                    rdpq_tex_upload(TILE0, &bkd_m2_surf[room_idx], NULL);
                                    room_uv_scale = 8.0f / 32.0f;
                                } else if (bkd_m1_surf) {
                                    /* CI8 mid-far: 16×16 + 256-color TLUT */
                                    rdpq_mode_tlut(TLUT_RGBA16);
                                    rdpq_tex_upload_tlut(bkd_m1_pal[room_idx], 0, 256);
                                    rdpq_tex_upload(TILE0, &bkd_m1_surf[room_idx], NULL);
                                    room_uv_scale = 16.0f / 32.0f;
                                }
                                room_baked = true;
                                far_last_tex = -99;
                                perf_far_uploads++;
                            }
                            skip_baked:;
                            bool is_water = (room->flags & 1) != 0;
                            perf_far_rooms++;

                            /* Pre-compute room origin offset (avoid per-vertex int→float) */
                            float room_ox = (float)room->x - cam_x;
                            float room_oz = (float)room->z - cam_z;

                            for (int f = 0; f < room->mesh.face_count; f++) {
                                tr_face_t *face = &room->mesh.faces[f];
                                int nv = face->vertex_count;
                                if (nv < 3) continue;

                                /* Quick centroid back-cull: check if face center
                                 * is behind camera using first vertex only (cheap). */
                                {
                                    uint16_t v0i = face->vertices[0];
                                    if (v0i < room->mesh.vertex_count) {
                                        tr_room_vertex_t *rv0 = &room->mesh.vertices[v0i];
                                        float cz0 = (room_ox + rv0->x) * sy +
                                                     (room_oz + rv0->z) * cy;
                                        if (cz0 < -512.0f) continue; /* behind camera */
                                    }
                                }

                                /* Texture: skip per-face upload if room has baked tile */
                                uint16_t tex_idx = face->texture & 0x7FFF;
                                bool is_flat = (face->texture & 0x8000) != 0;
                                if (room_baked) goto far_skip_upload;

                                if (!is_flat && tex_idx < num_otex && tex_surfs) {
                                    if ((int)tex_idx != far_last_tex) {
                                        /* Lambert LOD: choose mip from projected size.
                                         * Estimate screen area from room distance.
                                         * lod ≈ 1/2 * log2(screenPixelArea)
                                         * Simplified: use room dist_sq thresholds. */
                                        float rd = (room_idx < g_chunk_count)
                                            ? g_chunks[room_idx].dist_sq : 0.0f;
                                        /* mip_level: 0=32×32, 1=16×16, 2=8×8 */
                                        int mip_level = 0;
                                        if (rd > CHUNK_VERY_FAR_DIST2) mip_level = 2;
                                        else if (rd > CHUNK_MID_DIST2) mip_level = 1;

                                        /* Fallback chain (Lambert):
                                         * If upload budget exceeded, try lower mip.
                                         * If still over budget, use last-bound texture
                                         * (never skip geometry due to budget). */
                                        if (perf_far_uploads >= FAR_UPLOAD_BUDGET) {
                                            if (mip_level < 2) mip_level = 2; /* try 8×8 */
                                            else goto far_skip_upload; /* use last-bound tex */
                                        }

                                        rspq_flush();
                                        uint16_t *src = tex_tiles + tex_idx * TEX_TILE_DIM * TEX_TILE_DIM;

                                        if (mip_level == 2 && mip2_buf) {
                                            /* 32→8: 4×4 box filter */
                                            for (int my = 0; my < MIP2_DIM; my++)
                                                for (int mx = 0; mx < MIP2_DIM; mx++) {
                                                    int bx = mx*4, by = my*4;
                                                    int sr=0,sg=0,sb=0,sa=0;
                                                    for (int dy=0;dy<4;dy++)
                                                        for (int dx=0;dx<4;dx++) {
                                                            uint16_t p = src[(by+dy)*TEX_TILE_DIM+(bx+dx)];
                                                            sr+=p>>11; sg+=(p>>6)&0x1F;
                                                            sb+=(p>>1)&0x1F; sa|=p&1;
                                                        }
                                                    mip2_buf[my*MIP2_DIM+mx] =
                                                        (((sr+8)>>4)<<11)|(((sg+8)>>4)<<6)|(((sb+8)>>4)<<1)|sa;
                                                }
                                            rdpq_tex_upload(TILE0, &mip2_surf, NULL);
                                        } else if (mip_level == 1 && mip1_buf) {
                                            /* 32→16: 2×2 box filter */
                                            for (int my = 0; my < MIP1_DIM; my++)
                                                for (int mx = 0; mx < MIP1_DIM; mx++) {
                                                    int bx = mx*2, by = my*2;
                                                    uint16_t p00 = src[by*TEX_TILE_DIM+bx];
                                                    uint16_t p10 = src[by*TEX_TILE_DIM+bx+1];
                                                    uint16_t p01 = src[(by+1)*TEX_TILE_DIM+bx];
                                                    uint16_t p11 = src[(by+1)*TEX_TILE_DIM+bx+1];
                                                    int ar = ((p00>>11)+(p10>>11)+(p01>>11)+(p11>>11)+2)>>2;
                                                    int ag = (((p00>>6)&0x1F)+((p10>>6)&0x1F)+((p01>>6)&0x1F)+((p11>>6)&0x1F)+2)>>2;
                                                    int ab = (((p00>>1)&0x1F)+((p10>>1)&0x1F)+((p01>>1)&0x1F)+((p11>>1)&0x1F)+2)>>2;
                                                    int aa = (p00|p10|p01|p11)&1;
                                                    mip1_buf[my*MIP1_DIM+mx] = (ar<<11)|(ag<<6)|(ab<<1)|aa;
                                                }
                                            rdpq_tex_upload(TILE0, &mip1_surf, NULL);
                                        } else {
                                            /* Level 0: full 32×32 base */
                                            rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                        }
                                        far_last_tex = (int)tex_idx;
                                        perf_far_uploads++;
                                    }
                                } else if (is_flat) {
                                    if (white_px && far_last_tex != -2) {
                                        if (perf_far_uploads >= FAR_UPLOAD_BUDGET)
                                            goto far_skip_upload; /* use last-bound tex */
                                        rspq_flush();
                                        rdpq_tex_upload(TILE0, &white_surf, NULL);
                                        far_last_tex = -2;
                                        perf_far_uploads++;
                                    }
                                } else continue;

                                far_skip_upload:;
                                tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                    ? &g_level.object_textures[tex_idx] : NULL;

                                /* CPU transform to camera space (pre-projection).
                                 * cv[v]: rx, ry, rz, r, g, b, ts, tt */
                                float cv[4][8];
                                bool visible = true;
                                bool any_behind = false, all_behind = true;

                                /* Per-room fog (constant across vertices).
                                 * Ring 2 (void-prevention) rooms get heavy
                                 * fog so they blend into the background. */
                                float room_d = (room_idx < g_chunk_count)
                                    ? g_chunks[room_idx].dist_sq : 0.0f;
                                float fog = (room_d - 16777216.0f) / 383223784.0f;
                                if (fog < 0.0f) fog = 0.0f;
                                if (fog > 0.85f) fog = 0.85f;
                                /* Ring 2 rooms: minimum 60% fog */
                                if (room_idx < g_chunk_count &&
                                    g_chunks[room_idx].lod == CHUNK_LOD_FAR &&
                                    fog < 0.6f) {
                                    fog = 0.6f;
                                }

                                float rd_uv = room_d;
                                float uv_s = 1.0f;
                                if (mip2_buf && rd_uv > CHUNK_VERY_FAR_DIST2) uv_s = 0.25f;
                                else if (mip1_buf && rd_uv > CHUNK_MID_DIST2) uv_s = 0.5f;

                                for (int v = 0; v < nv; v++) {
                                    uint16_t vi = face->vertices[v];
                                    if (vi >= room->mesh.vertex_count) { visible = false; break; }
                                    tr_room_vertex_t *rv = &room->mesh.vertices[vi];

                                    float cx_v = room_ox + rv->x;
                                    float cy_v = (float)rv->y - cam_y;
                                    float cz_v = room_oz + rv->z;

                                    float rx = cx_v * cy - cz_v * sy;
                                    float rz_yaw = cx_v * sy + cz_v * cy;
                                    float ry = cy_v * cp - rz_yaw * sp;
                                    float rz = cy_v * sp + rz_yaw * cp;

                                    if (rz < NEAR_Z) any_behind = true;
                                    else all_behind = false;

                                    /* Lighting + fog */
                                    float bright = 1.0f - ((float)rv->light / 8191.0f);
                                    if (bright < 0.0f) bright = 0.0f;
                                    if (bright > 1.0f) bright = 1.0f;
                                    bright = 0.4f + bright * 0.6f;
                                    float r = is_water ? bright * 0.4f : bright;
                                    float g = is_water ? bright * 0.6f : bright;
                                    float b = is_water ? bright * 1.0f : bright;
                                    r = r * (1.0f - fog) + active_fog_r * fog;
                                    g = g * (1.0f - fog) + active_fog_g * fog;
                                    b = b * (1.0f - fog) + active_fog_b * fog;

                                    /* Tex coords */
                                    float ts = 0, tt = 0;
                                    if (room_baked) {
                                        float baked_dim = MIP0_DIM * room_uv_scale;
                                        ts = ((float)(v % 2)) * baked_dim;
                                        tt = ((float)(v / 2)) * baked_dim;
                                    } else if (otex && tex_uv_map) {
                                        float tu = (float)(otex->uv[v].u >> 8);
                                        float tv = (float)(otex->uv[v].v >> 8);
                                        ts = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2] * uv_s;
                                        tt = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3] * uv_s;
                                    }

                                    cv[v][0] = rx;  cv[v][1] = ry;  cv[v][2] = rz;
                                    cv[v][3] = r;   cv[v][4] = g;   cv[v][5] = b;
                                    cv[v][6] = ts;  cv[v][7] = tt;
                                }
                                if (!visible || all_behind) continue;

                                /* Near-plane clip (Sutherland-Hodgman against z=NEAR_Z).
                                 * Max output: nv+1 verts (tri→4, quad→5). */
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
                                            /* Edge crosses near plane — interpolate */
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

                                /* Project clipped vertices to screen */
                                float sv[6][9];
                                bool screen_ok = true;
                                for (int v = 0; v < clip_nv; v++) {
                                    float rz = clipped[v][2];
                                    float inv_z = 256.0f / rz;
                                    float scx = 160.0f + clipped[v][0] * inv_z;
                                    float scy = 120.0f + clipped[v][1] * inv_z;
                                    if (scx < -400 || scx > 720 ||
                                        scy < -300 || scy > 540) {
                                        screen_ok = false; break;
                                    }
                                    sv[v][0] = scx;  sv[v][1] = scy;
                                    sv[v][2] = clipped[v][3]; sv[v][3] = clipped[v][4];
                                    sv[v][4] = clipped[v][5]; sv[v][5] = 1.0f;
                                    sv[v][6] = clipped[v][6]; sv[v][7] = clipped[v][7];
                                    sv[v][8] = inv_z;
                                }
                                if (!screen_ok) continue;

                                int num_tris = clip_nv - 2;
                                for (int tri = 0; tri < num_tris; tri++) {
                                    rdpq_triangle(&TRIFMT_SHADE_TEX,
                                        sv[0], sv[tri+1], sv[tri+2]);
                                    far_tris++;
                                    perf_far_tris = far_tris;
                                    if ((far_tris & 79) == 0)
                                        rspq_flush();
                                    if (far_tris >= FAR_TRI_BUDGET)
                                        goto far_pass_done;
                                }
                            }
                        }
                    }
                    far_pass_done:

                    /* Flush far pass commands before GL context begins.
                     * Non-blocking — far pass is now light enough (8 uploads,
                     * 300 tri cap) that queue backlog is not a risk. */
                    /* Reset RDPQ state before entering GL — the far pass
                     * may have set TLUT mode for CI8/CI4 textures which
                     * could corrupt GL's RGBA16 texture interpretation. */
                    rdpq_mode_tlut(TLUT_NONE);
                    rspq_flush();

                    /* ===== GL NEAR PASS ===== */
                    gl_context_begin();

                    /* One-time GL state setup on first frame */
                    if (!gl_setup_done) {
                        gl_setup_done = true;

                        #define WORLD_SCALE (1.0f / 32.0f)

                        float near_p = 4.0f;
                        float far_p = 350.0f;
                        float fov_half = 30.0f * 3.14159f / 180.0f;
                        float top = near_p * tanf(fov_half);
                        float right = top * (320.0f / 240.0f);
                        glMatrixMode(GL_PROJECTION);
                        glLoadIdentity();
                        glFrustum(-right, right, -top, top, near_p, far_p);

                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(GL_LESS);
                        glDisable(GL_CULL_FACE);
                        glShadeModel(GL_SMOOTH);

                        glEnable(GL_TEXTURE_2D);
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        glDisable(GL_LIGHTING);
                        glDisable(GL_FOG);

                        if (gl_textures) {
                            glGenTextures(num_otex, gl_textures);
                            for (int i = 0; i < num_otex; i++) {
                                glBindTexture(GL_TEXTURE_2D, gl_textures[i]);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                                glSurfaceTexImageN64(GL_TEXTURE_2D, 0, &tex_surfs[i], NULL);
                            }
                        }
                        if (white_px) {
                            glGenTextures(1, &gl_white_tex);
                            glBindTexture(GL_TEXTURE_2D, gl_white_tex);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                            glSurfaceTexImageN64(GL_TEXTURE_2D, 0, &white_surf, NULL);
                        }
                    }

                    glClear(GL_DEPTH_BUFFER_BIT);

                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity();
                    glRotatef(cam_rot_x * 180.0f / 3.14159f, 1, 0, 0);
                    glRotatef(cam_rot_y * 180.0f / 3.14159f, 0, 1, 0);

                    /* ===== GL NEAR PASS — interleaved vertex array ===== */
                    /* Interleaved: pos(3f) + color(4ub) + texcoord(2f) = 24 bytes/vert */
                    typedef struct {
                        float x, y, z;
                        uint8_t r, g, b, a;
                        float s, t;
                    } gl_vert_t;
                    #define MAX_GL_VERTS 4000
                    static gl_vert_t gl_verts[MAX_GL_VERTS] __attribute__((aligned(16)));
                    int gl_vc = 0;
                    int cur_tex = -1;
                    #define VERT_STRIDE sizeof(gl_vert_t)

                    #define FLUSH_GL() do { \
                        if (gl_vc > 0) { \
                            data_cache_hit_writeback(gl_verts, gl_vc * VERT_STRIDE); \
                            glEnableClientState(GL_VERTEX_ARRAY); \
                            glEnableClientState(GL_COLOR_ARRAY); \
                            glEnableClientState(GL_TEXTURE_COORD_ARRAY); \
                            glVertexPointer(3, GL_FLOAT, VERT_STRIDE, &gl_verts[0].x); \
                            glColorPointer(4, GL_UNSIGNED_BYTE, VERT_STRIDE, &gl_verts[0].r); \
                            glTexCoordPointer(2, GL_FLOAT, VERT_STRIDE, &gl_verts[0].s); \
                            glDrawArrays(GL_TRIANGLES, 0, gl_vc); \
                            glDisableClientState(GL_VERTEX_ARRAY); \
                            glDisableClientState(GL_COLOR_ARRAY); \
                            glDisableClientState(GL_TEXTURE_COORD_ARRAY); \
                            gl_vc = 0; \
                        } \
                    } while(0)

                    /* ===== GL NEAR PASS ONLY =====
                     * Far pass moved to RDPQ (before gl_context_begin).
                     * Near rooms are in rooms_to_draw[0 .. near_count-1]. */
                    {
                        int loop_start = 0;
                        int loop_end   = near_count;
                        int pass       = 1;  /* near pass identifier for fog logic */

                    for (int ri = loop_start; ri < loop_end; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];
                        bool is_water = (room->flags & 1) != 0;
                        perf_near_rooms++;

                        /* Face decimation: skip faces for distant rooms.
                         * Current room (lara_room): all faces.
                         * Connected rooms beyond near threshold: skip
                         * every Nth face based on distance ratio.
                         * Cap at 2 — never decimate harder than 50%.
                         * Beyond that, the room should be drawn as IA4
                         * fog (ring 2) instead of decimated GL. */
                        int face_skip = 1; /* 1 = all faces */
                        if (room_idx != lara_room && room_idx < g_chunk_count) {
                            tr_chunk_t *c = &g_chunks[room_idx];
                            if (c->dist_sq > c->near_dist2) {
                                face_skip = 2; /* connected beyond near: every 2nd */
                            }
                        }

                        int max_faces = room->mesh.face_count;
                        for (int f = 0; f < max_faces; f += face_skip) {
                            tr_face_t *face = &room->mesh.faces[f];
                            int nv = face->vertex_count;
                            if (nv < 3) continue;

                            uint16_t tex_idx = face->texture & 0x7FFF;
                            bool is_flat = (face->texture & 0x8000) != 0;
                            int want_tex;
                            if (is_flat) {
                                want_tex = -2;
                            } else if (tex_idx < num_otex && gl_textures) {
                                want_tex = (int)tex_idx;
                            } else continue;

                            if (want_tex != cur_tex) {
                                FLUSH_GL();
                                if (want_tex == -2 && gl_white_tex)
                                    glBindTexture(GL_TEXTURE_2D, gl_white_tex);
                                else if (want_tex >= 0)
                                    glBindTexture(GL_TEXTURE_2D, gl_textures[want_tex]);
                                cur_tex = want_tex;
                                perf_near_binds++;
                            }

                            tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                ? &g_level.object_textures[tex_idx] : NULL;

                            int num_tris = nv - 2;
                            if (gl_vc + num_tris * 3 > MAX_GL_VERTS) FLUSH_GL();

                            /* Build per-face vertex data */
                            float px[4], py[4], pz[4], fs[4], ft[4];
                            uint8_t cr[4], cg[4], cb[4];
                            bool ok = true;
                            for (int v = 0; v < nv; v++) {
                                uint16_t vi = face->vertices[v];
                                if (vi >= room->mesh.vertex_count) { ok = false; break; }
                                tr_room_vertex_t *rv = &room->mesh.vertices[vi];

                                float vx_w = (float)(room->x + rv->x);
                                float vy_w = (float)rv->y;
                                float vz_w = (float)(room->z + rv->z);
                                px[v] = (vx_w - cam_x) * WORLD_SCALE;
                                py[v] = -(vy_w - cam_y) * WORLD_SCALE;
                                pz[v] = -(vz_w - cam_z) * WORLD_SCALE;

                                float bright = 1.0f - ((float)rv->light / 8191.0f);
                                if (bright < 0.0f) bright = 0.0f;
                                if (bright > 1.0f) bright = 1.0f;
                                bright = 0.4f + bright * 0.6f;
                                float r = is_water ? bright * 0.4f : bright;
                                float g = is_water ? bright * 0.6f : bright;
                                float b = is_water ? bright * 1.0f : bright;

                                /* Apply fog for MID LOD rooms (GL near pass).
                                 * Uses per-room fog_start_dist2 so small rooms
                                 * fog sooner and large rooms fog later. */
                                bool apply_fog = (room_idx < g_chunk_count &&
                                    g_chunks[room_idx].lod >= CHUNK_LOD_MID);
                                if (apply_fog) {
                                    float fdx = vx_w - cam_x;
                                    float fdz = vz_w - cam_z;
                                    float d2 = fdx*fdx + fdz*fdz;
                                    float fog_start = g_chunks[room_idx].fog_start_dist2;
                                    float fog_range = g_chunks[room_idx].max_draw_dist2 - fog_start;
                                    if (fog_range < 1.0f) fog_range = 1.0f;
                                    float fog = (d2 - fog_start) / fog_range;
                                    /* Vertical bias: lower = more fog (TR1 Y-down) */
                                    float y_bias = (vy_w - cam_y) * 0.00004f;
                                    if (y_bias < 0.0f) y_bias = 0.0f;
                                    if (y_bias > 0.2f) y_bias = 0.2f;
                                    fog += y_bias;
                                    if (fog < 0.0f) fog = 0.0f;
                                    if (fog > 0.85f) fog = 0.85f; /* never fully opaque */
                                    /* Fog toward active room fog color */
                                    r = r * (1.0f - fog) + active_fog_r * fog;
                                    g = g * (1.0f - fog) + active_fog_g * fog;
                                    b = b * (1.0f - fog) + active_fog_b * fog;
                                }

                                cr[v] = (uint8_t)(r * 255.0f);
                                cg[v] = (uint8_t)(g * 255.0f);
                                cb[v] = (uint8_t)(b * 255.0f);

                                if (otex && tex_uv_map) {
                                    float tu = (float)(otex->uv[v].u >> 8);
                                    float tv = (float)(otex->uv[v].v >> 8);
                                    float s_px = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                    float t_px = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                    /* Normalize to 0-1, inset by half texel */
                                    float half_t = 0.5f / (float)TEX_TILE_DIM;
                                    float base_s = s_px / (float)TEX_TILE_DIM * (1.0f - 2.0f*half_t) + half_t;
                                    float base_t = t_px / (float)TEX_TILE_DIM * (1.0f - 2.0f*half_t) + half_t;

                                    fs[v] = base_s;
                                    ft[v] = base_t;
                                } else {
                                    fs[v] = 0; ft[v] = 0;
                                }
                            }
                            if (!ok) continue;

                            for (int tri = 0; tri < num_tris; tri++) {
                                int idx[3] = {0, tri+1, tri+2};
                                for (int k = 0; k < 3; k++) {
                                    int i = idx[k];
                                    gl_vert_t *vp = &gl_verts[gl_vc++];
                                    vp->x = px[i]; vp->y = py[i]; vp->z = pz[i];
                                    vp->r = cr[i]; vp->g = cg[i]; vp->b = cb[i]; vp->a = 255;
                                    vp->s = fs[i]; vp->t = ft[i];
                                }
                            }
                        }
                    }
                    FLUSH_GL();

                    /* ===== GL STATIC MESHES (near rooms only) =====
                     * Z-bias: scale static mesh positions slightly closer
                     * to camera (0.999×) to win Z-test against coplanar
                     * room walls.  glPolygonOffset not available in libdragon. */
                    #define SM_Z_BIAS 0.997f  /* stronger bias to reduce Z-fighting flicker */
                    for (int ri = 0; ri < near_count; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];

                        for (int si = 0; si < room->static_mesh_count; si++) {
                            tr_room_static_mesh_t *sm = &room->static_meshes[si];

                            /* Quick frustum cull */
                            float sdx = (float)sm->pos.x - cam_x;
                            float sdz = (float)sm->pos.z - cam_z;
                            float sm_rz = sdx * sy + sdz * cy;
                            if (sm_rz < -1024) continue;
                            float sm_rx = sdx * cy - sdz * sy;
                            /* Only apply horizontal frustum cull at distance —
                             * when close, the mesh is visible even if off-center */
                            if (sm_rz > 1024) { float l = sm_rz * 3.0f; if (sm_rx > l || sm_rx < -l) continue; }

                            /* Find static object definition */
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
                                tr_face_t *face = &mesh->faces[f];
                                int nv = face->vertex_count;
                                if (nv < 3) continue;

                                uint16_t tex_idx = face->texture & 0x7FFF;
                                bool is_flat = (face->texture & 0x8000) != 0;
                                int want_tex;
                                uint8_t sm_fr = (uint8_t)(sm_bright * 255.0f);
                                uint8_t sm_fg = sm_fr, sm_fb = sm_fr;
                                if (is_flat) {
                                    want_tex = -2;
                                    uint8_t ci = tex_idx & 0xFF;
                                    sm_fr = (uint8_t)((float)g_level.palette[ci*3+0] / 63.0f * sm_bright * 255.0f);
                                    sm_fg = (uint8_t)((float)g_level.palette[ci*3+1] / 63.0f * sm_bright * 255.0f);
                                    sm_fb = (uint8_t)((float)g_level.palette[ci*3+2] / 63.0f * sm_bright * 255.0f);
                                } else if (tex_idx < num_otex && gl_textures) {
                                    want_tex = (int)tex_idx;
                                } else continue;

                                if (want_tex != cur_tex) {
                                    FLUSH_GL();
                                    if (want_tex == -2 && gl_white_tex)
                                        glBindTexture(GL_TEXTURE_2D, gl_white_tex);
                                    else if (want_tex >= 0)
                                        glBindTexture(GL_TEXTURE_2D, gl_textures[want_tex]);
                                    cur_tex = want_tex;
                                }

                                tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                    ? &g_level.object_textures[tex_idx] : NULL;

                                int num_tris = nv - 2;
                                if (gl_vc + num_tris * 3 > MAX_GL_VERTS) FLUSH_GL();

                                float px[4], py[4], pz[4], fs[4], ft[4];
                                bool ok = true;
                                for (int v = 0; v < nv; v++) {
                                    uint16_t vi = face->vertices[v];
                                    if (vi >= mesh->vertex_count) { ok = false; break; }

                                    /* Local → world: rotate by yaw + translate */
                                    float lx = (float)mesh->vertices[vi].x;
                                    float lz = (float)mesh->vertices[vi].z;
                                    float wx = (float)sm->pos.x + lx * sm_cy_r - lz * sm_sy_r;
                                    float wy = (float)sm->pos.y + (float)mesh->vertices[vi].y;
                                    float wz = (float)sm->pos.z + lx * sm_sy_r + lz * sm_cy_r;

                                    /* Camera-relative (CPU subtract, RSP rotates).
                                     * SM_Z_BIAS pulls mesh slightly toward camera
                                     * to win Z-test against coplanar room walls. */
                                    px[v] = (wx - cam_x) * WORLD_SCALE * SM_Z_BIAS;
                                    py[v] = -(wy - cam_y) * WORLD_SCALE * SM_Z_BIAS;
                                    pz[v] = -(wz - cam_z) * WORLD_SCALE * SM_Z_BIAS;

                                    /* Palette color for flat, uniform shade for textured */

                                    if (otex && tex_uv_map) {
                                        float tu = (float)(otex->uv[v].u >> 8);
                                        float tv = (float)(otex->uv[v].v >> 8);
                                        float s_px = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                        float t_px = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                        float half_t = 0.5f / (float)TEX_TILE_DIM;
                                        fs[v] = s_px / (float)TEX_TILE_DIM * (1.0f - 2.0f*half_t) + half_t;
                                        ft[v] = t_px / (float)TEX_TILE_DIM * (1.0f - 2.0f*half_t) + half_t;
                                    } else {
                                        fs[v] = 0; ft[v] = 0;
                                    }
                                }
                                if (!ok) continue;

                                for (int tri = 0; tri < num_tris; tri++) {
                                    int idx[3] = {0, tri+1, tri+2};
                                    for (int k = 0; k < 3; k++) {
                                        int i = idx[k];
                                        gl_vert_t *vp = &gl_verts[gl_vc++];
                                        vp->x = px[i]; vp->y = py[i]; vp->z = pz[i];
                                        vp->r = sm_fr; vp->g = sm_fg; vp->b = sm_fb; vp->a = 255;
                                        vp->s = fs[i]; vp->t = ft[i];
                                    }
                                }
                            }
                        }
                    }
                    FLUSH_GL();

                    #undef SM_Z_BIAS

                    /* ===== GL LARA ===== */
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
                                    tr_face_t *face = &mesh->faces[f];
                                    int nv = face->vertex_count;
                                    if (nv < 3) continue;

                                    uint16_t tex_idx = face->texture & 0x7FFF;
                                    bool is_flat = (face->texture & 0x8000) != 0;
                                    int want_tex;
                                    /* Flat-colored face: look up palette color */
                                    uint8_t flat_r = lshade, flat_g = lshade, flat_b = lshade;
                                    if (is_flat) {
                                        want_tex = -2;
                                        uint8_t ci = tex_idx & 0xFF;
                                        flat_r = (uint8_t)((float)g_level.palette[ci*3+0] / 63.0f * lara_shade * 255.0f);
                                        flat_g = (uint8_t)((float)g_level.palette[ci*3+1] / 63.0f * lara_shade * 255.0f);
                                        flat_b = (uint8_t)((float)g_level.palette[ci*3+2] / 63.0f * lara_shade * 255.0f);
                                    } else if (tex_idx < num_otex && gl_textures) {
                                        want_tex = (int)tex_idx;
                                    } else continue;

                                    if (want_tex != cur_tex) {
                                        FLUSH_GL();
                                        if (want_tex == -2 && gl_white_tex)
                                            glBindTexture(GL_TEXTURE_2D, gl_white_tex);
                                        else if (want_tex >= 0)
                                            glBindTexture(GL_TEXTURE_2D, gl_textures[want_tex]);
                                        cur_tex = want_tex;
                                    }

                                    tr_object_texture_t *otex = (!is_flat && tex_idx < g_level.object_texture_count)
                                        ? &g_level.object_textures[tex_idx] : NULL;

                                    int num_tris = nv - 2;
                                    if (gl_vc + num_tris * 3 > MAX_GL_VERTS) FLUSH_GL();

                                    float px[4], py[4], pz[4], ffs[4], fft[4];
                                    bool ok = true;
                                    for (int v = 0; v < nv; v++) {
                                        uint16_t vi = face->vertices[v];
                                        if (vi >= mesh->vertex_count) { ok = false; break; }
                                        float vx = (float)mesh->vertices[vi].x;
                                        float vz = (float)mesh->vertices[vi].z;
                                        float wx = bx + vx*lcy_r - vz*lsy_r;
                                        float wy = by + (float)mesh->vertices[vi].y;
                                        float wz = bz + vx*lsy_r + vz*lcy_r;

                                        px[v] = (wx - cam_x) * WORLD_SCALE;
                                        py[v] = -(wy - cam_y) * WORLD_SCALE;
                                        pz[v] = -(wz - cam_z) * WORLD_SCALE;

                                        if (otex && tex_uv_map) {
                                            float tu = (float)(otex->uv[v].u >> 8);
                                            float tv = (float)(otex->uv[v].v >> 8);
                                            float s_px = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                            float t_px = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                            float half_t = 0.5f / (float)TEX_TILE_DIM;
                                            ffs[v] = s_px / (float)TEX_TILE_DIM * (1.0f - 2.0f*half_t) + half_t;
                                            fft[v] = t_px / (float)TEX_TILE_DIM * (1.0f - 2.0f*half_t) + half_t;
                                        } else {
                                            ffs[v] = 0; fft[v] = 0;
                                        }
                                    }
                                    if (!ok) continue;

                                    for (int tri = 0; tri < num_tris; tri++) {
                                        int idx[3] = {0, tri+1, tri+2};
                                        for (int k = 0; k < 3; k++) {
                                            int i = idx[k];
                                            gl_vert_t *vp = &gl_verts[gl_vc++];
                                            vp->x = px[i]; vp->y = py[i]; vp->z = pz[i];
                                            vp->r = is_flat ? flat_r : lshade;
                                            vp->g = is_flat ? flat_g : lshade;
                                            vp->b = is_flat ? flat_b : lshade;
                                            vp->a = 255;
                                            vp->s = ffs[i]; vp->t = fft[i];
                                        }
                                    }
                                }
                            }
                        }
                    }
                    FLUSH_GL();

                    /* ===== LARA SHADOW (dark circle on floor) ===== */
                    {
                        /* Use Lara's item position directly */
                        tr_item_t *shadow_item = NULL;
                        for (int i = 0; i < g_level.item_count; i++)
                            if (g_level.items[i].object_id == 0) { shadow_item = &g_level.items[i]; break; }

                        if (shadow_item) {
                            int32_t shadow_fl, shadow_cl;
                            GET_SECTOR_HEIGHTS(shadow_item->pos.x, shadow_item->pos.z,
                                               shadow_item->pos.y, shadow_fl, shadow_cl);
                            float shadow_y = (float)shadow_fl + 4.0f;
                            float shadow_rad = 120.0f;
                            float shadow_cx_w = (float)shadow_item->pos.x;
                            float shadow_cz_w = (float)shadow_item->pos.z;

                            if (gl_white_tex) {
                                FLUSH_GL();
                                glBindTexture(GL_TEXTURE_2D, gl_white_tex);
                                cur_tex = -2;
                            }

                            glEnable(GL_BLEND);
                            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                            glDepthMask(GL_FALSE);

                            float sc_x = (shadow_cx_w - cam_x) * WORLD_SCALE;
                            float sc_y = -(shadow_y - cam_y) * WORLD_SCALE;
                            float sc_z = -(shadow_cz_w - cam_z) * WORLD_SCALE;

                            #define SHADOW_PTS 12
                            float rim_x[SHADOW_PTS], rim_z2[SHADOW_PTS];
                            for (int sp3 = 0; sp3 < SHADOW_PTS; sp3++) {
                                float a = (float)sp3 * 6.28318f / (float)SHADOW_PTS;
                                float wx = shadow_cx_w + shadow_rad * sinf(a);
                                float wz = shadow_cz_w + shadow_rad * cosf(a);
                                rim_x[sp3] = (wx - cam_x) * WORLD_SCALE;
                                rim_z2[sp3] = -(wz - cam_z) * WORLD_SCALE;
                            }

                            for (int sp3 = 0; sp3 < SHADOW_PTS; sp3++) {
                                int next = (sp3 + 1) % SHADOW_PTS;
                                if (gl_vc + 3 > MAX_GL_VERTS) FLUSH_GL();
                                gl_vert_t *v0 = &gl_verts[gl_vc++];
                                v0->x = sc_x; v0->y = sc_y; v0->z = sc_z;
                                v0->r = 0; v0->g = 0; v0->b = 0; v0->a = 160;
                                v0->s = 0; v0->t = 0;
                                gl_vert_t *v1 = &gl_verts[gl_vc++];
                                v1->x = rim_x[sp3]; v1->y = sc_y; v1->z = rim_z2[sp3];
                                v1->r = 0; v1->g = 0; v1->b = 0; v1->a = 80;
                                v1->s = 0; v1->t = 0;
                                gl_vert_t *v2 = &gl_verts[gl_vc++];
                                v2->x = rim_x[next]; v2->y = sc_y; v2->z = rim_z2[next];
                                v2->r = 0; v2->g = 0; v2->b = 0; v2->a = 80;
                                v2->s = 0; v2->t = 0;
                            }
                            FLUSH_GL();

                            glDisable(GL_BLEND);
                            glDepthMask(GL_TRUE);
                            #undef SHADOW_PTS
                        }
                    }

                    } /* end GL near pass */

                    #undef FLUSH_GL

#if 0 /* TODO: Convert these to GL (phase 2) — rdpq conflicts with GL context */
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

                            if (white_px && last_tex_idx != -2) {
                                rdpq_tex_upload(TILE0, &white_surf, NULL);
                                last_tex_idx = -2;
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
                                sv[v][7]=0; sv[v][8]=0; sv[v][9]=1.0f/rz_s;
                            }
                            rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                            rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                        }
                    }

                    /* ===== DRAW STATIC OBJECTS ===== */
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
                                    if (white_px && last_tex_idx != -2) {
                                        rdpq_tex_upload(TILE0, &white_surf, NULL);
                                        last_tex_idx = -2;
                                    }
                                } else if (tex_idx < num_otex) {
                                    otex = &g_level.object_textures[tex_idx];
                                    if ((int)tex_idx != last_tex_idx) {
                                        rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                        last_tex_idx = (int)tex_idx;
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

                    /* ===== DRAW PLACED ITEMS (pickups, enemies, doors) ===== */
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
                                if (white_px && last_tex_idx != -2) { rdpq_tex_upload(TILE0, &white_surf, NULL); last_tex_idx = -2; }
                            } else if (tex_idx < num_otex) {
                                otex = &g_level.object_textures[tex_idx];
                                if ((int)tex_idx != last_tex_idx) { rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL); last_tex_idx = (int)tex_idx; }
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
                            rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                            if (face->vertex_count == 4)
                                rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                        }
                    }

                    /* ===== DRAW LARA ===== */
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
                                        if (white_px && last_tex_idx != -2) {
                                            rdpq_tex_upload(TILE0, &white_surf, NULL);
                                            last_tex_idx = -2;
                                        }
                                    } else if (tex_idx < num_otex) {
                                        otex = &g_level.object_textures[tex_idx];
                                        if ((int)tex_idx != last_tex_idx) {
                                            rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                            last_tex_idx = (int)tex_idx;
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

                    /* ===== SHADOW CIRCLE under Lara ===== */
                    {
                        tr_item_t *shadow_item = NULL;
                        for (int i = 0; i < g_level.item_count; i++)
                            if (g_level.items[i].object_id == 0) { shadow_item = &g_level.items[i]; break; }
                    if (shadow_item) {
                        float sx_pos = (float)shadow_item->pos.x;
                        float sy_pos = (float)shadow_item->pos.y + 10.0f;
                        float sz_pos = (float)shadow_item->pos.z;

                        if (white_px && last_tex_idx != -2) {
                            rdpq_tex_upload(TILE0, &white_surf, NULL);
                            last_tex_idx = -2;
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
#endif /* phase 2 rdpq renderers */

                    /* FPS display disabled during GL migration */

                    /* Show frame (no glFinish — upload/tri caps prevent
                     * queue backlog without blocking the CPU). */
                    gl_context_end();
                    rdpq_detach_show();

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

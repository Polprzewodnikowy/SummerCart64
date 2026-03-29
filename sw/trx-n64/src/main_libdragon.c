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

/* -------------------------------------------------------------------
 * Bump allocator
 * ------------------------------------------------------------------- */

#define GAME_HEAP_SIZE  (4 * 1024 * 1024)
static uint8_t game_heap[GAME_HEAP_SIZE] __attribute__((aligned(16)));
static uint32_t heap_offset = 0;

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

#define ARCHIVE_SECTOR  0x10000
#define CACHE_BASE      ((uint8_t *)0xB1000000)
#define CACHE_SIZE      (3 * 1024 * 1024)
#define SLOT_SIZE       (512 * 1024)
static uint8_t staging[512 * 1024] __attribute__((aligned(16)));

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
         * Always read full uncompressed size — the asset packer
         * stores uncompressed .PHD data despite showing compression stats. */
        uint32_t read_size = level_size;
        uint32_t read_bytes = (read_size + 511) & ~511;

        /* Align read size to 16 bytes for DMA */
        read_bytes = (read_bytes + 15) & ~15;

        n64_mem_init();
        uint8_t *level_data = (uint8_t *)n64_mem_alloc(read_bytes + 4096);

        printf("  Alloc %u KB... ", (read_bytes + 4096) / 1024);
        if (!level_data) {
            printf("FAILED (heap full)\n");
        } else if (level_sector == 0) {
            printf("asset not found in index\n");
        }

        if (level_data && level_sector > 0) {
            uint32_t cart_addr = ASSET_CART_ADDR + level_sector * 512;
            /* Use libdragon's dma_read which handles alignment */
            dma_read(level_data, cart_addr, read_bytes);

            printf("  Loaded %u KB from ROM.\n", read_bytes / 1024);
            printf("  First bytes: %02x %02x %02x %02x\n",
                   level_data[0], level_data[1], level_data[2], level_data[3]);

            /* Parse the .PHD level.
             * DO NOT reset the heap here! level_data is at heap offset 0.
             * The parser allocates from the heap too — if we reset, its
             * allocations overwrite level_data (including texture pages
             * which are at the BEGINNING of .PHD files). */
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
                printf("  Heap:     %u KB used\n\n",
                       (GAME_HEAP_SIZE - n64_mem_available()) / 1024);

                printf("LEVEL LOADED! Preparing textures...\n");
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

                /* Create a 1x1 solid white texture for flat-colored faces.
                 * When uploaded, TEX0 = white, so TEX0 * SHADE = SHADE. */
                uint16_t *white_px = (uint16_t *)malloc_uncached_aligned(64, 8);
                surface_t white_surf = {0};
                if (white_px) {
                    white_px[0] = 0xFFFF; /* RGBA16 white, full alpha */
                    white_surf = surface_make_linear(white_px, FMT_RGBA16, 1, 1);
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

                /* Close console, init display for 3D */
                console_close();
                display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
                rdpq_init();

                /* Z-buffer for proper depth sorting */
                surface_t zbuf = surface_alloc(FMT_RGBA16, 320, 240);
                bool has_zbuf = (zbuf.buffer != NULL);

                /* Camera state */
                float cam_x = 0, cam_y = 0, cam_z = 0;
                float cam_rot_y = 0, cam_rot_x = 0;  /* yaw + pitch */

                /* Find Lara's start position */
                for (int i = 0; i < g_level.item_count; i++) {
                    if (g_level.items[i].object_id == 0) {
                        cam_rot_y = (float)g_level.items[i].rotation * 3.14159f / 32768.0f;
                        /* Start camera slightly behind Lara */
                        cam_x = (float)g_level.items[i].pos.x - 600.0f * sinf(cam_rot_y);
                        cam_y = (float)g_level.items[i].pos.y - 768;
                        cam_z = (float)g_level.items[i].pos.z - 600.0f * cosf(cam_rot_y);
                        break;
                    }
                }

                /* Use proper sin/cos (Taylor approximations break at large angles) */
                #define WRAP_ANGLE(a) ((a) > 3.14159f ? (a) - 6.28318f : ((a) < -3.14159f ? (a) + 6.28318f : (a)))

                /* FPS counter */
                int fps_frames = 0;
                uint32_t fps_last = TICKS_READ();
                int fps_display = 0;

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
                    static bool walk_mode = false;
                    static int speed_level = 1;
                    if (pressed.r) { speed_level++; if (speed_level > 3) speed_level = 0; }
                    if (pressed.l) walk_mode = !walk_mode;
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

                    /* C buttons: move forward/back + strafe */
                    if (input.btn.c_up) { cam_x += move_speed * sy; cam_z += move_speed * cy; }
                    if (input.btn.c_down) { cam_x -= move_speed * sy; cam_z -= move_speed * cy; }
                    if (input.btn.c_left) { cam_x -= move_speed * cy; cam_z += move_speed * sy; }
                    if (input.btn.c_right) { cam_x += move_speed * cy; cam_z -= move_speed * sy; }

                    /* D-pad: up/down = fly, left/right = strafe */
                    if (input.btn.d_up) cam_y -= move_speed;
                    if (input.btn.d_down) cam_y += move_speed;
                    if (input.btn.d_left) { cam_x -= move_speed * cy; cam_z += move_speed * sy; }
                    if (input.btn.d_right) { cam_x += move_speed * cy; cam_z -= move_speed * sy; }

                    /* ===== ROOM TRACKING + FLOOR COLLISION =====
                     * Always track which room the camera is in (for BFS).
                     * A button toggles walk mode (floor clamping). */
                    if (pressed.a) walk_mode = !walk_mode;

                    {
                        /* Find which room the camera is in */
                        int cam_room = lara_room;
                        int32_t icx = (int32_t)cam_x;
                        int32_t icz = (int32_t)cam_z;

                        /* Check current room, then portals, then 2nd-level portals */
                        tr_room_t *cur = &g_level.rooms[lara_room];
                        int32_t rx = icx - cur->x;
                        int32_t rz = icz - cur->z;
                        bool found = (rx >= 0 && rx < cur->sectors_x * 1024 &&
                                      rz >= 0 && rz < cur->sectors_z * 1024);
                        if (!found) {
                            for (int p = 0; p < cur->portal_count && !found; p++) {
                                int d1 = cur->portals[p].room;
                                if (d1 < 0 || d1 >= g_level.room_count) continue;
                                tr_room_t *rm = &g_level.rooms[d1];
                                rx = icx - rm->x; rz = icz - rm->z;
                                if (rx >= 0 && rx < rm->sectors_x * 1024 &&
                                    rz >= 0 && rz < rm->sectors_z * 1024) {
                                    cam_room = d1; found = true; break;
                                }
                                /* Check 2nd-level portals */
                                for (int q = 0; q < rm->portal_count && !found; q++) {
                                    int d2 = rm->portals[q].room;
                                    if (d2 < 0 || d2 >= g_level.room_count) continue;
                                    tr_room_t *rm2 = &g_level.rooms[d2];
                                    rx = icx - rm2->x; rz = icz - rm2->z;
                                    if (rx >= 0 && rx < rm2->sectors_x * 1024 &&
                                        rz >= 0 && rz < rm2->sectors_z * 1024) {
                                        cam_room = d2; found = true;
                                    }
                                }
                            }
                        }

                        /* Always update current room for BFS traversal */
                        lara_room = cam_room;

                        /* Floor clamping (walk mode only) */
                        if (walk_mode) {
                            tr_room_t *room = &g_level.rooms[cam_room];
                            int32_t sx = (icx - room->x) >> 10;
                            int32_t sz = (icz - room->z) >> 10;
                            if (sx < 0) sx = 0;
                            if (sx >= room->sectors_x) sx = room->sectors_x - 1;
                            if (sz < 0) sz = 0;
                            if (sz >= room->sectors_z) sz = room->sectors_z - 1;

                            if (room->sectors) {
                                tr_sector_t *sector = &room->sectors[sz * room->sectors_x + sx];
                                int32_t floor_y = sector->floor * -256;

                                if (sector->pit_room != 255 && sector->pit_room < g_level.room_count) {
                                    tr_room_t *below = &g_level.rooms[sector->pit_room];
                                    int32_t bx = (icx - below->x) >> 10;
                                    int32_t bz = (icz - below->z) >> 10;
                                    if (bx >= 0 && bx < below->sectors_x &&
                                        bz >= 0 && bz < below->sectors_z && below->sectors) {
                                        tr_sector_t *bs = &below->sectors[bz * below->sectors_x + bx];
                                        int32_t bf = bs->floor * -256;
                                        if (bf > floor_y) floor_y = bf;
                                    }
                                }

                                /* Gravity: always walk on floor */
                                cam_y = (float)floor_y - 768.0f;
                            }
                        }
                    }

                    /* Start button: return to console */
                    if (pressed.start) break;

                    /* Begin frame */
                    surface_t *disp = display_get();
                    rdpq_attach_clear(disp, has_zbuf ? &zbuf : NULL);

                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, SHADE, 0), (TEX0, 0, SHADE, 0)));
                    rdpq_mode_persp(true);
                    if (has_zbuf) rdpq_mode_zbuf(true, true);
                    rdpq_mode_filter(FILTER_BILINEAR);
                    rdpq_mode_antialias(AA_STANDARD);

                    /* Precompute pitch rotation */
                    float sp = sinf(cam_rot_x);
                    float cp = cosf(cam_rot_x);

                    /* Track current texture for batching */
                    int last_tex_idx = -1;

                    /* Build visible room list via portal traversal.
                     * Start from Lara's room, follow portals up to depth 4. */
                    int rooms_to_draw[64];
                    int num_draw = 0;
                    bool room_drawn[256];
                    memset(room_drawn, 0, sizeof(room_drawn));

                    /* BFS portal traversal — limit depth for performance */
                    rooms_to_draw[num_draw++] = lara_room;
                    room_drawn[lara_room] = true;
                    int queue_head = 0;
                    int max_rooms = 5;  /* Texture sort keeps uploads low */
                    while (queue_head < num_draw && num_draw < max_rooms) {
                        int ri = rooms_to_draw[queue_head++];
                        if (ri < 0 || ri >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[ri];
                        for (int p = 0; p < room->portal_count && num_draw < max_rooms; p++) {
                            int dest = room->portals[p].room;
                            if (dest >= 0 && dest < g_level.room_count && !room_drawn[dest]) {
                                room_drawn[dest] = true;
                                rooms_to_draw[num_draw++] = dest;
                            }
                        }
                    }

                    /* Draw rooms with frustum culling */
                    for (int ri = 0; ri < num_draw; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];
                        bool is_water = (room->flags & 1) != 0;

                        /* Room-level frustum cull: check if room center is visible */
                        float rcx = room->x + room->sectors_x * 512.0f - cam_x;
                        float rcz = room->z + room->sectors_z * 512.0f - cam_z;
                        float room_rz = rcx * sy + rcz * cy; /* camera-space Z */
                        float room_rx = rcx * cy - rcz * sy; /* camera-space X */
                        float room_radius = (room->sectors_x + room->sectors_z) * 512.0f;
                        /* Skip if room is entirely behind camera */
                        if (room_rz < -room_radius) continue;
                        /* Skip if room is entirely off to the side */
                        if (room_rz > 0) {
                            float lim = room_rz * 2.5f + room_radius;
                            if (room_rx > lim || room_rx < -lim) continue;
                        }

                        int max_faces = room->mesh.face_count;
                        if (max_faces > 500) max_faces = 500;
                        for (int f = 0; f < max_faces; f++) {
                            tr_face_t *face = &room->mesh.faces[f];
                            if (face->vertex_count < 3) continue;

                            /* Upload texture if available */
                            uint16_t tex_idx = face->texture & 0x7FFF;
                            bool has_tex = false;
                            tr_object_texture_t *otex = NULL;
                            if (tex_idx < num_otex && tex_surfs &&
                                tex_idx < g_level.object_texture_count) {
                                otex = &g_level.object_textures[tex_idx];
                                has_tex = true;
                                if ((int)tex_idx != last_tex_idx) {
                                    rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                    last_tex_idx = (int)tex_idx;
                                }
                            }
                            /* Skip faces with no valid texture (flat-colored faces
                             * or textures beyond our cap) — they'd render with
                             * wrong texture data. TODO: render as solid color. */
                            if (!has_tex) continue;

                            /* Quick distance cull with first vertex (squared, no sqrt) */
                            {
                                uint16_t v0i = face->vertices[0];
                                if (v0i < room->mesh.vertex_count) {
                                    float dx = room->x + room->mesh.vertices[v0i].x - cam_x;
                                    float dz = room->z + room->mesh.vertices[v0i].z - cam_z;
                                    if (dx*dx + dz*dz > 200000000.0f) continue; /* ~14K units */
                                }
                            }

                            float sv[4][10];
                            bool visible = true;

                            for (int v = 0; v < face->vertex_count; v++) {
                                uint16_t vi = face->vertices[v];
                                if (vi >= room->mesh.vertex_count) { visible = false; break; }

                                tr_room_vertex_t *rv = &room->mesh.vertices[vi];

                                float wx = room->x + rv->x - cam_x;
                                float wy = rv->y - cam_y;
                                float wz = room->z + rv->z - cam_z;

                                /* Yaw rotation (around Y) */
                                float rx = wx * cy - wz * sy;
                                float rz_yaw = wx * sy + wz * cy;

                                /* Pitch rotation (around X) */
                                float ry = wy * cp - rz_yaw * sp;
                                float rz = wy * sp + rz_yaw * cp;

                                if (rz < 1.0f) { visible = false; break; }

                                float inv_z = 256.0f / rz;
                                float screen_x = 160.0f + rx * inv_z;
                                float screen_y = 120.0f + ry * inv_z;

                                if (screen_x < -1024 || screen_x > 1344 ||
                                    screen_y < -1024 || screen_y > 1264)
                                    { visible = false; break; }

                                sv[v][0] = screen_x;
                                sv[v][1] = screen_y;
                                /* Z normalized to [0,1] for zbuf (0=near, 1=far) */
                                sv[v][2] = rz / 32768.0f;
                                if (sv[v][2] > 1.0f) sv[v][2] = 1.0f;

                                /* Distance fog: darken with distance */
                                float fog = 1.0f - rz / 16384.0f;
                                if (fog < 0.0f) fog = 0.0f;

                                int16_t light = rv->light;
                                float bright = 1.0f - ((float)light / 8191.0f);
                                if (bright < 0.0f) bright = 0.0f;
                                if (bright > 1.0f) bright = 1.0f;
                                bright = (0.4f + bright * 0.6f) * fog;
                                /* Water rooms get blue tint */
                                sv[v][3] = is_water ? bright * 0.4f : bright;
                                sv[v][4] = is_water ? bright * 0.6f : bright;
                                sv[v][5] = is_water ? bright * 1.0f : bright;
                                sv[v][6] = 1.0f;

                                if (has_tex && tex_uv_map) {
                                    float tu = (float)(otex->uv[v].u >> 8);
                                    float tv = (float)(otex->uv[v].v >> 8);
                                    sv[v][7] = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                    sv[v][8] = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                } else {
                                    sv[v][7] = 0.0f;
                                    sv[v][8] = 0.0f;
                                }
                                sv[v][9] = 1.0f / rz;
                            }

                            if (!visible) continue;

                            /* Backface cull disabled for debugging */
                            /* float cross = (sv[1][0]-sv[0][0])*(sv[2][1]-sv[0][1])
                                        - (sv[1][1]-sv[0][1])*(sv[2][0]-sv[0][0]);
                            if (cross < 0 && !(face->texture & 0x8000)) continue; */

                            rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                            if (face->vertex_count == 4)
                                rdpq_triangle(has_zbuf ? &TRIFMT_ZBUF_SHADE_TEX : &TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                        }

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
                                    if (rz < 1.0f) { visible = false; break; }

                                    float inv_z = 256.0f / rz;
                                    float fog = 1.0f - rz / 16384.0f;
                                    if (fog < 0) fog = 0;
                                    sv[v][0] = 160.0f + rx * inv_z;
                                    sv[v][1] = 120.0f + ry * inv_z;
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
                                if (rz < 1.0f) { vis = false; break; }
                                float inv_z = 256.0f / rz;
                                float fog = 1.0f - rz/16384.0f; if (fog < 0) fog = 0;
                                sv[v][0] = 160.0f + rx*inv_z; sv[v][1] = 120.0f + ry*inv_z;
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
                                        if (rz < 1.0f) { visible = false; break; }

                                        float inv_z = 256.0f / rz;
                                        sv[v][0] = 160.0f + rx * inv_z;
                                        sv[v][1] = 120.0f + ry * inv_z;
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
                            if (rz < 1.0f) { vis = false; break; }
                            float inv_z=256.0f/rz;
                            sv_s[v][0]=160.0f+rx*inv_z;
                            sv_s[v][1]=120.0f+ry*inv_z;
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

                    /* FPS display: draw as colored rectangles for digits */
                    {
                        /* Simple digit renderer using fill rectangles */
                        rdpq_set_mode_fill(RGBA32(0xFF, 0xFF, 0x00, 0xFF));
                        int tens = fps_display / 10;
                        int ones = fps_display % 10;
                        /* Draw bar-graph style: each unit = 3px wide block */
                        for (int d = 0; d < fps_display && d < 30; d++)
                            rdpq_fill_rectangle(4 + d * 4, 4, 4 + d * 4 + 3, 8);
                        /* Restore render mode for next frame */
                    }
                    (void)0; /* Suppress empty statement warning */

                    /* Show frame */
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

                /* START pressed — return to console briefly */
                rdpq_close();
                display_close();
                console_init();
                console_set_render_mode(RENDER_MANUAL);
                printf("Paused. Press START for 3D.\n");
                printf("Camera: %.0f, %.0f, %.0f\n", cam_x, cam_y, cam_z);
                printf("FPS: %d  Audio: %d samples\n", fps_display, parsed_samples);
                /* Debug: Lara animation info */
                {
                    tr_object_def_t *dbg_obj = NULL;
                    for (int i = 0; i < g_level.object_count; i++)
                        if (g_level.objects[i].object_id == 0) { dbg_obj = &g_level.objects[i]; break; }
                    if (dbg_obj) {
                        int ai = dbg_obj->anim_idx;
                        printf("Lara: meshes=%d anim_idx=%d\n", dbg_obj->mesh_count, ai);
                        printf("  frame_off=%u bone_idx=%d\n", dbg_obj->frame_offset, dbg_obj->bone_idx);
                        if (ai >= 0 && ai < g_level.anim_count) {
                            tr_anim_t *a = &g_level.anims[ai];
                            printf("  anim: frames=%d-%d size=%d interp=%d\n",
                                   a->frame_base, a->frame_end, a->frame_size, a->interpolation);
                            /* Show first frame rotation data for mesh 0 */
                            int fo = dbg_obj->frame_offset / 2;
                            if (g_level.anim_frames && fo + 11 < g_level.anim_frame_count) {
                                int16_t *f = &g_level.anim_frames[fo];
                                printf("  fr0: off=%d,%d,%d rot0=%04x,%04x\n",
                                       f[6], f[7], f[8], (uint16_t)f[9], (uint16_t)f[10]);
                            }
                        }
                    }
                }
                console_render();

                /* Wait for START to re-enter 3D */
                while (1) {
                    joypad_poll();
                    joypad_buttons_t p2 = joypad_get_buttons_pressed(JOYPAD_PORT_1);
                    if (p2.start) break;
                }

                /* Re-init display and rdpq for 3D */
                console_close();
                display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
                rdpq_init();
                goto render_loop;

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

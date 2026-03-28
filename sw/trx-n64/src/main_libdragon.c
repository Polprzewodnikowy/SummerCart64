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

        /* Read level data from ROM via PI DMA */
        uint32_t read_size = level_compressed ? level_compressed : level_size;
        if (read_size == 0) read_size = level_size;
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
                if (num_otex > 512) num_otex = 512;  /* 512 × 2KB = 1MB uncached */

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
                printf("  %d textures ready (%d KB). Heap: %u KB left.\n",
                       num_otex, num_otex * TEX_TILE_BYTES / 1024,
                       n64_mem_available() / 1024);

                /* ============================================
                 * 3D RENDERING LOOP
                 * ============================================
                 * Switch from console text to RDP graphics.
                 * Free-fly camera with analog stick.
                 * Flat-shaded room triangles via rdpq.
                 */

                /* Close console, init display for 3D */
                console_close();
                display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
                rdpq_init();

                /* No Z-buffer for now — back-to-front room order handles depth */

                /* Camera state */
                float cam_x = 0, cam_y = 0, cam_z = 0;
                float cam_rot_y = 0;

                /* Find Lara's start position */
                for (int i = 0; i < g_level.item_count; i++) {
                    if (g_level.items[i].object_id == 0) {
                        cam_x = (float)g_level.items[i].pos.x;
                        cam_y = (float)g_level.items[i].pos.y - 512;
                        cam_z = (float)g_level.items[i].pos.z;
                        cam_rot_y = (float)g_level.items[i].rotation * 3.14159f / 32768.0f;
                        break;
                    }
                }

                /* Use proper sin/cos (Taylor approximations break at large angles) */
                #define WRAP_ANGLE(a) ((a) > 3.14159f ? (a) - 6.28318f : ((a) < -3.14159f ? (a) + 6.28318f : (a)))

                /* Render loop */
                render_loop:
                while (1) {
                    joypad_poll();
                    joypad_inputs_t input = joypad_get_inputs(JOYPAD_PORT_1);
                    joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

                    /* Camera movement — L/R bumpers cycle speed levels */
                    static int speed_level = 1;  /* 0=slow, 1=normal, 2=fast, 3=turbo */
                    if (pressed.l) { speed_level--; if (speed_level < 0) speed_level = 3; }
                    if (pressed.r) { speed_level++; if (speed_level > 3) speed_level = 0; }
                    float speed_mult[] = { 8.0f, 32.0f, 128.0f, 512.0f };
                    float move_speed = speed_mult[speed_level];
                    float turn_speed = 0.12f;

                    /* Z button = temporary boost (hold for turbo) */
                    if (input.btn.z) move_speed *= 4.0f;

                    /* Joystick: look around (X = turn, Y = move forward/back) */
                    if (input.stick_x > 20 || input.stick_x < -20)
                        cam_rot_y += input.stick_x * turn_speed * 0.01f;
                    cam_rot_y = WRAP_ANGLE(cam_rot_y);

                    float sy = sinf(cam_rot_y);
                    float cy = cosf(cam_rot_y);

                    /* Joystick Y: move forward/back */
                    if (input.stick_y > 20) { cam_x += move_speed * sy; cam_z += move_speed * cy; }
                    if (input.stick_y < -20) { cam_x -= move_speed * sy; cam_z -= move_speed * cy; }

                    /* D-pad: strafe left/right, fly up/down */
                    if (input.btn.d_left) { cam_x -= move_speed * cy; cam_z += move_speed * sy; }
                    if (input.btn.d_right) { cam_x += move_speed * cy; cam_z -= move_speed * sy; }
                    if (input.btn.d_up) cam_y -= move_speed;
                    if (input.btn.d_down) cam_y += move_speed;

                    /* C buttons: move forward/back + strafe left/right */
                    if (input.btn.c_up) { cam_x += move_speed * sy; cam_z += move_speed * cy; }
                    if (input.btn.c_down) { cam_x -= move_speed * sy; cam_z -= move_speed * cy; }
                    if (input.btn.c_left) { cam_x -= move_speed * cy; cam_z += move_speed * sy; }
                    if (input.btn.c_right) { cam_x += move_speed * cy; cam_z -= move_speed * sy; }

                    /* ===== ROOM TRACKING + FLOOR COLLISION =====
                     * Always track which room the camera is in (for BFS).
                     * A button toggles walk mode (floor clamping). */
                    static bool walk_mode = false;
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

                                float floor_limit = (float)floor_y - 768.0f;
                                if (cam_y > floor_limit) cam_y = floor_limit;
                            }
                        }
                    }

                    /* Start button: return to console */
                    if (pressed.start) break;

                    /* Begin frame */
                    surface_t *disp = display_get();
                    rdpq_attach_clear(disp, NULL);

                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, SHADE, 0), (TEX0, 0, SHADE, 0)));
                    rdpq_mode_persp(true);
                    rdpq_mode_filter(FILTER_POINT);

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
                    int max_rooms = 5;  /* Limit visible rooms for FPS */
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

                    /* Draw far rooms first (reverse BFS order) for
                     * rough back-to-front sorting without Z-buffer */
                    for (int ri = num_draw - 1; ri >= 0; ri--) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];

                        int max_faces = room->mesh.face_count;
                        if (max_faces > 300) max_faces = 300;  /* Cap per room */
                        for (int f = 0; f < max_faces; f++) {
                            tr_face_t *face = &room->mesh.faces[f];
                            if (face->vertex_count < 3) continue;

                            /* Upload pre-converted RGBA16 texture tile */
                            uint16_t tex_idx = face->texture & 0x7FFF;
                            bool has_tex = false;
                            tr_object_texture_t *otex = NULL;
                            if (tex_idx < num_otex && tex_surfs) {
                                otex = &g_level.object_textures[tex_idx];
                                has_tex = true;
                                if ((int)tex_idx != last_tex_idx) {
                                    rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                    last_tex_idx = (int)tex_idx;
                                }
                            }

                            /* Transform vertices: {X, Y, R, G, B, A, S, T, INV_W} */
                            float sv[4][9];
                            bool visible = true;

                            for (int v = 0; v < face->vertex_count; v++) {
                                uint16_t vi = face->vertices[v];
                                if (vi >= room->mesh.vertex_count) { visible = false; break; }

                                tr_room_vertex_t *rv = &room->mesh.vertices[vi];

                                float wx = room->x + rv->x - cam_x;
                                float wy = rv->y - cam_y;
                                float wz = room->z + rv->z - cam_z;

                                float rx = wx * cy - wz * sy;
                                float rz = wx * sy + wz * cy;

                                if (rz < 1.0f) { visible = false; break; }

                                float inv_z = 256.0f / rz;
                                float screen_x = 160.0f + rx * inv_z;
                                float screen_y = 120.0f + wy * inv_z;

                                /* Only cull if vertex is WAY off-screen.
                                 * RDP handles clipping for partially visible faces.
                                 * RDP uses s12.2 coords, so stay within ~1024. */
                                if (screen_x < -1024 || screen_x > 1344 ||
                                    screen_y < -1024 || screen_y > 1264)
                                    { visible = false; break; }

                                sv[v][0] = screen_x;
                                sv[v][1] = screen_y;

                                /* Per-vertex lighting (TR1: 0=bright, 8191=dark) */
                                int16_t light = rv->light;
                                float bright = 1.0f - ((float)light / 8191.0f);
                                if (bright < 0.0f) bright = 0.0f;
                                if (bright > 1.0f) bright = 1.0f;
                                bright = 0.4f + bright * 0.6f;
                                sv[v][2] = bright;
                                sv[v][3] = bright;
                                sv[v][4] = bright;
                                sv[v][5] = 1.0f;

                                /* Texture coords mapped to tile space */
                                if (has_tex && tex_uv_map) {
                                    float tu = (float)(otex->uv[v].u >> 8);
                                    float tv = (float)(otex->uv[v].v >> 8);
                                    sv[v][6] = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                    sv[v][7] = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                } else {
                                    sv[v][6] = 0.0f;
                                    sv[v][7] = 0.0f;
                                }
                                sv[v][8] = 1.0f / rz;
                            }

                            if (!visible) continue;

                            /* Backface cull disabled for debugging */
                            /* float cross = (sv[1][0]-sv[0][0])*(sv[2][1]-sv[0][1])
                                        - (sv[1][1]-sv[0][1])*(sv[2][0]-sv[0][0]);
                            if (cross < 0 && !(face->texture & 0x8000)) continue; */

                            rdpq_triangle(&TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                            if (face->vertex_count == 4)
                                rdpq_triangle(&TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                        }
                    }

                    /* ===== DRAW LARA (disabled for FPS) ===== */
                    if (0) {
                        /* Find Lara's object definition */
                        tr_object_def_t *lara_obj = NULL;
                        for (int i = 0; i < g_level.object_count; i++) {
                            if (g_level.objects[i].object_id == 0) {
                                lara_obj = &g_level.objects[i];
                                break;
                            }
                        }

                        /* Find Lara's item for position */
                        tr_item_t *lara_item = NULL;
                        for (int i = 0; i < g_level.item_count; i++) {
                            if (g_level.items[i].object_id == 0) {
                                lara_item = &g_level.items[i];
                                break;
                            }
                        }

                        if (lara_obj && lara_item && lara_obj->mesh_count > 0) {
                            /* Decode first animation frame for T-pose */
                            int frame_off = lara_obj->frame_offset / 2; /* byte→word offset */
                            int16_t *fr = &g_level.anim_frames[frame_off];
                            /* fr[0..5] = bounding box, fr[6..8] = root offset */
                            float root_dx = (float)fr[6];
                            float root_dy = (float)fr[7];
                            float root_dz = (float)fr[8];

                            /* 3x4 matrix stack for bone hierarchy (max 8 deep) */
                            float ms[8][12]; /* each: [R00 R01 R02 Tx R10 R11 R12 Ty R20 R21 R22 Tz] */
                            int msp = 0;

                            /* Helper macros for 3x4 matrix */
                            #define M_IDT(m) do { memset(m,0,48); (m)[0]=(m)[4]=(m)[8]=1.0f; } while(0)
                            #define M_TX(m) (m)[3]
                            #define M_TY(m) (m)[7]
                            #define M_TZ(m) (m)[11]
                            #define M_PUSH() do { if(msp<7){memcpy(ms[msp+1],ms[msp],48);msp++;} } while(0)
                            #define M_POP()  do { if(msp>0) msp--; } while(0)
                            #define M_CUR    ms[msp]

                            /* Translate: T += R * (dx,dy,dz) */
                            #define M_TRANS(m,dx,dy,dz) do { \
                                (m)[3]+=(m)[0]*(dx)+(m)[1]*(dy)+(m)[2]*(dz); \
                                (m)[7]+=(m)[4]*(dx)+(m)[5]*(dy)+(m)[6]*(dz); \
                                (m)[11]+=(m)[8]*(dx)+(m)[9]*(dy)+(m)[10]*(dz); \
                            } while(0)

                            /* Rotate around Y: apply Ry to R columns */
                            #define M_ROTY(m,a) do { \
                                float _s=sinf(a),_c=cosf(a); \
                                float _t0,_t1; \
                                _t0=(m)[0];_t1=(m)[2]; (m)[0]=_t0*_c+_t1*_s; (m)[2]=-_t0*_s+_t1*_c; \
                                _t0=(m)[4];_t1=(m)[6]; (m)[4]=_t0*_c+_t1*_s; (m)[6]=-_t0*_s+_t1*_c; \
                                _t0=(m)[8];_t1=(m)[10];(m)[8]=_t0*_c+_t1*_s;(m)[10]=-_t0*_s+_t1*_c; \
                            } while(0)

                            /* Rotate around X */
                            #define M_ROTX(m,a) do { \
                                float _s=sinf(a),_c=cosf(a); \
                                float _t0,_t1; \
                                _t0=(m)[1];_t1=(m)[2]; (m)[1]=_t0*_c-_t1*_s; (m)[2]=_t0*_s+_t1*_c; \
                                _t0=(m)[5];_t1=(m)[6]; (m)[5]=_t0*_c-_t1*_s; (m)[6]=_t0*_s+_t1*_c; \
                                _t0=(m)[9];_t1=(m)[10];(m)[9]=_t0*_c-_t1*_s;(m)[10]=_t0*_s+_t1*_c; \
                            } while(0)

                            /* Rotate around Z */
                            #define M_ROTZ(m,a) do { \
                                float _s=sinf(a),_c=cosf(a); \
                                float _t0,_t1; \
                                _t0=(m)[0];_t1=(m)[1]; (m)[0]=_t0*_c-_t1*_s; (m)[1]=_t0*_s+_t1*_c; \
                                _t0=(m)[4];_t1=(m)[5]; (m)[4]=_t0*_c-_t1*_s; (m)[5]=_t0*_s+_t1*_c; \
                                _t0=(m)[8];_t1=(m)[9]; (m)[8]=_t0*_c-_t1*_s; (m)[9]=_t0*_s+_t1*_c; \
                            } while(0)

                            /* Decode TR1 packed rotation (2 words → 3 angles) */
                            #define TR_ANG(v) ((float)(v) * 6.28318f / 65536.0f)

                            /* Initialize matrix at Lara's world position */
                            M_IDT(M_CUR);
                            M_TX(M_CUR) = (float)lara_item->pos.x;
                            M_TY(M_CUR) = (float)lara_item->pos.y;
                            M_TZ(M_CUR) = (float)lara_item->pos.z;

                            /* Apply Lara's facing direction */
                            float lara_yaw = (float)lara_item->rotation * 3.14159f / 32768.0f;
                            M_ROTY(M_CUR, lara_yaw);

                            /* Apply root offset from animation frame */
                            M_TRANS(M_CUR, root_dx, root_dy, root_dz);

                            /* Draw each mesh in Lara's model.
                             * Skip per-mesh rotations for now — just use bone
                             * offsets so we can verify the skeleton structure. */
                            int mc = lara_obj->mesh_count;
                            if (mc > 15) mc = 15;

                            for (int mi = 0; mi < mc; mi++) {
                                if (mi > 0) {
                                    /* Child mesh: apply bone push/pop + offset */
                                    int bi = lara_obj->bone_idx + (mi - 1);
                                    if (bi < g_level.anim_bone_count) {
                                        tr_anim_bone_t *bone = &g_level.anim_bones[bi];
                                        if (bone->flags & 1) M_POP();
                                        if (bone->flags & 2) M_PUSH();
                                        M_TRANS(M_CUR, (float)bone->x, (float)bone->y, (float)bone->z);
                                    }
                                }

                                /* Get the mesh */
                                int gmi = lara_obj->mesh_idx + mi;
                                if (gmi >= g_level.mesh_count) continue;
                                tr_object_mesh_t *mesh = &g_level.meshes[gmi];

                                /* Draw each face of this mesh */
                                for (int f = 0; f < mesh->face_count; f++) {
                                    tr_face_t *face = &mesh->faces[f];
                                    if (face->vertex_count < 3) continue;

                                    /* Upload texture (same path as rooms) */
                                    uint16_t tex_idx = face->texture & 0x7FFF;
                                    bool has_tex = false;
                                    tr_object_texture_t *otex = NULL;
                                    if (tex_idx < num_otex && tex_surfs) {
                                        otex = &g_level.object_textures[tex_idx];
                                        has_tex = true;
                                        if ((int)tex_idx != last_tex_idx) {
                                            rdpq_tex_upload(TILE0, &tex_surfs[tex_idx], NULL);
                                            last_tex_idx = (int)tex_idx;
                                        }
                                    }

                                    float sv[4][9];
                                    bool visible = true;

                                    for (int v = 0; v < face->vertex_count; v++) {
                                        uint16_t vi = face->vertices[v];
                                        if (vi >= mesh->vertex_count) { visible = false; break; }

                                        /* Transform mesh-local vertex through bone matrix */
                                        float lx = (float)mesh->vertices[vi].x;
                                        float ly = (float)mesh->vertices[vi].y;
                                        float lz = (float)mesh->vertices[vi].z;
                                        float wx = M_CUR[0]*lx + M_CUR[1]*ly + M_CUR[2]*lz + M_CUR[3];
                                        float wy = M_CUR[4]*lx + M_CUR[5]*ly + M_CUR[6]*lz + M_CUR[7];
                                        float wz = M_CUR[8]*lx + M_CUR[9]*ly + M_CUR[10]*lz + M_CUR[11];

                                        /* Camera transform (same as rooms) */
                                        float cx = wx - cam_x;
                                        float cy_val = wy - cam_y;
                                        float cz = wz - cam_z;
                                        float rx = cx * cy - cz * sy;
                                        float rz = cx * sy + cz * cy;

                                        if (rz < 1.0f) { visible = false; break; }
                                        float inv_z = 256.0f / rz;
                                        float screen_x = 160.0f + rx * inv_z;
                                        float screen_y = 120.0f + cy_val * inv_z;

                                        if (screen_x < -1024 || screen_x > 1344 ||
                                            screen_y < -1024 || screen_y > 1264)
                                            { visible = false; break; }

                                        sv[v][0] = screen_x;
                                        sv[v][1] = screen_y;
                                        sv[v][2] = 1.0f; sv[v][3] = 1.0f;
                                        sv[v][4] = 1.0f; sv[v][5] = 1.0f;

                                        if (has_tex && tex_uv_map) {
                                            float tu = (float)(otex->uv[v].u >> 8);
                                            float tv = (float)(otex->uv[v].v >> 8);
                                            sv[v][6] = (tu - tex_uv_map[tex_idx*4+0]) * tex_uv_map[tex_idx*4+2];
                                            sv[v][7] = (tv - tex_uv_map[tex_idx*4+1]) * tex_uv_map[tex_idx*4+3];
                                        } else {
                                            sv[v][6] = 0; sv[v][7] = 0;
                                        }
                                        sv[v][8] = 1.0f / rz;
                                    }

                                    if (!visible) continue;
                                    rdpq_triangle(&TRIFMT_SHADE_TEX, sv[0], sv[1], sv[2]);
                                    if (face->vertex_count == 4)
                                        rdpq_triangle(&TRIFMT_SHADE_TEX, sv[0], sv[2], sv[3]);
                                }
                            }
                        }
                    }

                    /* Show frame */
                    rdpq_detach_show();
                }

                /* START pressed — return to console briefly */
                rdpq_close();
                display_close();
                console_init();
                console_set_render_mode(RENDER_MANUAL);
                printf("Paused. Press START for 3D.\n");
                printf("Camera: %.0f, %.0f, %.0f\n", cam_x, cam_y, cam_z);
                console_render();

                /* Wait for START to re-enter 3D */
                while (1) {
                    joypad_poll();
                    joypad_buttons_t p2 = joypad_get_buttons_pressed(JOYPAD_PORT_1);
                    if (p2.start) break;
                }

                /* Re-init display and rdpq for 3D */
                console_close();
                display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
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

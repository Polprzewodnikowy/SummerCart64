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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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

            /* Parse the .PHD level */
            printf("  Parsing .PHD format...\n");
            n64_mem_init();  /* Reset heap for level data */
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

                printf("LEVEL LOADED! Rendering...\n");
                console_render();

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

                /* Simple sin/cos for camera movement */
                #define SINF(a) ((a) - (a)*(a)*(a)/6.0f + (a)*(a)*(a)*(a)*(a)/120.0f)
                #define COSF(a) (1.0f - (a)*(a)/2.0f + (a)*(a)*(a)*(a)/24.0f)
                #define WRAP_ANGLE(a) ((a) > 3.14159f ? (a) - 6.28318f : ((a) < -3.14159f ? (a) + 6.28318f : (a)))

                /* Render loop */
                render_loop:
                while (1) {
                    joypad_poll();
                    joypad_inputs_t input = joypad_get_inputs(JOYPAD_PORT_1);
                    joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

                    /* Camera movement */
                    float move_speed = 32.0f;
                    float turn_speed = 0.05f;

                    /* Analog stick: move/turn */
                    if (input.stick_x > 20 || input.stick_x < -20)
                        cam_rot_y += input.stick_x * turn_speed * 0.01f;
                    cam_rot_y = WRAP_ANGLE(cam_rot_y);

                    float forward = 0;
                    if (input.stick_y > 20) forward = move_speed;
                    if (input.stick_y < -20) forward = -move_speed;
                    if (input.btn.d_up) forward = move_speed;
                    if (input.btn.d_down) forward = -move_speed;
                    if (input.btn.d_left) cam_rot_y -= turn_speed;
                    if (input.btn.d_right) cam_rot_y += turn_speed;

                    float sy = SINF(WRAP_ANGLE(cam_rot_y));
                    float cy = COSF(WRAP_ANGLE(cam_rot_y));
                    cam_x += forward * sy;
                    cam_z += forward * cy;
                    if (input.btn.c_up) cam_y -= move_speed;
                    if (input.btn.c_down) cam_y += move_speed;

                    /* Start button: return to console */
                    if (pressed.start) break;

                    /* Begin frame */
                    surface_t *disp = display_get();
                    rdpq_attach_clear(disp, NULL);

                    /* Set up for shade triangle rendering (per-vertex color) */
                    rdpq_set_mode_standard();
                    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);

                    /* Build visible room list via portal traversal.
                     * Start from Lara's room, follow portals up to depth 4. */
                    int rooms_to_draw[64];
                    int num_draw = 0;
                    bool room_drawn[256];
                    memset(room_drawn, 0, sizeof(room_drawn));

                    /* BFS portal traversal */
                    rooms_to_draw[num_draw++] = lara_room;
                    room_drawn[lara_room] = true;
                    int queue_head = 0;
                    while (queue_head < num_draw && num_draw < 60) {
                        int ri = rooms_to_draw[queue_head++];
                        if (ri < 0 || ri >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[ri];
                        for (int p = 0; p < room->portal_count && num_draw < 60; p++) {
                            int dest = room->portals[p].room;
                            if (dest >= 0 && dest < g_level.room_count && !room_drawn[dest]) {
                                room_drawn[dest] = true;
                                rooms_to_draw[num_draw++] = dest;
                            }
                        }
                    }

                    for (int ri = 0; ri < num_draw; ri++) {
                        int room_idx = rooms_to_draw[ri];
                        if (room_idx < 0 || room_idx >= g_level.room_count) continue;
                        tr_room_t *room = &g_level.rooms[room_idx];

                        for (int f = 0; f < room->mesh.face_count; f++) {
                            tr_face_t *face = &room->mesh.faces[f];
                            if (face->vertex_count < 3) continue;

                            /* Transform vertices: world → screen with per-vertex color */
                            /* TRIFMT_SHADE: {X, Y, R, G, B, A} per vertex */
                            float sv[4][6];
                            bool visible = true;

                            for (int v = 0; v < face->vertex_count; v++) {
                                uint16_t vi = face->vertices[v];
                                if (vi >= room->mesh.vertex_count) { visible = false; break; }

                                tr_room_vertex_t *rv = &room->mesh.vertices[vi];

                                /* World position relative to camera */
                                float wx = room->x + rv->x - cam_x;
                                float wy = rv->y - cam_y;
                                float wz = room->z + rv->z - cam_z;

                                /* Rotate by camera Y */
                                float rx = wx * cy - wz * sy;
                                float rz = wx * sy + wz * cy;

                                /* Skip if behind camera */
                                if (rz < 32.0f) { visible = false; break; }

                                /* Perspective projection */
                                float inv_z = 256.0f / rz;
                                float sx = 160.0f + rx * inv_z;
                                float screen_y = 120.0f + wy * inv_z;

                                /* Cull if way off screen */
                                if (sx < -200 || sx > 520 || screen_y < -200 || screen_y > 440)
                                    { visible = false; break; }

                                sv[v][0] = sx;
                                sv[v][1] = screen_y;

                                /* Per-vertex lighting from TR1 light value.
                                 * 0 = brightest, 0x1FFF = darkest */
                                int16_t light = rv->light;
                                float bright = 1.0f - ((float)light / 8191.0f);
                                if (bright < 0.05f) bright = 0.05f;
                                if (bright > 1.0f) bright = 1.0f;
                                sv[v][2] = bright;         /* R */
                                sv[v][3] = bright;         /* G */
                                sv[v][4] = bright * 0.9f;  /* B (slight warm tint) */
                                sv[v][5] = 1.0f;           /* A */
                            }

                            if (!visible) continue;

                            /* Draw shaded triangle(s) */
                            rdpq_triangle(&TRIFMT_SHADE, sv[0], sv[1], sv[2]);

                            if (face->vertex_count == 4) {
                                rdpq_triangle(&TRIFMT_SHADE, sv[0], sv[2], sv[3]);
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
                printf("Rooms rendered: %d\n", num_draw);
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

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

#define GAME_HEAP_SIZE  (2 * 1024 * 1024)
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

    /* Read archive header from ROM via PI DMA */
    static uint8_t header_buf[512] __attribute__((aligned(16)));
    data_cache_hit_invalidate(header_buf, 512);
    pi_wait();
    *(volatile uint32_t *)(0xA4600000 + 0x00) = (uint32_t)header_buf & 0x1FFFFFFF;  /* PI_DRAM_ADDR */
    *(volatile uint32_t *)(0xA4600000 + 0x04) = ASSET_CART_ADDR;                     /* PI_CART_ADDR */
    *(volatile uint32_t *)(0xA4600000 + 0x0C) = 512 - 1;                             /* PI_WR_LEN */
    pi_wait();
    data_cache_hit_invalidate(header_buf, 512);

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

    /* If we have assets, try to load level 1 */
    if (g_has_assets) {
        printf("\nLoading LEVEL1 (Caves)...\n");

        uint32_t level_size = 0;
        /* Note: asset_load is renamed to sc64_asset_load via -D flag
         * to avoid collision with libdragon's asset_load */
        void *level_data = asset_load_with_size(&g_asset_mgr, 1, &level_size);

        if (level_data && level_size > 0) {
            printf("  Loaded %u KB from SD card.\n", level_size / 1024);

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

                printf("LEVEL LOADED SUCCESSFULLY!\n");
                printf("Tomb Raider 1 on N64.\n");
            } else {
                printf("  Parse FAILED: %s\n", tr_parse_error_string(res));
            }
        } else {
            printf("  Failed to load from SD!\n");
        }
    } else {
        printf("\nNo assets available.\n");
        printf("For SC64 hardware:\n");
        printf("  1. Pack levels: tr1_pack.py\n");
        printf("  2. Write to SD: dd seek=65536\n");
        printf("  3. Boot via SC64 menu\n");
    }

    console_render();

    /* Idle loop with controller test */
    while (1) {
        joypad_poll();
        joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if (keys.start) {
            /* Reload / cycle levels in future */
            console_clear();
            printf("START pressed — future: menu\n");
            console_render();
        }
    }

    return 0;
}

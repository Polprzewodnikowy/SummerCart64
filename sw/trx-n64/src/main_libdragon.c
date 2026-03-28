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
    printf("Loading assets from SD card...\n");

    /* Try to open the asset file from the filesystem */
    FILE *asset_file = fopen("rom:/tr1_assets.bin", "rb");
    if (!asset_file) {
        asset_file = fopen("sd:/tr1_assets.bin", "rb");
    }
    if (!asset_file) {
        /* Try DFS (DragonFS) */
        asset_file = fopen("/tr1_assets.bin", "rb");
    }

    if (asset_file) {
        printf("  Asset file opened!\n");

        /* Read the entire file into memory */
        fseek(asset_file, 0, SEEK_END);
        long file_size = ftell(asset_file);
        fseek(asset_file, 0, SEEK_SET);
        printf("  File size: %ld bytes\n", file_size);

        /* Read first 512 bytes (archive header) */
        static uint8_t header_buf[512] __attribute__((aligned(16)));
        size_t read = fread(header_buf, 1, 512, asset_file);
        printf("  Read %u bytes\n", (unsigned)read);
        printf("  Magic: %c%c%c%c\n",
               header_buf[0], header_buf[1], header_buf[2], header_buf[3]);

        fclose(asset_file);
    } else {
        printf("  No asset file found.\n");
        printf("  Copy tr1_assets.bin to SD card.\n");
    }

    /* Also try SC64 raw access for comparison */
    if (sc64_ok) {
        printf("\nTrying SC64 raw SD access...\n");
        asset_error_t err = asset_init(&g_asset_mgr, &(asset_init_config_t){
            .archive_base_sector = ARCHIVE_SECTOR,
            .cache_base = CACHE_BASE,
            .cache_size = CACHE_SIZE,
            .slot_size = SLOT_SIZE,
            .staging_buffer = staging,
            .staging_size = sizeof(staging),
            .backend = NULL,
        });

        if (err == ASSET_OK) {
            printf("  SC64 OK! %u assets found.\n", g_asset_mgr.index_count);
            g_has_assets = true;
        } else {
            printf("  SC64 failed: %s\n", asset_error_string(err));
        }
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

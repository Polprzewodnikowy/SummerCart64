/*
 * TRX-N64: Tomb Raider 1 — N64 + SC64 Port
 * Libdragon boot — works in emulators + SC64 real hardware
 *
 * Uses libdragon for boot/display/input.
 * Uses SC64 registers directly for asset streaming from SD card.
 * Uses our own .PHD parser for level loading.
 */

#include <libdragon.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Our game code */
#include "level/tr_types.h"
#include "level/tr_level_parse.h"

/* Asset streaming system */
#include "asset_manager.h"

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

#define SC64_REGS_BASE  0x1FFF0000UL
#define SC64_REG_KEY    (*(volatile uint32_t *)(SC64_REGS_BASE + 0x10))
#define SC64_KEY_RESET  0x00000000UL
#define SC64_KEY_LOCK   0xFFFFFFFFUL
#define SC64_KEY_UNL_1  0x5F554E4CUL  /* "_UNL" */
#define SC64_KEY_UNL_2  0x4F434B5FUL  /* "OCK_" */

/* PI interface for register access */
#define PI_BASE_REG     0xA4600000UL
#define PI_STATUS_REG   (*(volatile uint32_t *)(PI_BASE_REG + 0x10))

static void pi_wait(void) {
    while (PI_STATUS_REG & 3) {}
}

static void sc64_pi_write(volatile uint32_t *addr, uint32_t value) {
    pi_wait();
    *addr = value;
    pi_wait();
}

static bool sc64_unlock(void) {
    sc64_pi_write(&SC64_REG_KEY, SC64_KEY_RESET);
    sc64_pi_write(&SC64_REG_KEY, SC64_KEY_UNL_1);
    sc64_pi_write(&SC64_REG_KEY, SC64_KEY_UNL_2);

    /* Check if unlock succeeded by reading key register */
    pi_wait();
    uint32_t val = SC64_REG_KEY;
    pi_wait();
    return (val != 0);  /* Non-zero = unlocked */
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

    /* Try to initialize asset streaming */
    if (sc64_ok) {
        printf("Initializing asset streaming...\n");
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
            printf("  OK! %u assets found.\n", g_asset_mgr.index_count);
            g_has_assets = true;
        } else {
            printf("  Failed: %s\n", asset_error_string(err));
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

/*
 * TRX-N64: Tomb Raider 1 — N64 + SC64 Port
 *
 * Main entry point. Initializes all subsystems and runs the game loop.
 *
 * Architecture overview:
 *
 *   ┌─────────────────────────────────────────────┐
 *   │              TRX Game Logic                  │
 *   │  (lara, items, rooms, collision, anims, AI)  │
 *   │          Pure C — portable as-is             │
 *   ├────────────┬──────────────┬──────────────────┤
 *   │  N64 RDP   │  N64 Audio   │  N64 Platform    │
 *   │  Renderer  │  (RSP stub)  │  (VI, joybus,    │
 *   │            │              │   timer, memory)  │
 *   ├────────────┴──────────────┴──────────────────┤
 *   │         SC64 Asset Streaming System           │
 *   │    (SD card → LZ4 decompress → LRU cache)     │
 *   ├──────────────────────────────────────────────┤
 *   │   N64 Hardware (RDRAM, RDP, RSP, VI, PI)     │
 *   │   SC64 Hardware (SD card, SDRAM, registers)   │
 *   └──────────────────────────────────────────────┘
 *
 * Build milestones:
 *   M1: Boot → display solid color → read controller    ✓ (platform)
 *   M2: Load level geometry → render wireframe rooms     (← current)
 *   M3: Textured rooms with TMEM tile cache
 *   M4: Lara model + animation playback
 *   M5: Collision + movement
 *   M6: Full game logic integration
 *   M7: Audio (RSP mixer)
 *   M8: Polish + optimization
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "platform/n64_platform.h"
#include "render/n64_render.h"
#include "level/n64_level_loader.h"

/* Asset streaming system */
#include "../asset_streaming/src/asset_manager.h"
#include "../asset_streaming/src/asset_debug.h"

/* Bootloader infrastructure */
#include "display.h"
#include "io.h"
#include "sc64.h"
#include "exception.h"

#include <string.h>

/* -------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------- */

asset_manager_t g_asset_mgr;
n64_renderer_t g_renderer;
n64_input_state_t g_input;
n64_level_state_t g_level;

/* Asset streaming configuration */
#define ARCHIVE_SECTOR      0x10000         /* 32 MB into SD card */
#define CACHE_BASE          ((uint8_t *)0xB1000000)
#define CACHE_SIZE          (3 * 1024 * 1024)   /* 3 MB streaming cache */
#define SLOT_SIZE           (512 * 1024)         /* 512 KB per slot = 6 slots */

static uint8_t staging[512 * 1024] __attribute__((aligned(16)));

/* -------------------------------------------------------------------
 * Asset load helper (called by n64_platform.c file bridge)
 * ------------------------------------------------------------------- */

void *trx_asset_load(uint32_t asset_id, uint32_t *out_size) {
    return asset_load_with_size(&g_asset_mgr, asset_id, out_size);
}

/* -------------------------------------------------------------------
 * Boot screen (uses bootloader display for early init messages)
 * ------------------------------------------------------------------- */

static void show_boot_screen(void) {
    display_init(NULL);
    display_printf("TRX-N64: Tomb Raider 1\n");
    display_printf("N64 + SC64 Port\n");
    display_printf("========================\n\n");
}

static void boot_msg(const char *msg) {
    display_printf("%s\n", msg);
}

static void boot_error(const char *msg) {
    display_printf("\nERROR: %s\n", msg);
    display_printf("\nSystem halted.\n");
    while (1) { delay_ms(1000); }
}

/* -------------------------------------------------------------------
 * Subsystem initialization
 * ------------------------------------------------------------------- */

static bool init_asset_streaming(void) {
    boot_msg("Initializing asset streaming...");

    asset_debug_set_log(NULL);  /* Quiet for now */

    asset_error_t err = asset_init(&g_asset_mgr, &(asset_init_config_t){
        .archive_base_sector = ARCHIVE_SECTOR,
        .cache_base = CACHE_BASE,
        .cache_size = CACHE_SIZE,
        .slot_size = SLOT_SIZE,
        .staging_buffer = staging,
        .staging_size = sizeof(staging),
        .backend = NULL,  /* Real SC64 hardware */
    });

    if (err != ASSET_OK) {
        display_printf("  asset_init: %s\n", asset_error_string(err));
        return false;
    }

    display_printf("  %u assets, %u cache slots\n",
                   g_asset_mgr.index_count,
                   g_asset_mgr.cache.config.slot_count);
    return true;
}

static void init_renderer(void) {
    boot_msg("Initializing N64 renderer...");
    n64_render_init(&g_renderer);
    n64_video_init(N64_VIDEO_NTSC);
    boot_msg("  320x240 16-bit, double-buffered");
}

static void init_input(void) {
    boot_msg("Initializing controller...");
    n64_input_init();
    memset(&g_input, 0, sizeof(g_input));
}

static void init_memory(void) {
    boot_msg("Initializing memory pools...");
    n64_mem_init();
    display_printf("  %u KB game heap\n", N64_GAME_HEAP_SIZE / 1024);
}

/* -------------------------------------------------------------------
 * Level loading (milestone 2)
 * ------------------------------------------------------------------- */

static bool load_level(uint32_t level_num) {
    display_init(NULL);
    display_printf("Loading level %u...\n\n", level_num);

    if (!n64_level_begin(&g_level, level_num)) {
        display_printf("Failed to load level %u!\n", level_num);
        display_printf("  Check SD card has game assets.\n");
        delay_ms(3000);
        return false;
    }

    display_printf("  Level data: %u KB\n", g_level.level_size / 1024);

    /*
     * TODO (M2): Call TRX section readers here:
     *   Level_Section_ReadRooms(&g_level)
     *   Level_Section_ReadObjectMeshes(&g_level)
     *   Level_Section_ReadAnims(&g_level)
     *   ... etc
     *
     * Each section reader calls n64_level_read() which reads
     * sequentially from the cached level data.
     */

    /* Load texture pages and convert to N64 format */
    if (!n64_level_load_textures(&g_level)) {
        display_printf("  Warning: texture load failed\n");
    }

    display_printf("\nLevel loaded. Press A.\n");

    /* Wait for button */
    n64_input_state_t pad;
    do {
        delay_ms(16);
        n64_input_poll(&pad);
    } while (!n64_input_pressed(&pad, TR_INPUT_ACTION));

    return true;
}

/* -------------------------------------------------------------------
 * Game loop
 * ------------------------------------------------------------------- */

static void game_frame(void) {
    /* Poll input */
    n64_input_poll(&g_input);

    /*
     * TODO (M5+): Run TRX game logic:
     *   Game_Control()
     *   Lara_Control()
     *   Item_Control()
     *   ... etc
     */

    /* Begin rendering */
    n64_render_frame_begin(&g_renderer);

    /*
     * TODO (M2): Render rooms:
     *   - Determine visible rooms via portal traversal
     *   - For each visible room:
     *     - Sort faces by texture
     *     - Bind texture tile in TMEM
     *     - Submit triangles to RDP
     *
     * TODO (M4): Render entities:
     *   - Lara model with animation interpolation
     *   - Enemy models
     *   - Pickups, traps, doors
     */

    /* Debug overlay */
    n64_render_debug_overlay(&g_renderer);

    /* End frame: flush RDP, swap buffers */
    n64_render_frame_end(&g_renderer);

    /* VSync */
    n64_video_wait_vsync();
}

/* -------------------------------------------------------------------
 * Title screen / menu (simplified for initial port)
 * ------------------------------------------------------------------- */

static uint32_t title_screen(void) {
    display_init(NULL);
    display_printf("\n\n\n\n\n");
    display_printf("        TOMB RAIDER\n\n");
    display_printf("        N64 + SC64 Port\n\n\n");
    display_printf("    Press START to begin\n\n");
    display_printf("    D-Pad Up/Down: select level\n");

    uint32_t selected_level = 0;
    n64_input_state_t pad;
    memset(&pad, 0, sizeof(pad));

    while (1) {
        n64_input_poll(&pad);

        if (n64_input_pressed(&pad, TR_INPUT_UP)) {
            if (selected_level < 14) selected_level++;
        }
        if (n64_input_pressed(&pad, TR_INPUT_DOWN)) {
            if (selected_level > 0) selected_level--;
        }
        if (n64_input_pressed(&pad, TR_INPUT_PAUSE)) {
            return selected_level;
        }

        /* Redraw level selector */
        /* TODO: proper title screen rendering via RDP */
        display_init(NULL);
        display_printf("\n\n\n\n\n");
        display_printf("        TOMB RAIDER\n\n");
        display_printf("        N64 + SC64 Port\n\n\n");
        display_printf("    > Level %u\n\n", selected_level + 1);
        display_printf("    Press START to begin\n");
        display_printf("    D-Pad Up/Down: select level\n");

        delay_ms(100);
    }
}

/* -------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------- */

void main(void) {
    /* Early init using bootloader infrastructure */
    show_boot_screen();
    sc64_unlock();

    /* Initialize subsystems */
    init_memory();
    init_input();

    if (!init_asset_streaming()) {
        boot_error("Asset streaming init failed.\n"
                   "Ensure game archive is on SD card\n"
                   "at sector 0x10000.");
    }

    init_renderer();

    boot_msg("\nAll systems initialized.");
    boot_msg("Press A to continue.");

    /* Wait for user */
    n64_input_state_t pad;
    memset(&pad, 0, sizeof(pad));
    do {
        delay_ms(16);
        n64_input_poll(&pad);
    } while (!n64_input_pressed(&pad, TR_INPUT_ACTION));

    /* Game loop */
    while (1) {
        uint32_t level = title_screen();

        if (load_level(level)) {
            /* Run game until level ends or player quits */
            while (1) {
                game_frame();

                /* Check for pause/quit */
                if (n64_input_pressed(&g_input, TR_INPUT_PAUSE)) {
                    /* TODO: pause menu */
                    break;  /* Return to title for now */
                }
            }

            n64_level_end(&g_level);
            n64_mem_reset();  /* Free per-level allocations */
        }
    }
}

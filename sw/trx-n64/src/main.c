/*
 * TRX-N64: Tomb Raider 1 — N64 + SC64 Port
 *
 * Main entry point. Initializes all subsystems and runs the game loop.
 *
 * FAITHFULNESS POLICY:
 *   This port aims to be 100% faithful to the original TR1 gameplay.
 *   All game logic, physics, timing, AI, collision, and mechanics
 *   come directly from the TRX reimplementation with zero gameplay
 *   modifications. The only platform adaptations are:
 *     - Rendering: OpenGL → N64 RDP (same geometry, same textures)
 *     - Audio: SDL → N64 RSP (same samples, same mixing)
 *     - Input: Keyboard/Gamepad → N64 Controller
 *     - Storage: Filesystem → SD card asset streaming
 *   The game tick rate, animation timing, physics step size, and
 *   all numeric constants are preserved exactly from TRX.
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
 *   M2: Load level geometry → render lit rooms           ✓ (current)
 *   M3: Textured rooms with TMEM tile cache
 *   M4: Lara model + animation playback
 *   M5: Collision + movement (TRX game logic integration)
 *   M6: Full game logic (enemies, pickups, triggers, saves)
 *   M7: Audio (software mixer + N64 AI DMA, 3D positioning) ✓ (current)
 *   M8: Optimization (Z-off, face sort, bbox cull, state batch) ✓ (current)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "platform/n64_platform.h"
#include "render/n64_render.h"
#include "render/n64_transform.h"
#include "render/n64_room_draw.h"
#include "render/n64_textures.h"
#include "render/n64_entity_draw.h"
#include "anim/tr_anim.h"
#include "game/tr_game_shim.h"
#include "audio/n64_audio.h"
#include "level/n64_level_loader.h"
#include "level/tr_level_parse.h"
#include "level/tr_types.h"

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
audio_state_t g_audio;
n64_input_state_t g_input;
n64_level_state_t g_level_state;
tr_level_t g_level;
transform_state_t g_transform;
room_draw_state_t g_room_draw;

/* Texture pages (converted to N64 CI8 format) */
static n64_texture_page_t g_tex_pages[N64_MAX_TEX_PAGES];

/* Animation state per entity (used by renderer, synced from game state) */
#define MAX_ENTITIES    256
static tr_anim_state_t g_entity_anims[MAX_ENTITIES];
static tr_matrix_stack_t g_matrix_stack;

/* Game mode toggle (free-fly debug vs real game) */
static bool g_debug_camera = false;

/*
 * TR1 runs at 30 fps (33.33ms per frame).
 * All game logic is tied to this frame rate — we preserve it exactly.
 */
#define TR1_FRAME_MS    33

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

    asset_debug_set_log(NULL);

    asset_error_t err = asset_init(&g_asset_mgr, &(asset_init_config_t){
        .archive_base_sector = ARCHIVE_SECTOR,
        .cache_base = CACHE_BASE,
        .cache_size = CACHE_SIZE,
        .slot_size = SLOT_SIZE,
        .staging_buffer = staging,
        .staging_size = sizeof(staging),
        .backend = NULL,
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

    /* Initialize transform pipeline: 320×240, matching original TR1's
     * internal resolution (PS1 was 320×240 as well). */
    transform_init(&g_transform, N64_SCREEN_W, N64_SCREEN_H);

    /* TR1 default FOV: ~80° (≈14563 in TR angle units).
     * This matches the original exactly. */
    transform_set_perspective(&g_transform, 14563,
                               FIX16(16), FIX16(20480));

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
 * Level loading
 * ------------------------------------------------------------------- */

static bool load_level(uint32_t level_num) {
    display_init(NULL);
    display_printf("Loading level %u...\n\n", level_num);

    /* Load level data via asset streaming */
    if (!n64_level_begin(&g_level_state, level_num)) {
        display_printf("Failed to load level %u!\n", level_num);
        display_printf("  Check SD card has game assets.\n");
        delay_ms(3000);
        return false;
    }

    display_printf("  Level data: %u KB\n", g_level_state.level_size / 1024);

    /* Parse the .PHD binary format into our level structures */
    display_printf("  Parsing level format...\n");
    tr_parse_result_t res = tr_level_parse(g_level_state.level_data,
                                            g_level_state.level_size,
                                            &g_level);
    if (res != TR_PARSE_OK) {
        display_printf("  Parse error: %s\n", tr_parse_error_string(res));
        delay_ms(3000);
        return false;
    }

    display_printf("  Rooms: %d\n", g_level.room_count);
    display_printf("  Meshes: %d\n", (int)g_level.mesh_count);
    display_printf("  Items: %d\n", (int)g_level.item_count);
    display_printf("  Textures: %d pages, %d object tex\n",
                   (int)g_level.tex_page_count,
                   (int)g_level.object_texture_count);
    display_printf("  Animations: %d\n", (int)g_level.anim_count);
    display_printf("  Memory used: %u KB / %u KB\n",
                   (N64_GAME_HEAP_SIZE - n64_mem_available()) / 1024,
                   N64_GAME_HEAP_SIZE / 1024);

    /* Convert texture pages to N64 CI8 format — pixel-perfect, byte-for-byte
     * preservation of original palette indices. Only the palette colors are
     * converted from TR1's 6-bit RGB to RGBA5551. */
    if (g_level.tex_pages && g_level.tex_page_count > 0) {
        display_printf("  Converting %d texture pages (CI8, faithful)...\n",
                       (int)g_level.tex_page_count);
        n64_tex_init(g_tex_pages,
                     g_level.tex_pages,
                     g_level.palette,
                     g_level.tex_page_count);
    }

    /* Initialize room renderer with texture pages */
    room_draw_init(&g_room_draw, &g_level, &g_transform, &g_renderer, g_tex_pages);

    /* Initialize game state — this sets up all entities, finds Lara,
     * initializes the camera, and prepares the game for play.
     * All physics constants and state machine values are TR1-exact. */
    game_init(&g_game, &g_level);
    tr_matrix_init(&g_matrix_stack);

    /* Sync renderer's entity anim array from game state */
    memset(g_entity_anims, 0, sizeof(g_entity_anims));
    for (int i = 0; i < g_game.item_count && i < MAX_ENTITIES; i++) {
        g_entity_anims[i] = g_game.items[i].anim;
    }
    display_printf("  Entities initialized: %d\n", g_game.item_count);
    if (g_game.lara_item) {
        display_printf("  Lara at room %d: (%d, %d, %d)\n",
                       g_game.lara_item->room_num,
                       g_game.lara_item->pos.x,
                       g_game.lara_item->pos.y,
                       g_game.lara_item->pos.z);
    }

    /* Set camera from game state (game_init found Lara and positioned camera) */
    transform_set_camera(&g_transform, &g_game.camera);

    display_printf("\nLevel loaded. Press A to explore.\n");
    display_printf("Analog=move, C=look, Z=down, R=up\n");

    n64_input_state_t pad;
    memset(&pad, 0, sizeof(pad));
    do {
        delay_ms(16);
        n64_input_poll(&pad);
    } while (!n64_input_pressed(&pad, TR_INPUT_ACTION));

    return true;
}

/* -------------------------------------------------------------------
 * Free-fly camera (milestone 2 debug tool)
 *
 * Lets you fly through the level geometry with the N64 controller.
 * Will be replaced by TRX's camera system in milestone 5.
 * ------------------------------------------------------------------- */

#define FLY_SPEED   64      /* World units per frame */
#define TURN_SPEED  512     /* Angle units per frame */

static void update_free_camera(void) {
    tr_camera_t *cam = &g_game.camera;

    /* Rotation with C-buttons and analog stick */
    if (n64_input_held(&g_input, TR_INPUT_LOOK_UP))
        cam->rot_x -= TURN_SPEED;
    if (n64_input_held(&g_input, TR_INPUT_LOOK_DOWN))
        cam->rot_x += TURN_SPEED;
    if (g_input.analog_x > 12 || g_input.analog_x < -12)
        cam->rot_y += g_input.analog_x * 8;

    if (cam->rot_x > 16384) cam->rot_x = 16384;
    if (cam->rot_x < -16384) cam->rot_x = -16384;

    int32_t speed = 0;
    if (g_input.analog_y > 12)
        speed = (g_input.analog_y * FLY_SPEED) >> 7;
    else if (g_input.analog_y < -12)
        speed = (g_input.analog_y * FLY_SPEED) >> 7;
    if (n64_input_held(&g_input, TR_INPUT_UP)) speed = FLY_SPEED;
    if (n64_input_held(&g_input, TR_INPUT_DOWN)) speed = -FLY_SPEED;

    fix16_t sy = tr_sin(cam->rot_y);
    fix16_t cy = tr_cos(cam->rot_y);
    cam->x += (speed * (sy >> 8)) >> 8;
    cam->z += (speed * (cy >> 8)) >> 8;

    if (n64_input_held(&g_input, TR_INPUT_LOOK))
        cam->y -= FLY_SPEED;
    if (n64_input_held(&g_input, TR_INPUT_DRAW))
        cam->y += FLY_SPEED;

    transform_set_camera(&g_transform, cam);
}

/* -------------------------------------------------------------------
 * Game loop
 * ------------------------------------------------------------------- */

static void game_frame(void) {
    uint32_t frame_start = n64_timer_ticks();

    /* Poll input */
    n64_input_poll(&g_input);

    /* Map N64 controller to game input bits */
    game_map_input(&g_game, &g_input);

    /* Toggle debug camera with L+R+Start */
    if (n64_input_held(&g_input, TR_INPUT_LOOK) &&
        n64_input_pressed(&g_input, TR_INPUT_PAUSE)) {
        g_debug_camera = !g_debug_camera;
    }

    if (g_debug_camera) {
        /* Free-fly debug camera (M2 behavior) */
        update_free_camera();
    } else {
        /*
         * Game logic tick — exact TR1 update order:
         *   1. Lara control (movement, physics, state machine)
         *   2. Other item control (enemies, traps — M6)
         *   3. Camera update (chase Lara)
         *
         * All physics, collision, and state transitions use
         * exact TR1 constants and algorithms.
         */
        game_frame_tick(&g_game);

        /* Sync camera from game state to transform pipeline */
        transform_set_camera(&g_transform, &g_game.camera);
    }

    /* Sync entity animation states from game to renderer */
    for (int i = 0; i < g_game.item_count && i < MAX_ENTITIES; i++) {
        g_entity_anims[i] = g_game.items[i].anim;
    }

    /* Determine camera room for rendering */
    int16_t camera_room = g_debug_camera ?
        game_get_room_at(&g_game, g_game.camera_room,
                          g_game.camera.x, g_game.camera.y, g_game.camera.z) :
        g_game.camera_room;

    /* Begin rendering */
    n64_render_frame_begin(&g_renderer);

    /* Draw all visible rooms from camera's current room */
    room_draw_frame(&g_room_draw, camera_room);

    /* Draw entities in visible rooms */
    for (int i = 0; i < g_room_draw.visible_count; i++) {
        int16_t room_idx = g_room_draw.visible[i].room_idx;
        n64_draw_room_entities(&g_level, room_idx,
                                &g_matrix_stack, &g_transform,
                                &g_renderer.cmd_buf,
                                &g_room_draw.tmem,
                                g_tex_pages,
                                g_entity_anims,
                                MAX_ENTITIES);
    }

    /* Debug overlay showing stats */
    n64_render_debug_overlay(&g_renderer);

    /* Mix and submit one frame of audio to AI */
    audio_mix_frame(&g_audio);

    /* End frame: flush RDP, swap buffers */
    n64_render_frame_end(&g_renderer);

    /* Frame pacing: TR1 runs at exactly 30 fps (33ms per frame).
     * This preserves original gameplay timing — all physics,
     * animation, and AI are tied to this frame rate. */
    uint32_t elapsed_ms = (n64_timer_ticks() - frame_start) / N64_TICKS_PER_MS;
    if (elapsed_ms < TR1_FRAME_MS) {
        n64_timer_wait_ms(TR1_FRAME_MS - elapsed_ms);
    }
}

/* -------------------------------------------------------------------
 * Title screen / menu
 * ------------------------------------------------------------------- */

static const char *level_names[] = {
    "Caves",
    "City of Vilcabamba",
    "Lost Valley",
    "Tomb of Qualopec",
    "St. Francis' Folly",
    "Colosseum",
    "Palace Midas",
    "The Cistern",
    "Tomb of Tihocan",
    "City of Khamoon",
    "Obelisk of Khamoon",
    "Sanctuary of the Scion",
    "Natla's Mines",
    "Atlantis",
    "The Great Pyramid",
};

static uint32_t title_screen(void) {
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

        display_init(NULL);
        display_printf("\n\n\n");
        display_printf("        TOMB RAIDER\n\n");
        display_printf("        N64 + SC64 Port\n");
        display_printf("        (Faithful to original)\n\n");
        display_printf("  > %d. %s\n\n", selected_level + 1,
                       level_names[selected_level]);
        display_printf("  D-Pad Up/Down: select level\n");
        display_printf("  START: begin\n");

        delay_ms(100);
    }
}

/* -------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------- */

void main(void) {
    show_boot_screen();
    sc64_unlock();

    init_memory();
    init_input();

    if (!init_asset_streaming()) {
        boot_error("Asset streaming init failed.\n"
                   "Ensure game archive is on SD card\n"
                   "at sector 0x10000.");
    }

    init_renderer();

    boot_msg("Initializing audio...");
    audio_init(&g_audio);
    display_printf("  22050 Hz 16-bit stereo, %d channels\n", AUDIO_MAX_CHANNELS);

    boot_msg("\nAll systems initialized.");
    boot_msg("Press A to continue.");

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
            while (1) {
                game_frame();

                if (n64_input_pressed(&g_input, TR_INPUT_PAUSE)) {
                    break;
                }
            }

            n64_level_end(&g_level_state);
            n64_mem_reset();
        }
    }
}

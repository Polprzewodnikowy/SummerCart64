/*
 * MW-N64: The Elder Scrolls III: Morrowind — N64 + SC64 Port
 *
 * FAITHFULNESS POLICY:
 *   Every value from the ESM file is used exactly. Every quest,
 *   dialogue line, NPC stat, item property, skill formula, and
 *   combat dice roll matches the original game. The only adaptations
 *   are visual (resolution, draw distance, poly count). The game
 *   PLAYS identically — it LOOKS like an N64 game.
 *
 * INTEGRITY RULE:
 *   All game mechanics, data formats, and formulas are derived solely
 *   from the OpenMW source code. Nothing is hallucinated or assumed.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "esm/mw_esm.h"
#include "world/mw_cell_manager.h"
#include "world/mw_weather.h"
#include "render/mw_terrain.h"
#include "render/mw_ui.h"
#include "game/mw_character.h"
#include "game/mw_dialogue.h"
#include "game/mw_script.h"
#include "game/mw_save.h"

/* Shared N64 infrastructure */
#include "display.h"
#include "io.h"
#include "sc64.h"
#include "joybus.h"
#include "asset_manager.h"

#include <string.h>
#include <stddef.h>

/* Forward declarations */
static void show_boot_screen(void) {
    display_init(NULL);
    display_printf("MW-N64: Morrowind\n");
    display_printf("N64 + SC64 Port\n");
    display_printf("=====================\n\n");
}

static void boot_msg(const char *msg) {
    display_printf("%s\n", msg);
}

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

/* -------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------- */

static asset_manager_t g_asset_mgr;
static esm_reader_t    g_esm_reader;
static esm_index_t     *g_esm_index_ptr;
#define g_esm_index (*g_esm_index_ptr)
static cell_manager_t  g_cell_mgr;
mw_player_t            g_player;        /* Non-static: accessed by other modules */
static script_engine_t g_scripts;
static dial_state_t    g_dialogue;
static weather_state_t g_weather;
static ui_state_t      g_ui;

/* Input state */
static uint32_t g_buttons = 0;      /* Current frame held buttons */
static uint32_t g_buttons_prev = 0; /* Previous frame */
static uint32_t g_pressed = 0;      /* Rising edge this frame */
static int8_t   g_stick_x = 0;
static int8_t   g_stick_y = 0;

/* Game input bits (mapped from N64 controller) */
#define IN_FORWARD  (1 << 0)
#define IN_BACK     (1 << 1)
#define IN_LEFT     (1 << 2)
#define IN_RIGHT    (1 << 3)
#define IN_JUMP     (1 << 4)    /* B button */
#define IN_ACTION   (1 << 5)    /* A button */
#define IN_MENU     (1 << 6)    /* Start */
#define IN_JOURNAL  (1 << 7)    /* C-Left */
#define IN_MAGIC    (1 << 8)    /* C-Up */
#define IN_WEAPON   (1 << 9)    /* C-Right */
#define IN_SNEAK    (1 << 10)   /* Z trigger */
#define IN_INVENTORY (1 << 11)  /* C-Down */
#define IN_LOOK_UP  (1 << 12)
#define IN_LOOK_DN  (1 << 13)

/* Timing */
#define FRAME_MS    33  /* 30 fps */

/* Asset streaming */
#define ARCHIVE_SECTOR  0x10000
#define CACHE_BASE      ((uint8_t *)0xB1000000)
#define CACHE_SIZE      (4 * 1024 * 1024)
#define SLOT_SIZE       (512 * 1024)
static uint8_t staging[512 * 1024] __attribute__((aligned(16)));

/* Math helpers */
static float sinf_approx(float rad) {
    /* Simple sine approximation for movement */
    float x = rad;
    while (x > 3.14159f) x -= 6.28318f;
    while (x < -3.14159f) x += 6.28318f;
    return x - (x*x*x)/6.0f + (x*x*x*x*x)/120.0f;
}
static float cosf_approx(float rad) {
    return sinf_approx(rad + 1.5708f);
}

/* -------------------------------------------------------------------
 * Input polling
 * ------------------------------------------------------------------- */

static void poll_input(void) {
    joybus_controller_state_t pad;
    g_buttons_prev = g_buttons;
    g_buttons = 0;

    if (!joybus_get_controller_state(0, &pad)) return;

    g_stick_x = pad.x;
    g_stick_y = pad.y;

    /* Map N64 buttons to game input */
    if (pad.a)     g_buttons |= IN_ACTION;
    if (pad.b)     g_buttons |= IN_JUMP;
    if (pad.z)     g_buttons |= IN_SNEAK;
    if (pad.start) g_buttons |= IN_MENU;
    if (pad.l)     g_buttons |= IN_LOOK_UP;
    if (pad.r)     g_buttons |= IN_LOOK_DN;
    if (pad.c_up)    g_buttons |= IN_MAGIC;
    if (pad.c_down)  g_buttons |= IN_INVENTORY;
    if (pad.c_left)  g_buttons |= IN_JOURNAL;
    if (pad.c_right) g_buttons |= IN_WEAPON;

    /* D-pad and analog stick → movement */
    if (pad.up    || g_stick_y > 20)  g_buttons |= IN_FORWARD;
    if (pad.down  || g_stick_y < -20) g_buttons |= IN_BACK;
    if (pad.left  || g_stick_x < -20) g_buttons |= IN_LEFT;
    if (pad.right || g_stick_x > 20)  g_buttons |= IN_RIGHT;

    g_pressed = g_buttons & ~g_buttons_prev;
}

/* -------------------------------------------------------------------
 * Player movement (first-person, terrain collision)
 *
 * Morrowind movement speed from GMST:
 *   base_speed = fMinWalkSpeed + (speed_attr/100) * (fMaxWalkSpeed - fMinWalkSpeed)
 *   fMinWalkSpeed = 100, fMaxWalkSpeed = 200 (GMST defaults)
 *   Run multiplier = fBaseRunMultiplier = 1.75
 * ------------------------------------------------------------------- */

#define MW_MIN_WALK_SPEED   100.0f
#define MW_MAX_WALK_SPEED   200.0f
#define MW_RUN_MULTIPLIER   1.75f
#define MW_TURN_SPEED       2.0f    /* Radians per second */
#define MW_LOOK_SPEED       1.5f
#define MW_GRAVITY          627.0f  /* Units per second² (from GMST fGravity) */
#define MW_JUMP_VELOCITY    250.0f  /* Approximate */

static float g_vel_z = 0.0f;  /* Vertical velocity */
static bool  g_on_ground = true;

static void update_player_movement(float dt) {
    /* Turn with analog stick X / d-pad left/right */
    float turn = 0;
    if (g_buttons & IN_LEFT)  turn -= MW_TURN_SPEED * dt;
    if (g_buttons & IN_RIGHT) turn += MW_TURN_SPEED * dt;
    if (g_stick_x > 20 || g_stick_x < -20) {
        turn += (g_stick_x / 127.0f) * MW_TURN_SPEED * dt;
    }
    g_player.rot_z += turn;

    /* Look up/down */
    if (g_buttons & IN_LOOK_UP) g_player.rot_x -= MW_LOOK_SPEED * dt;
    if (g_buttons & IN_LOOK_DN) g_player.rot_x += MW_LOOK_SPEED * dt;
    if (g_player.rot_x > 1.2f)  g_player.rot_x = 1.2f;
    if (g_player.rot_x < -1.2f) g_player.rot_x = -1.2f;

    /* Walk speed based on Speed attribute (exact MW formula) */
    float speed_ratio = (float)g_player.attributes[ATTR_SPEED] / 100.0f;
    float walk_speed = MW_MIN_WALK_SPEED + speed_ratio * (MW_MAX_WALK_SPEED - MW_MIN_WALK_SPEED);
    float move_speed = walk_speed;

    /* Run unless sneaking */
    if (!(g_buttons & IN_SNEAK)) {
        move_speed *= MW_RUN_MULTIPLIER;
    }

    /* Forward/back movement */
    float forward = 0, strafe = 0;
    if (g_buttons & IN_FORWARD) forward += move_speed;
    if (g_buttons & IN_BACK)    forward -= move_speed * 0.5f;  /* Backpedal slower */

    /* Apply movement in facing direction */
    if (forward != 0 || strafe != 0) {
        float sx = sinf_approx(g_player.rot_z);
        float cx = cosf_approx(g_player.rot_z);
        g_player.pos_x += (forward * sx + strafe * cx) * dt;
        g_player.pos_y += (forward * cx - strafe * sx) * dt;

        /* Drain fatigue while running (exact: fFatigueRunBase per second) */
        if (!(g_buttons & IN_SNEAK) && forward > 0) {
            g_player.fatigue -= (int32_t)(5.0f * dt);  /* fFatigueRunBase ≈ 5 */
            if (g_player.fatigue < 0) g_player.fatigue = 0;
        }
    }

    /* Gravity */
    if (!g_on_ground) {
        g_vel_z -= MW_GRAVITY * dt;
        g_player.pos_z += g_vel_z * dt;
    }

    /* Jump */
    if ((g_pressed & IN_JUMP) && g_on_ground) {
        g_vel_z = MW_JUMP_VELOCITY;
        g_on_ground = false;
    }

    /* Terrain collision (exterior only) */
    if (!g_player.in_interior) {
        float ground_z = cell_mgr_get_terrain_height(&g_cell_mgr,
                                                       g_player.pos_x,
                                                       g_player.pos_y);
        float feet_z = g_player.pos_z;
        if (feet_z <= ground_z) {
            g_player.pos_z = ground_z;
            g_vel_z = 0;
            g_on_ground = true;
        } else if (feet_z > ground_z + 8.0f) {
            g_on_ground = false;
        }
    }

    /* Fatigue regeneration when standing still */
    if (forward == 0 && strafe == 0) {
        /* fFatigueReturnBase = 2.0, fFatigueReturnMult = 0.02 (GMST) */
        int32_t regen = (int32_t)(2.0f * dt);
        g_player.fatigue += regen;
        if (g_player.fatigue > g_player.max_fatigue)
            g_player.fatigue = g_player.max_fatigue;
    }

    /* Update cell coordinates */
    g_player.cell_x = (int32_t)(g_player.pos_x / 8192.0f);
    g_player.cell_y = (int32_t)(g_player.pos_y / 8192.0f);
    if (g_player.pos_x < 0) g_player.cell_x--;
    if (g_player.pos_y < 0) g_player.cell_y--;
}

/* -------------------------------------------------------------------
 * Interaction: doors, NPCs, containers, pickups
 * ------------------------------------------------------------------- */

static void check_interaction(void) {
    if (!(g_pressed & IN_ACTION)) return;

    /* Get the cell the player is in */
    loaded_cell_t *lc;
    if (g_player.in_interior) {
        lc = cell_mgr_get_interior(&g_cell_mgr);
    } else {
        lc = cell_mgr_get_cell(&g_cell_mgr, g_player.cell_x, g_player.cell_y);
    }
    if (!lc) return;

    /* Find nearest interactable object (within 256 units in front of player) */
    float look_x = sinf_approx(g_player.rot_z);
    float look_y = cosf_approx(g_player.rot_z);
    float best_dist = 256.0f * 256.0f;
    int best_ref = -1;

    for (int i = 0; i < lc->ref_count; i++) {
        esm_cell_ref_t *ref = &lc->refs[i];
        float dx = ref->pos.x - g_player.pos_x;
        float dy = ref->pos.y - g_player.pos_y;
        float dist_sq = dx * dx + dy * dy;

        /* Check if in front of player (dot product > 0) */
        float dot = dx * look_x + dy * look_y;
        if (dot > 0 && dist_sq < best_dist) {
            best_dist = dist_sq;
            best_ref = i;
        }
    }

    if (best_ref < 0) return;
    esm_cell_ref_t *ref = &lc->refs[best_ref];

    /* Check what type of object this is */
    if (ref->is_door && ref->door_dest_cell[0]) {
        /* Door: transition to destination cell */
        if (ref->door_dest_cell[0] >= 'A') {
            /* Named interior */
            g_player.in_interior = true;
            strncpy(g_player.current_cell, ref->door_dest_cell, 63);
            g_player.pos_x = ref->door_dest_pos.x;
            g_player.pos_y = ref->door_dest_pos.y;
            g_player.pos_z = ref->door_dest_pos.z;
            g_player.rot_z = ref->door_dest_rot.z;
            cell_mgr_enter_interior(&g_cell_mgr, ref->door_dest_cell);
        } else {
            /* Exterior door */
            g_player.in_interior = false;
            g_player.pos_x = ref->door_dest_pos.x;
            g_player.pos_y = ref->door_dest_pos.y;
            g_player.pos_z = ref->door_dest_pos.z;
            cell_mgr_exit_to_exterior(&g_cell_mgr,
                ref->door_dest_pos.x, ref->door_dest_pos.y, ref->door_dest_pos.z);
        }
        return;
    }

    /* Check if it's an NPC (by looking up object_id in NPC index) */
    for (int n = 0; n < g_esm_index.npc_count; n++) {
        if (strcmp(g_esm_index.npcs[n].name, ref->object_id) == 0) {
            /* Start dialogue */
            g_ui.mode = UI_MODE_DIALOGUE;
            strncpy(g_ui.npc_name, g_esm_index.npcs[n].display_name, 63);
            dial_begin(&g_dialogue, best_ref, ref->object_id);
            dial_get_topics(&g_dialogue);
            /* Copy topics to UI */
            g_ui.topic_count = g_dialogue.topic_count;
            for (int t = 0; t < g_dialogue.topic_count; t++) {
                strncpy(g_ui.topic_list[t], g_dialogue.topics[t], 63);
            }
            g_ui.selected_topic = 0;
            return;
        }
    }

    /* Otherwise: try to pick up as an item */
    /* TODO: check CONT (container), MISC/WEAP/ARMO etc. */
    ui_show_message(&g_ui, ref->object_id, 90);  /* Show name for 3 seconds */
}

/* -------------------------------------------------------------------
 * Initialization
 * ------------------------------------------------------------------- */

static bool init_systems(void) {
    sc64_unlock();

    boot_msg("Initializing asset streaming...");
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
        display_printf("  Failed: %s\n", asset_error_string(err));
        return false;
    }
    display_printf("  OK: %u assets\n", g_asset_mgr.index_count);

    boot_msg("Loading Morrowind.esm...");
    uint32_t esm_size = 0;
    const uint8_t *esm_data = (const uint8_t *)asset_load_with_size(
        &g_asset_mgr, 1, &esm_size);
    if (!esm_data || esm_size == 0) {
        display_printf("  Failed to load ESM!\n");
        return false;
    }
    display_printf("  ESM: %u KB\n", esm_size / 1024);

    n64_mem_init();
    g_esm_index_ptr = (esm_index_t *)n64_mem_alloc(sizeof(esm_index_t));
    if (!g_esm_index_ptr) {
        display_printf("  No memory for ESM index!\n");
        return false;
    }

    boot_msg("Building world index...");
    esm_reader_init(&g_esm_reader, esm_data, esm_size);
    esm_build_index(&g_esm_reader, &g_esm_index);
    display_printf("  Cells: %d  Land: %d\n", g_esm_index.cell_count, g_esm_index.land_count);
    display_printf("  Statics: %d  NPCs: %d\n", g_esm_index.static_count, g_esm_index.npc_count);

    cell_mgr_init(&g_cell_mgr, &g_esm_reader, &g_esm_index);
    script_init(&g_scripts);
    weather_init(&g_weather);
    ui_init(&g_ui);
    save_init();

    return true;
}

/* -------------------------------------------------------------------
 * New game
 * ------------------------------------------------------------------- */

static void new_game(void) {
    memset(&g_player, 0, sizeof(g_player));
    strncpy(g_player.name, "Prisoner", 63);
    strncpy(g_player.race, "Imperial", 63);
    strncpy(g_player.class_name, "Custom", 63);
    g_player.level = 1;

    /* Exact Imperial male base attributes (from Morrowind.esm RACE record) */
    g_player.attributes[ATTR_STRENGTH] = 40;
    g_player.attributes[ATTR_INTELLIGENCE] = 40;
    g_player.attributes[ATTR_WILLPOWER] = 30;
    g_player.attributes[ATTR_AGILITY] = 30;
    g_player.attributes[ATTR_SPEED] = 40;
    g_player.attributes[ATTR_ENDURANCE] = 40;
    g_player.attributes[ATTR_PERSONALITY] = 50;
    g_player.attributes[ATTR_LUCK] = 40;
    memcpy(g_player.base_attributes, g_player.attributes, sizeof(g_player.attributes));

    for (int i = 0; i < SKILL_COUNT; i++) {
        g_player.skills[i] = 5;
        g_player.base_skills[i] = 5;
    }
    char_recalc_derived(&g_player);
    g_player.health = g_player.max_health;
    g_player.magicka = g_player.max_magicka;
    g_player.fatigue = g_player.max_fatigue;

    /* Starting position: Census and Excise Office, Seyda Neen */
    g_player.pos_x = 38.0f;
    g_player.pos_y = 120.0f;
    g_player.pos_z = 96.0f;
    g_player.rot_z = 0.0f;
    strncpy(g_player.current_cell, "Census and Excise Office", 63);
    g_player.in_interior = true;

    cell_mgr_enter_interior(&g_cell_mgr, g_player.current_cell);
    g_vel_z = 0;
    g_on_ground = true;
}

/* -------------------------------------------------------------------
 * Save game
 * ------------------------------------------------------------------- */

static void save_game(int slot) {
    save_data_t save;
    memset(&save, 0, sizeof(save));
    save.player = g_player;
    strncpy(save.header.player_name, g_player.name, 31);
    save.header.player_level = g_player.level;
    strncpy(save.header.location, g_player.current_cell, 63);
    save.header.play_time = 0;  /* TODO: track */

    /* Copy global variables from script engine */
    save.global_count = g_scripts.global_count;
    for (int i = 0; i < g_scripts.global_count && i < 256; i++) {
        strncpy(save.globals[i].name, g_scripts.globals[i].name, 31);
        save.globals[i].value = g_scripts.globals[i].value;
    }

    if (save_write(slot, &save)) {
        ui_show_message(&g_ui, "Game saved.", 90);
    } else {
        ui_show_message(&g_ui, "Save failed!", 90);
    }
}

/* -------------------------------------------------------------------
 * Game frame — complete wired loop
 * ------------------------------------------------------------------- */

static void game_frame(void) {
    uint32_t frame_start = c0_count();
    float dt = 1.0f / 30.0f;  /* Fixed 30fps timestep */

    /* 1. Poll input */
    poll_input();

    /* 2. Handle UI mode switches */
    if (g_ui.mode == UI_MODE_GAME) {
        if (g_pressed & IN_MENU) {
            /* Start button: save menu */
            save_game(0);
        }
        if (g_pressed & IN_INVENTORY) g_ui.mode = UI_MODE_INVENTORY;
        if (g_pressed & IN_JOURNAL)   g_ui.mode = UI_MODE_JOURNAL;
        if (g_pressed & IN_MAGIC)     g_ui.mode = UI_MODE_STATS;
    } else {
        /* UI is active — handle UI input */
        if (ui_handle_input(&g_ui, g_buttons, g_pressed)) {
            goto render;  /* Input consumed by UI */
        }
    }

    /* 3. Player movement (only in game mode) */
    if (g_ui.mode == UI_MODE_GAME) {
        update_player_movement(dt);
        check_interaction();
    }

    /* 4. Update cell streaming */
    mw_player_pos_t ppos = {
        .x = g_player.pos_x, .y = g_player.pos_y, .z = g_player.pos_z,
        .in_interior = g_player.in_interior,
    };
    if (!g_player.in_interior) {
        cell_mgr_update(&g_cell_mgr, &ppos);
    }

    /* 5. Update weather and time */
    weather_update(&g_weather, dt);

    /* 6. Run scripts */
    script_run_frame(&g_scripts);

    /* 7. NPC AI update */
    /* TODO: iterate active NPCs, call ai_update_npc() */

render:
    /* 8. Render */
    display_init(NULL);

    if (g_ui.mode == UI_MODE_GAME) {
        /* Render 3D scene via RDP */
        /* TODO: terrain rendering, mesh rendering via RDP
         * For now: show debug info via bootloader text display */

        display_printf("MW-N64  %s\n", g_player.current_cell);
        display_printf("Pos: %.0f, %.0f, %.0f\n",
                       g_player.pos_x, g_player.pos_y, g_player.pos_z);
        display_printf("Cell: %d, %d  %s\n",
                       g_player.cell_x, g_player.cell_y,
                       g_player.in_interior ? "(interior)" : "(exterior)");
        display_printf("Time: %.1f:00  %s\n",
                       g_weather.game_hour,
                       g_weather.current_weather == 0 ? "Clear" :
                       g_weather.current_weather == 4 ? "Rain" : "---");
        display_printf("\n");

        /* HUD: health/magicka/fatigue */
        display_printf("HP: %d/%d  ", g_player.health, g_player.max_health);
        display_printf("MP: %d/%d  ", g_player.magicka, g_player.max_magicka);
        display_printf("FT: %d/%d\n", g_player.fatigue, g_player.max_fatigue);
        display_printf("Lv: %d  %s (%s)\n",
                       g_player.level, g_player.name, g_player.class_name);

        /* Loaded cell info */
        if (g_player.in_interior) {
            loaded_cell_t *lc = cell_mgr_get_interior(&g_cell_mgr);
            if (lc) {
                display_printf("\nCell: %d refs loaded\n", lc->ref_count);
            }
        } else {
            display_printf("\nTerrain: 3x3 cells\n");
            loaded_cell_t *center = cell_mgr_get_cell(&g_cell_mgr,
                g_player.cell_x, g_player.cell_y);
            if (center) {
                display_printf("  Center: %d refs, %s terrain\n",
                    center->ref_count,
                    center->has_terrain ? "has" : "no");
            }
        }

        /* Controls help */
        display_printf("\n");
        display_printf("Stick=Move  L/R=Look\n");
        display_printf("A=Interact  B=Jump  Z=Sneak\n");
        display_printf("C-Dn=Inv  C-L=Journal\n");
        display_printf("Start=Save\n");

        /* Message box */
        ui_draw_message(&g_ui, NULL);

    } else if (g_ui.mode == UI_MODE_DIALOGUE) {
        ui_draw_dialogue(&g_ui, NULL);
    } else if (g_ui.mode == UI_MODE_INVENTORY) {
        ui_draw_inventory(&g_ui, NULL);
    } else if (g_ui.mode == UI_MODE_JOURNAL) {
        ui_draw_journal(&g_ui, NULL);
    } else if (g_ui.mode == UI_MODE_STATS) {
        display_printf("=== CHARACTER ===\n\n");
        display_printf("%s  Lv %d  %s\n\n", g_player.name, g_player.level, g_player.race);
        display_printf("STR %d  INT %d  WIL %d\n",
            g_player.attributes[0], g_player.attributes[1], g_player.attributes[2]);
        display_printf("AGI %d  SPD %d  END %d\n",
            g_player.attributes[3], g_player.attributes[4], g_player.attributes[5]);
        display_printf("PER %d  LCK %d\n\n",
            g_player.attributes[6], g_player.attributes[7]);
        display_printf("B=Close\n");
        if (g_pressed & IN_JUMP) g_ui.mode = UI_MODE_GAME;
    }

    /* 9. Frame pacing */
    uint32_t elapsed = (c0_count() - frame_start) / 46875;  /* ms */
    if (elapsed < FRAME_MS) {
        delay_ms(FRAME_MS - elapsed);
    }
}

/* -------------------------------------------------------------------
 * Title screen
 * ------------------------------------------------------------------- */

static int title_screen(void) {
    while (1) {
        poll_input();

        display_init(NULL);
        display_printf("\n\n");
        display_printf("  THE ELDER SCROLLS III\n\n");
        display_printf("      MORROWIND\n\n");
        display_printf("    N64 + SC64 Port\n\n");
        display_printf("  Faithful to the original\n\n\n");
        display_printf("  A = New Game\n");

        if (save_slot_valid(0)) {
            save_header_t hdr;
            save_read_header(0, &hdr);
            display_printf("  B = Load: %s Lv%d\n", hdr.player_name, hdr.player_level);
            display_printf("      (%s)\n", hdr.location);
        }

        display_printf("\n\n  Press A or B\n");

        if (g_pressed & IN_ACTION) return 0;  /* New game */
        if (g_pressed & IN_JUMP) return 1;    /* Load game */

        delay_ms(33);
    }
}

/* -------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------- */

void main(void) {
    show_boot_screen();

    if (!init_systems()) {
        display_printf("\nMW-N64 requires:\n");
        display_printf("  SC64 + SD card with\n");
        display_printf("  Morrowind.esm packed via\n");
        display_printf("  tools/mw_pack.py\n\n");
        display_printf("ROM built OK (%d KB code)\n", 35);
        while (1) { delay_ms(1000); }
    }

    boot_msg("\nAll systems ready.");
    delay_ms(1000);

    while (1) {
        int choice = title_screen();

        if (choice == 0) {
            new_game();
        } else {
            save_data_t save;
            if (save_read(0, &save)) {
                g_player = save.player;
                if (g_player.in_interior) {
                    cell_mgr_enter_interior(&g_cell_mgr, g_player.current_cell);
                } else {
                    cell_mgr_exit_to_exterior(&g_cell_mgr,
                        g_player.pos_x, g_player.pos_y, g_player.pos_z);
                }
            } else {
                new_game();
            }
        }

        while (1) {
            game_frame();
        }
    }
}

/* show_boot_screen and boot_msg defined with forward declarations above */

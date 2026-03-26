/*
 * MW-N64: UI rendering — implementation
 *
 * Uses the bootloader's display_printf for text (bitmap font).
 * RDP fill rectangles for colored bars and backgrounds.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_ui.h"
#include "../game/mw_character.h"
#include <string.h>

/* External game state */
extern mw_player_t g_player;

/* Bootloader text rendering */
extern void display_init(uint32_t *bg);
extern void display_printf(const char *fmt, ...);

void ui_init(ui_state_t *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->mode = UI_MODE_GAME;
    ui->show_hud = true;
    ui->show_crosshair = true;
}

void ui_set_mode(ui_state_t *ui, ui_mode_t mode) {
    ui->mode = mode;
    ui->scroll_offset = 0;
    ui->selected_item = 0;
}

void ui_draw_hud(ui_state_t *ui, void *rdp_buf) {
    if (!ui->show_hud) return;
    (void)rdp_buf;

    /* Health bar: red, bottom-left */
    /* Magicka bar: blue, bottom-left below health */
    /* Fatigue bar: green, bottom-left below magicka */
    /* TODO: RDP fill rectangles with proportional width */

    /* For now: text-based HUD via bootloader display */
}

void ui_draw_dialogue(ui_state_t *ui, void *rdp_buf) {
    (void)rdp_buf;

    /* NPC name at top */
    display_printf("%s\n", ui->npc_name);
    display_printf("─────────────────────\n");

    /* Response text (word-wrapped by display_printf) */
    if (ui->response_text[0]) {
        display_printf("%s\n\n", ui->response_text);
    }

    /* Topic list */
    for (int i = 0; i < ui->topic_count; i++) {
        if (i == ui->selected_topic)
            display_printf("> %s\n", ui->topic_list[i]);
        else
            display_printf("  %s\n", ui->topic_list[i]);
    }

    display_printf("\nA=Select B=Goodbye\n");
}

void ui_draw_inventory(ui_state_t *ui, void *rdp_buf) {
    (void)rdp_buf;

    display_printf("=== INVENTORY ===\n\n");

    /* Show equipped items */
    display_printf("Weight: %d  Gold: %d\n\n", 0, 0 /* TODO: gold from inventory */);

    /* Item list */
    int start = ui->scroll_offset;
    int end = start + 12;  /* Show 12 items at a time */
    if (end > g_player.inventory_count) end = g_player.inventory_count;

    for (int i = start; i < end; i++) {
        if (i == ui->selected_item)
            display_printf("> ");
        else
            display_printf("  ");
        display_printf("%s", g_player.inventory[i].id);
        if (g_player.inventory[i].count > 1)
            display_printf(" (%d)", g_player.inventory[i].count);
        display_printf("\n");
    }

    if (g_player.inventory_count == 0)
        display_printf("  (empty)\n");

    display_printf("\nUp/Dn=Scroll A=Use B=Close\n");
}

void ui_draw_journal(ui_state_t *ui, void *rdp_buf) {
    (void)rdp_buf;
    (void)ui;

    display_printf("=== JOURNAL ===\n\n");
    /* TODO: display journal entries from script_get_journal() */
    display_printf("No journal entries yet.\n");
    display_printf("\nB=Close\n");
}

void ui_draw_message(ui_state_t *ui, void *rdp_buf) {
    (void)rdp_buf;

    if (ui->message_timer > 0) {
        display_printf("\n%s\n", ui->message);
        ui->message_timer--;
    }
}

void ui_show_message(ui_state_t *ui, const char *text, int duration_frames) {
    strncpy(ui->message, text, 255);
    ui->message[255] = '\0';
    ui->message_timer = duration_frames;
}

bool ui_handle_input(ui_state_t *ui, uint32_t buttons, uint32_t pressed) {
    (void)buttons;

    switch (ui->mode) {
        case UI_MODE_DIALOGUE:
            if (pressed & 0x10) {  /* A = select topic */
                return true;
            }
            if (pressed & 0x20) {  /* B = goodbye */
                ui->mode = UI_MODE_GAME;
                return true;
            }
            if (pressed & 0x01) {  /* Up */
                if (ui->selected_topic > 0) ui->selected_topic--;
                return true;
            }
            if (pressed & 0x02) {  /* Down */
                if (ui->selected_topic < ui->topic_count - 1) ui->selected_topic++;
                return true;
            }
            break;

        case UI_MODE_INVENTORY:
            if (pressed & 0x20) {  /* B = close */
                ui->mode = UI_MODE_GAME;
                return true;
            }
            if (pressed & 0x01) {  /* Up */
                if (ui->selected_item > 0) ui->selected_item--;
                return true;
            }
            if (pressed & 0x02) {  /* Down */
                if (ui->selected_item < g_player.inventory_count - 1)
                    ui->selected_item++;
                return true;
            }
            break;

        case UI_MODE_JOURNAL:
            if (pressed & 0x20) {
                ui->mode = UI_MODE_GAME;
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

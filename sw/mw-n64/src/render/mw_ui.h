/*
 * MW-N64: User interface rendering
 *
 * Morrowind UI: dialogue box, inventory grid, journal pages,
 * health/magicka/fatigue bars, minimap, crosshair.
 *
 * Rendered as RDP rectangles and text overlays on the framebuffer.
 * No 3D UI elements — all 2D.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_UI_H
#define MW_UI_H

#include <stdint.h>
#include <stdbool.h>

/* UI screen modes */
typedef enum {
    UI_MODE_GAME = 0,       /* Normal gameplay HUD */
    UI_MODE_DIALOGUE = 1,   /* Talking to NPC */
    UI_MODE_INVENTORY = 2,  /* Inventory screen */
    UI_MODE_JOURNAL = 3,    /* Journal / quest log */
    UI_MODE_MAP = 4,        /* World map */
    UI_MODE_STATS = 5,      /* Character stats */
    UI_MODE_MAGIC = 6,      /* Spell selection */
    UI_MODE_REST = 7,       /* Rest / wait menu */
    UI_MODE_LEVELUP = 8,    /* Level up screen */
    UI_MODE_CHARGEN = 9,    /* Character creation */
} ui_mode_t;

/* UI state */
typedef struct {
    ui_mode_t mode;
    int cursor_x, cursor_y;    /* Menu cursor position */
    int scroll_offset;          /* For scrollable lists */
    int selected_item;          /* Currently selected item index */

    /* Dialogue UI */
    char npc_name[64];
    char response_text[4096];
    char topic_list[32][64];
    int  topic_count;
    int  selected_topic;

    /* Inventory UI */
    int  inv_category;          /* 0=all, 1=weapons, 2=armor, 3=misc */

    /* Message box */
    char message[256];
    int  message_timer;         /* Frames remaining */

    /* HUD */
    bool show_hud;
    bool show_crosshair;
} ui_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

void ui_init(ui_state_t *ui);
void ui_set_mode(ui_state_t *ui, ui_mode_t mode);

/* Draw HUD overlay (health/magicka/fatigue bars, minimap) */
void ui_draw_hud(ui_state_t *ui, void *rdp_buf);

/* Draw dialogue screen */
void ui_draw_dialogue(ui_state_t *ui, void *rdp_buf);

/* Draw inventory screen */
void ui_draw_inventory(ui_state_t *ui, void *rdp_buf);

/* Draw journal */
void ui_draw_journal(ui_state_t *ui, void *rdp_buf);

/* Draw message box */
void ui_draw_message(ui_state_t *ui, void *rdp_buf);

/* Show a timed message */
void ui_show_message(ui_state_t *ui, const char *text, int duration_frames);

/* Process input for current UI mode. Returns true if input was consumed. */
bool ui_handle_input(ui_state_t *ui, uint32_t buttons, uint32_t pressed);

#endif /* MW_UI_H */

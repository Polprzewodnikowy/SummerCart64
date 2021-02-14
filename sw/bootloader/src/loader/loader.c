#include <libdragon.h>

#include "platform.h"
#include "assets/assets.h"

#include "loader.h"


#define X_OFFSET        (24)
#define Y_OFFSET        (16)
#define Y_PADDING       (12)

#define LAST_CHAR(s)    (sizeof(s) - 1)
#define EDIT_CHAR(s, n) s[(n) >= 0 ? 0 : LAST_CHAR((s)) + (n)]
#define NTOA(n)         ('0' + ((n) %  10))


static bool loader_initialized = false;

static char version_string[] = "SC64 Bootloader ver. X.X";
static char error_number_string[] = "ERROR X";
static const char *error_strings[] = {    
    "No error :O",
    "SummerCart64 not detected",
    "SD Card not detected",
    "No filesystem (FAT or exFAT)\nfound on SD Card",
    "Unable to locate menu file",
    "Error while reading data from\nSD Card",
    "Unknown error",
};

static int x_offset = X_OFFSET;
static int y_offset = Y_OFFSET;


static display_context_t loader_get_display(bool lock) {
    display_context_t display;

    do {
        display = display_lock();
    } while (!display && lock);

    x_offset = X_OFFSET;
    y_offset = Y_OFFSET;

    return display;
}

static void loader_draw_version_and_logo(display_context_t display) {
    EDIT_CHAR(version_string, -3) = NTOA(BOOTLOADER_VERSION_MAJOR);
    EDIT_CHAR(version_string, -1) = NTOA(BOOTLOADER_VERSION_MINOR);
    graphics_draw_text(display, x_offset, y_offset, version_string);
    y_offset += Y_PADDING;

    sprite_t *logo = _binary_build_sc64_logo_sprite_start;

    int center_x = (320 / 2) - (logo->width / 2);
    int center_y = (240 / 2) - (logo->height / 2);

    graphics_draw_sprite(display, center_x, center_y, logo);
}

static void loader_init(void) {
    init_interrupts();

    audio_init(44100, 2);
    audio_write_silence();

    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_OFF);

    loader_initialized = true;
}

void loader_cleanup(void) {
    audio_close();

    display_close();

    disable_interrupts();

    loader_initialized = false;
}

void loader_display_logo(void) {
    if (!loader_initialized) {
        loader_init();
    }

    display_context_t display;

    display = loader_get_display(true);

    graphics_fill_screen(display, 0);

    loader_draw_version_and_logo(display);

    display_show(display);
}

void loader_display_message(const char *message) {
    if (!loader_initialized) {
        loader_init();
    }

    display_context_t display;

    display = loader_get_display(true);

    graphics_fill_screen(display, 0);

    loader_draw_version_and_logo(display);

    graphics_draw_text(display, x_offset, y_offset, message);

    display_show(display);
}

void loader_display_error_and_halt(menu_load_error_t error, const char *message) {
    if (!loader_initialized) {
        loader_init();
    }

    display_context_t display;

    display = loader_get_display(true);

    graphics_fill_screen(display, 0);

    loader_draw_version_and_logo(display);

    EDIT_CHAR(error_number_string, -1) = NTOA(error);
    graphics_draw_text(display, x_offset, y_offset, error_number_string);
    y_offset += Y_PADDING;

    int error_string_index = error >= E_MENU_END ? (E_MENU_END - 1) : error;
    const char *error_string = error_strings[error_string_index];
    graphics_draw_text(display, x_offset, y_offset, error_string);
    y_offset += Y_PADDING * 2;

    graphics_draw_text(display, x_offset, y_offset, message);

    display_show(display);

    while (1);
}

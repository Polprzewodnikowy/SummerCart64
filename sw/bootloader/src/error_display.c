#include <stdio.h>

#include "error_display.h"


void error_display_and_halt(menu_load_error_t error, const char *path) {
    init_interrupts();

    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);

    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_clear();

    printf("Bootloader error:\n\n%2d: ", error);

    switch (error) {
        case E_MENU_OK:
            printf("No error :O");
            break;
        case E_MENU_NOT_SC64:
            printf("SummerCart64 not detected");
            break;
        case E_MENU_ERROR_NO_OR_BAD_SD_CARD:
            printf("SD Card not detected or bad");
            break;
        case E_MENU_ERROR_NO_FILESYSTEM:
            printf("No filesystem (FAT or exFAT)\nfound on SD Card");
            break;
        case E_MENU_ERROR_NO_FILE_TO_LOAD:
            printf("Unable to locate menu file:\n(%s)", path);
            break;
        case E_MENU_ERROR_BAD_FILE:
            printf("Menu file is corrupted");
            break;
        case E_MENU_ERROR_READ_EXCEPTION:
            printf("Error while reading data from\nSD Card");
            break;
        case E_MENU_ERROR_OTHER:
        default:
            printf("Unknown error");
            break;
    }

    console_render();

    while (1);
}

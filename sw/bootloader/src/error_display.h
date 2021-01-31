#ifndef ERROR_DISPLAY_H__
#define ERROR_DISPLAY_H__


#include "platform.h"


typedef enum menu_load_error_e {
    E_MENU_OK,
    E_MENU_ERROR_NOT_SC64,
    E_MENU_ERROR_NO_CARD,
    E_MENU_ERROR_NO_FILESYSTEM,
    E_MENU_ERROR_NO_FILE,
    E_MENU_ERROR_READ_ERROR,
    E_MENU_ERROR_OTHER_ERROR,
} menu_load_error_t;


void error_display_and_halt(menu_load_error_t error, const char *path);


#endif

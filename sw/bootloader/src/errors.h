#ifndef ERRORS_H__
#define ERRORS_H__


typedef enum menu_load_error_e {
    E_MENU_OK,
    E_MENU_ERROR_NOT_SC64,
    E_MENU_ERROR_NO_CARD,
    E_MENU_ERROR_NO_FILESYSTEM,
    E_MENU_ERROR_NO_FILE,
    E_MENU_ERROR_READ_ERROR,
    E_MENU_ERROR_OTHER_ERROR,
    E_MENU_END,
} menu_load_error_t;


#endif

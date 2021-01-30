#ifndef SC64_SD_FS_H__
#define SC64_SD_FS_H__


#include "platform.h"


typedef enum sc64_sd_fs_error_e {
    SC64_SD_FS_OK,
    SC64_SD_FS_NO_CARD,
    SC64_SD_FS_NO_FILESYSTEM,
    SC64_SD_FS_NO_FILE,
    SC64_SD_FS_READ_ERROR,
    SC64_SD_FS_OTHER_ERROR,
} sc64_sd_fs_error_t;


sc64_sd_fs_error_t sc64_sd_fs_load_rom(const char *path);


#endif

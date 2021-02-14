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
    SC64_SD_FS_WRITE_ERROR,
} sc64_sd_fs_error_t;


typedef struct sc64_sd_fs_config_s {
    char *rom;
    char *save;
    uint8_t save_type;
    bool save_writeback;
    bool rom_reload;
    uint16_t cic_seed;
    tv_type_t tv_type;
} sc64_sd_fs_config_t;


sc64_sd_fs_error_t sc64_sd_fs_init(void);
void sc64_sd_fs_deinit(void);
sc64_sd_fs_error_t sc64_sd_fs_load_config(const char *path, sc64_sd_fs_config_t *config);
sc64_sd_fs_error_t sc64_sd_fs_load_rom(const char *path);
sc64_sd_fs_error_t sc64_sd_fs_load_save(const char *path);
sc64_sd_fs_error_t sc64_sd_fs_store_save(const char *path);


#endif

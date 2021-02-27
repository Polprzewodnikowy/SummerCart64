#include "ff.h"
#include "diskio.h"

#include "sc64.h"
#include "sc64_sd.h"
#include "sc64_sd_fs.h"

#include <string.h>
#include <stdlib.h>


static uint8_t current_bank = SC64_BANK_INVALID;
static uint32_t current_offset = 0;
static uint8_t save_buffer[128 * 1024] __attribute__((aligned(16)));
static bool fs_initialized = false;
static FATFS fatfs;


static DRESULT sc64_sd_fs_load_with_dma(BYTE pdrv, FSIZE_t offset, LBA_t sector, UINT count) {
    sc64_sd_err_t error;

    if (pdrv > 0) {
        return RES_PARERR;
    }

    error = sc64_sd_sectors_read_dma(sector, count, current_bank, current_offset + offset);
    
    if (error != E_OK) {
        switch (error) {
            case E_NO_INIT:
                return RES_NOTRDY;
            case E_PAR_ERROR:
                return RES_PARERR;
            default:
                return RES_ERROR;
        }
    }

    return RES_OK;
}


sc64_sd_fs_error_t sc64_sd_fs_init(void) {
    FRESULT fresult;

    fresult = f_mount(&fatfs, "", 1);
    if (fresult != FR_OK) {
        switch (fresult) {
            case FR_DISK_ERR:
                return SC64_SD_FS_READ_ERROR;
            case FR_NOT_READY:
                return SC64_SD_FS_NO_CARD;
            case FR_NO_FILESYSTEM:
                return SC64_SD_FS_NO_FILESYSTEM;
            default:
                return SC64_SD_FS_OTHER_ERROR;
        }
    }

    fs_initialized = true;

    return SC64_SD_FS_OK;
}

void sc64_sd_fs_deinit(void) {
    if (fs_initialized) {
        f_unmount("");
    }

    sc64_sd_deinit();
}

sc64_sd_fs_error_t sc64_sd_fs_load_config(const char *path, sc64_sd_fs_config_t *config) {
    FRESULT fresult;
    FIL fil;
    char config_buffer[256];

    fresult = f_open(&fil, path, FA_READ);
    if (fresult != FR_OK) {
        switch (fresult) {
            case FR_DISK_ERR:
            case FR_NOT_READY:
                return SC64_SD_FS_READ_ERROR;
            case FR_NO_FILE:
            case FR_NO_PATH:
                return SC64_SD_FS_NO_FILE;
            default:
                return SC64_SD_FS_OTHER_ERROR;
        }
    }

    while (!f_eof(&fil)) {
        char *line = f_gets(config_buffer, sizeof(config_buffer), &fil);

        if (line == NULL) {
            break;
        }

        if (strncmp("rom=", line, 4) == 0) {
            strncpy(config->rom, line + 4, sizeof(config_buffer) - 4);
        } else if (strncmp("rom_reload=", line, 11) == 0) {
            config->rom_reload = line[11] != '0';
        } else if (strncmp("save=", line, 5) == 0) {
            strncpy(config->save, line + 5, sizeof(config_buffer) - 5);
        } else if (strncmp("save_type=", line, 10) == 0) {
            config->save_type = (uint8_t) strtol(line + 10, NULL, 10);
        } else if (strncmp("save_writeback=", line, 15) == 0) {
            config->save_writeback = line[15] != '0';
        } else if (strncmp("cic_seed=", line, 9) == 0) {
            config->cic_seed = (uint16_t) strtoul(line + 9, NULL, 16);
        } else if (strncmp("tv_type=", line, 8) == 0) {
            config->tv_type = (tv_type_t) strtol(line + 8, NULL, 10);
        }
    }

    fresult = f_close(&fil);
    if (fresult != FR_OK) {
        return SC64_SD_FS_OTHER_ERROR;
    }

    return SC64_SD_FS_OK;
}

sc64_sd_fs_error_t sc64_sd_fs_load_rom(const char *path) {
    FRESULT fresult;

    current_bank = SC64_BANK_SDRAM;
    current_offset = 0;

    fresult = fe_load(path, SC64_SDRAM_SIZE, sc64_sd_fs_load_with_dma);
    if (fresult != FR_OK) {
        switch (fresult) {
            case FR_DISK_ERR:
            case FR_NOT_READY:
                return SC64_SD_FS_READ_ERROR;
            case FR_NO_FILE:
            case FR_NO_PATH:
                return SC64_SD_FS_NO_FILE;
            default:
                return SC64_SD_FS_OTHER_ERROR;
        }
    }

    return SC64_SD_FS_OK;
}

sc64_sd_fs_error_t sc64_sd_fs_load_save(const char *path) {
    FRESULT fresult;
    uint32_t scr;
    size_t length;

    scr = sc64_get_scr();
    length = 0;

    if (scr & SC64_CART_SCR_EEPROM_ENABLE) {
        length = (scr & SC64_CART_SCR_EEPROM_16K_MODE) ? 2048 : 512;
        current_bank = SC64_BANK_EEPROM;
        current_offset = 0;
    } else if (scr & (SC64_CART_SCR_SRAM_ENABLE | SC64_CART_SCR_FLASHRAM_ENABLE)) {
        length = 128 * 1024;
        current_bank = SC64_BANK_SDRAM;
        current_offset = sc64_get_save_address();
    }

    if ((length == 0) || (path == NULL)) {
        return SC64_SD_FS_OK;
    }

    fresult = fe_load(path, length, sc64_sd_fs_load_with_dma);
    if (fresult != FR_OK) {
        switch (fresult) {
            case FR_DISK_ERR:
            case FR_NOT_READY:
                return SC64_SD_FS_READ_ERROR;
            case FR_NO_FILE:
            case FR_NO_PATH:
                return SC64_SD_FS_NO_FILE;
            default:
                return SC64_SD_FS_OTHER_ERROR;
        }
    }

    return SC64_SD_FS_OK;
}

sc64_sd_fs_error_t sc64_sd_fs_store_save(const char *path) {
    FRESULT fresult;
    FIL fil;
    UINT written;
    uint32_t scr;
    size_t length;

    scr = sc64_get_scr();
    length = 0;

    if (scr & SC64_CART_SCR_EEPROM_ENABLE) {
        sc64_enable_eeprom_pi();
        length = (scr & SC64_CART_SCR_EEPROM_16K_MODE) ? 2048 : 512;
        platform_pi_dma_read(save_buffer, &SC64_EEPROM->MEM, length);
        platform_cache_invalidate(save_buffer, length);
        sc64_disable_eeprom_pi();
    } else if (scr & (SC64_CART_SCR_SRAM_ENABLE | SC64_CART_SCR_FLASHRAM_ENABLE)) {
        length = 128 * 1024;
        platform_pi_dma_read(save_buffer, sc64_get_save_address(), length);
        platform_cache_invalidate(save_buffer, length);
    }

    if ((length == 0) || (path == NULL)) {
        return SC64_SD_FS_OK;
    }

    fresult = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fresult != FR_OK) {
        return SC64_SD_FS_WRITE_ERROR;
    }

    fresult = f_write(&fil, save_buffer, length, &written);
    if (fresult != FR_OK) {
        return SC64_SD_FS_WRITE_ERROR;
    }

    fresult = f_close(&fil);
    if (fresult != FR_OK) {
        return SC64_SD_FS_WRITE_ERROR;
    }

    return SC64_SD_FS_OK;
}

#ifndef SC64_SD_H__
#define SC64_SD_H__


#include "platform.h"


typedef enum sc64_sd_err_e {
    E_OK,
    E_TIMEOUT,
    E_CRC_ERROR,
    E_BAD_INDEX,
    E_PAR_ERROR,
    E_FIFO_ERROR,
    E_NO_INIT,
} sc64_sd_err_t;


bool sc64_sd_init(void);
void sc64_sd_deinit(void);
bool sc64_sd_get_status(void);
sc64_sd_err_t sc64_sd_read_sectors(uint32_t starting_sector, size_t count, void *buffer);
sc64_sd_err_t sc64_sd_read_sectors_dma(uint32_t starting_sector, size_t count, uint8_t bank, uint32_t address);


#endif

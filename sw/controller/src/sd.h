#ifndef SD_H__
#define SD_H__


#include <stdbool.h>
#include <stdint.h>


#define SD_SECTOR_SIZE      (512)
#define SD_CARD_INFO_SIZE   (32)


typedef enum {
    SD_OK = 0,
    SD_ERROR_NO_CARD_IN_SLOT = 1,
    SD_ERROR_NOT_INITIALIZED = 2,
    SD_ERROR_INVALID_ARGUMENT = 3,
    SD_ERROR_INVALID_ADDRESS = 4,
    SD_ERROR_INVALID_OPERATION = 5,
    SD_ERROR_CMD2_IO = 6,
    SD_ERROR_CMD3_IO = 7,
    SD_ERROR_CMD6_CHECK_IO = 8,
    SD_ERROR_CMD6_CHECK_CRC = 9,
    SD_ERROR_CMD6_CHECK_TIMEOUT = 10,
    SD_ERROR_CMD6_CHECK_RESPONSE = 11,
    SD_ERROR_CMD6_SWITCH_IO = 12,
    SD_ERROR_CMD6_SWITCH_CRC = 13,
    SD_ERROR_CMD6_SWITCH_TIMEOUT = 14,
    SD_ERROR_CMD6_SWITCH_RESPONSE = 15,
    SD_ERROR_CMD7_IO = 16,
    SD_ERROR_CMD8_IO = 17,
    SD_ERROR_CMD9_IO = 18,
    SD_ERROR_CMD10_IO = 19,
    SD_ERROR_CMD18_IO = 20,
    SD_ERROR_CMD18_CRC = 21,
    SD_ERROR_CMD18_TIMEOUT = 22,
    SD_ERROR_CMD25_IO = 23,
    SD_ERROR_CMD25_CRC = 24,
    SD_ERROR_CMD25_TIMEOUT = 25,
    SD_ERROR_ACMD6_IO = 26,
    SD_ERROR_ACMD41_IO = 27,
    SD_ERROR_ACMD41_OCR = 28,
    SD_ERROR_ACMD41_TIMEOUT = 29,
} sd_error_t;

typedef sd_error_t sd_process_sectors_t (uint32_t address, uint32_t sector, uint32_t count);


sd_error_t sd_card_init (void);
void sd_card_deinit (void);
bool sd_card_is_inserted (void);
uint32_t sd_card_get_status (void);
sd_error_t sd_card_get_info (uint32_t address);
sd_error_t sd_set_byte_swap (bool enabled);

sd_error_t sd_write_sectors (uint32_t address, uint32_t sector, uint32_t count);
sd_error_t sd_read_sectors (uint32_t address, uint32_t sector, uint32_t count);

sd_error_t sd_optimize_sectors (uint32_t address, uint32_t *sector_table, uint32_t count, sd_process_sectors_t sd_process_sectors);

void sd_init (void);

void sd_process (void);


#endif

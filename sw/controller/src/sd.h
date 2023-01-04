#ifndef SD_H__
#define SD_H__


#include <stdbool.h>


#define SD_SECTOR_SIZE      (512)


typedef bool sd_process_sectors_t (uint32_t address, uint32_t sector, uint32_t count);


bool sd_card_init (void);
void sd_card_deinit (void);
uint32_t sd_card_get_status (void);
bool sd_card_get_info (uint32_t address);
bool sd_write_sectors (uint32_t address, uint32_t sector, uint32_t count);
bool sd_read_sectors (uint32_t address, uint32_t sector, uint32_t count);
bool sd_optimize_sectors (uint32_t address, uint32_t *sector_table, uint32_t count, sd_process_sectors_t sd_process_sectors);
void sd_init (void);
void sd_process (void);


#endif

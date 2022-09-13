#ifndef SD_H__
#define SD_H__


#include <stdbool.h>


bool sd_card_init (void);
void sd_card_deinit (void);
bool sd_card_get_status (void);
bool sd_card_get_info (uint32_t address);
bool sd_write_sectors (uint32_t address, uint32_t sector, uint32_t count);
bool sd_read_sectors (uint32_t address, uint32_t sector, uint32_t count);
void sd_init (void);
void sd_process (void);


#endif

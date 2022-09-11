#ifndef SD_H__
#define SD_H__


#include <stdbool.h>


bool sd_read_sectors (uint32_t address, uint32_t sector, uint32_t count);
bool sd_card_init (void);
void sd_card_deinit (void);
void sd_init (void);
void sd_process (void);


#endif

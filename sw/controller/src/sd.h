#ifndef SD_H__
#define SD_H__


#include <stdbool.h>


bool sd_read_sectors (uint32_t starting_sector, uint32_t address, uint32_t length);
bool sd_card_initialize (void);
void sd_init (void);
void sd_process (void);


#endif

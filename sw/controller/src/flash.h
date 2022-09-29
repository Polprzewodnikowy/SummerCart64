#ifndef FLASH_H__
#define FLASH_H__


#include <stdint.h>


#define FLASH_ERASE_BLOCK_SIZE  (128 * 1024)


void flash_wait_busy (void);
void flash_erase_block (uint32_t offset);


#endif

#ifndef FLASH_H__
#define FLASH_H__


#include <stdbool.h>
#include <stdint.h>


#define FLASH_ERASE_BLOCK_SIZE  (128 * 1024)


bool flash_program (uint32_t src, uint32_t dst, uint32_t length);
void flash_wait_busy (void);
bool flash_erase_block (uint32_t address);


#endif

#ifndef FLASH_H__
#define FLASH_H__


#include "sys.h"


uint32_t flash_read (uint32_t sdram_offset);
void flash_program (uint32_t sdram_offset);


#endif

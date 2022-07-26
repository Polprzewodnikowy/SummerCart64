#include "flash.h"
#include "fpga.h"


#define ERASE_BLOCK_SIZE    (64 * 1024)


void flash_erase_block (uint32_t offset) {
    uint8_t dummy[2];

    for (int i = 0; i < 2; i++) {
        while (fpga_reg_get(REG_FLASH_SCR) & FLASH_SCR_BUSY);
        fpga_reg_set(REG_FLASH_SCR, offset);
        fpga_mem_read(offset, 2, dummy);
        offset += ERASE_BLOCK_SIZE;
    }
}

#include "flash.h"
#include "fpga.h"


#define FLASH_ADDRESS       (0x04000000UL)
#define ERASE_BLOCK_SIZE    (64 * 1024)


void flash_wait_busy (void) {
    uint8_t dummy[2];
    fpga_mem_read(FLASH_ADDRESS, 2, dummy);
}

void flash_erase_block (uint32_t offset) {
    for (int i = 0; i < (FLASH_ERASE_BLOCK_SIZE / ERASE_BLOCK_SIZE); i++) {
        fpga_reg_set(REG_FLASH_SCR, offset);
        while (fpga_reg_get(REG_FLASH_SCR) & FLASH_SCR_BUSY);
        offset += ERASE_BLOCK_SIZE;
    }
    flash_wait_busy();
}

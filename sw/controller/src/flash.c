#include "flash.h"
#include "fpga.h"


void flash_erase_block (uint32_t offset) {
    uint8_t dummy[2];

    while (fpga_reg_get(REG_FLASH_SCR) & FLASH_SCR_BUSY);
    fpga_reg_set(REG_FLASH_SCR, offset);
    fpga_mem_read(offset, 2, dummy);
}

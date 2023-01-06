#include "flash.h"
#include "fpga.h"


#define FLASH_ADDRESS       (0x04000000UL)
#define FLASH_SIZE          (16 * 1024 * 1024)
#define ERASE_BLOCK_SIZE    (64 * 1024)


bool flash_program (uint32_t src, uint32_t dst, uint32_t length) {
    if (((src + length) >= FLASH_ADDRESS) && (src < (FLASH_ADDRESS + FLASH_SIZE))) {
        return true;
    }
    if ((dst < FLASH_ADDRESS) || ((dst + length) >= (FLASH_ADDRESS + FLASH_SIZE))) {
        return true;
    }
    while (length > 0) {
        uint32_t block = (length > FPGA_MAX_MEM_TRANSFER) ? FPGA_MAX_MEM_TRANSFER : length;
        fpga_mem_copy(src, dst, block);
        src += block;
        dst += block;
        length -= block;
    }
    return false;
}

void flash_wait_busy (void) {
    uint8_t dummy[2];
    fpga_mem_read(FLASH_ADDRESS, 2, dummy);
}

bool flash_erase_block (uint32_t offset) {
    if ((offset % FLASH_ERASE_BLOCK_SIZE) != 0) {
        return true;
    }
    offset &= (FLASH_SIZE - 1);
    for (int i = 0; i < (FLASH_ERASE_BLOCK_SIZE / ERASE_BLOCK_SIZE); i++) {
        fpga_reg_set(REG_FLASH_SCR, offset);
        while (fpga_reg_get(REG_FLASH_SCR) & FLASH_SCR_BUSY);
        offset += ERASE_BLOCK_SIZE;
    }
    flash_wait_busy();
    return false;
}

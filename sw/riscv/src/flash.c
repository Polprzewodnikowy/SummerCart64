#include "flash.h"


static io32_t dummy;


uint32_t flash_size (void) {
    return FLASH_SIZE;
}

void flash_read (uint32_t sdram_offset) {
    io32_t *flash = (io32_t *) (FLASH_BASE);
    io32_t *sdram = (io32_t *) (SDRAM_BASE + sdram_offset);

    for (size_t i = 0; i < FLASH_SIZE; i += sizeof(io32_t)) {
        *sdram++ = *flash++;
    }
}

void flash_program (uint32_t sdram_offset) {
    io32_t *flash = (io32_t *) (FLASH_BASE);
    io32_t *sdram = (io32_t *) (SDRAM_BASE + sdram_offset);

    CFG->SCR |= CFG_SCR_FLASH_WP_DISABLE;

    CFG->SCR |= CFG_SCR_FLASH_ERASE_START;

    while (CFG->SCR & CFG_SCR_FLASH_ERASE_BUSY);

    for (int i = 0; i < FLASH_SIZE; i += sizeof(io32_t)) {
        *flash++ = *sdram++;
    }

    CFG->SCR |= CFG_SCR_FLASH_WP_ENABLE;

    flash = (io32_t *) (FLASH_BASE);

    dummy = *flash;

    return;
}

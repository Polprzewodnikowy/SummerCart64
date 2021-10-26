#include "flash.h"


uint32_t flash_read (uint32_t sdram_offset) {
    io32_t *flash = (io32_t *) (FLASH_BASE);
    io32_t *sdram = (io32_t *) (SDRAM_BASE + sdram_offset);

    for (size_t i = 0; i < FLASH_SIZE; i += 4) {
        *sdram++ = *flash++;
    }

    return FLASH_SIZE;
}

void flash_program (uint32_t sdram_offset) {
    uint32_t cr;

    io32_t *flash = (io32_t *) (FLASH_BASE);
    io32_t *sdram = (io32_t *) (SDRAM_BASE + sdram_offset);

    cr = FLASH_CONFIG->CR;
    for (size_t sector = 0; sector < FLASH_NUM_SECTORS; sector++) {
        cr &= ~(1 << (FLASH_CR_WRITE_PROTECT_BIT + sector));
    }
    FLASH_CONFIG->CR = cr;

    while ((FLASH_CONFIG->SR & FLASH_SR_STATUS_MASK) != FLASH_SR_STATUS_IDLE);

    for (size_t sector = 0; sector < FLASH_NUM_SECTORS; sector++) {
        cr = FLASH_CONFIG->CR;
        cr &= ~(FLASH_CR_SECTOR_ERASE_MASK);
        cr |= ((sector + 1) << FLASH_CR_SECTOR_ERASE_BIT);
        FLASH_CONFIG->CR = cr;

        while ((FLASH_CONFIG->SR & FLASH_SR_STATUS_MASK) == FLASH_SR_STATUS_BUSY_ERASE);

        if (!(FLASH_CONFIG->SR & FLASH_SR_ERASE_SUCCESSFUL)) {
            break;
        }
    }

    if (FLASH_CONFIG->SR & FLASH_SR_ERASE_SUCCESSFUL) {
        for (size_t word = 0; word < FLASH_SIZE; word += 4) {
            *flash++ = *sdram++;

            if (!(FLASH_CONFIG->SR & FLASH_SR_WRITE_SUCCESSFUL)) {
                break;
            }
        }
    }

    cr = FLASH_CONFIG->CR;
    cr |= FLASH_CR_SECTOR_ERASE_MASK;
    for (size_t sector = 0; sector < FLASH_NUM_SECTORS; sector++) {
        cr |= (1 << (FLASH_CR_WRITE_PROTECT_BIT + sector));
    }
    FLASH_CONFIG->CR = cr;

    return;
}

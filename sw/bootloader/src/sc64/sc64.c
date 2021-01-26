#include "sc64.h"


static void sc64_enable_peripheral(uint32_t mask) {
    uint32_t config = platform_pi_io_read(&SC64_CART->scr);

    config |= mask;

    platform_pi_io_write(&SC64_CART->scr, config);
}

static void sc64_disable_peripheral(uint32_t mask) {
    uint32_t config = platform_pi_io_read(&SC64_CART->scr);

    config &= ~mask;

    platform_pi_io_write(&SC64_CART->scr, config);
}

void sc64_enable_sd(void) {
    sc64_enable_peripheral(SC64_CART_SCR_SD_ENABLE);
}

void sc64_disable_sd(void) {
    sc64_disable_peripheral(SC64_CART_SCR_SD_ENABLE);
} 

void sc64_enable_eeprom_pi(void) {
    sc64_enable_peripheral(SC64_CART_SCR_EEPROM_PI_ENABLE);
}

void sc64_disable_eeprom_pi(void) {
    sc64_disable_peripheral(SC64_CART_SCR_EEPROM_PI_ENABLE);
}

void sc64_enable_eeprom(uint8_t mode_16k) {
    sc64_enable_peripheral(SC64_CART_SCR_EEPROM_ENABLE);
    if (mode_16k) {
        sc64_enable_peripheral(SC64_CART_SCR_EEPROM_16K_MODE);
    } else {
        sc64_disable_peripheral(SC64_CART_SCR_EEPROM_16K_MODE);
    }
}

void sc64_disable_eeprom(void) {
    sc64_disable_peripheral(SC64_CART_SCR_EEPROM_ENABLE);
}

void sc64_enable_ddipl(void) {
    sc64_enable_peripheral(SC64_CART_SCR_DDIPL_ENABLE);
}

void sc64_disable_ddipl(void) {
    sc64_disable_peripheral(SC64_CART_SCR_DDIPL_ENABLE);
}

void sc64_enable_rom_switch(void) {
    sc64_enable_peripheral(SC64_CART_SCR_ROM_SWITCH);
}

void sc64_disable_rom_switch(void) {
    sc64_disable_peripheral(SC64_CART_SCR_ROM_SWITCH);
}

void sc64_enable_sdram_writable(void) {
    sc64_enable_peripheral(SC64_CART_SCR_SDRAM_WRITABLE);
}

void sc64_disable_sdram_writable(void) {
    sc64_disable_peripheral(SC64_CART_SCR_SDRAM_WRITABLE);
}

uint32_t sc64_get_boot_mode(void) {
    return platform_pi_io_read(&SC64_CART->boot);
}

void sc64_set_boot_mode(uint32_t boot) {
    platform_pi_io_write(&SC64_CART->boot, boot);
}

uint32_t sc64_get_version(void) {
    return platform_pi_io_read(&SC64_CART->version);
}

void sc64_set_ddipl_address(uint32_t address) {
    platform_pi_io_write(&SC64_CART->ddipl_addr, address);
}

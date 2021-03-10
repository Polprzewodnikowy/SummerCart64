#include "control.h"
#include "helpers.h"
#include "io_dma.h"
#include "registers.h"
#include "save.h"


static bool sc64_save_address_length_get(uint32_t *pi_address, size_t *length, bool *is_eeprom) {
    sc64_save_type_t save_type = sc64_save_type_get();

    *is_eeprom = false;

    switch (save_type) {
        case SC64_SAVE_EEPROM_4K:
            *pi_address = SC64_EEPROM_BASE;
            *length = 512;
            *is_eeprom = true;
            break;
        case SC64_SAVE_EEPROM_16K:
            *pi_address = SC64_EEPROM_BASE;
            *length = 2048;
            *is_eeprom = true;
            break;
        case SC64_SAVE_SRAM_256K:
            *pi_address = sc64_save_address_get();
            *length = (32 * 1024);
            break;
        case SC64_SAVE_SRAM_768K:
            *pi_address = sc64_save_address_get();
            *length = (96 * 1024);
            break;
        case SC64_SAVE_FLASHRAM_1M:
            *pi_address = sc64_save_address_get();
            *length = (128 * 1024);
            break;
        default:
            return false;
    }

    return true;
}


void sc64_save_type_set(sc64_save_type_t save_type) {
    uint32_t scr;

    scr = sc64_io_read(&SC64_CART->SCR);

    scr &= ~(
        SC64_CART_SCR_FLASHRAM_ENABLE |
        SC64_CART_SCR_SRAM_768K_MODE |
        SC64_CART_SCR_SRAM_ENABLE |
        SC64_CART_SCR_EEPROM_16K_MODE |
        SC64_CART_SCR_EEPROM_ENABLE
    );

    switch (save_type)
    {
        case SC64_SAVE_EEPROM_4K:
            scr |= SC64_CART_SCR_EEPROM_ENABLE;
            break;
        case SC64_SAVE_EEPROM_16K:
            scr |= SC64_CART_SCR_EEPROM_ENABLE | SC64_CART_SCR_EEPROM_16K_MODE;
            break;
        case SC64_SAVE_SRAM_256K:
            scr |= SC64_CART_SCR_SRAM_ENABLE;
            break;
        case SC64_SAVE_SRAM_768K:
            scr |= SC64_CART_SCR_SRAM_ENABLE | SC64_CART_SCR_SRAM_768K_MODE;
            break;
        case SC64_SAVE_FLASHRAM_1M:
            scr |= SC64_CART_SCR_FLASHRAM_ENABLE;
            break;
        default:
            break;
    }
    
    sc64_io_write(&SC64_CART->SCR, scr);
}

sc64_save_type_t sc64_save_type_get(void) {
    uint32_t scr;

    scr = sc64_io_read(&SC64_CART->SCR);

    if (scr & SC64_CART_SCR_FLASHRAM_ENABLE) {
        return SC64_SAVE_FLASHRAM_1M;
    } else if (scr & SC64_CART_SCR_SRAM_ENABLE) {
        return (scr & SC64_CART_SCR_SRAM_768K_MODE) ? SC64_SAVE_SRAM_768K : SC64_SAVE_SRAM_256K;
    } else if (scr & SC64_CART_SCR_EEPROM_ENABLE) {
        return (scr & SC64_CART_SCR_EEPROM_16K_MODE) ? SC64_SAVE_EEPROM_16K : SC64_SAVE_EEPROM_4K;
    }

    return SC64_SAVE_DISABLED;
}

void sc64_save_address_set(uint32_t address) {
    sc64_io_write(&SC64_CART->SAVE_ADDR, address);
}

uint32_t sc64_save_address_get(void) {
    return sc64_io_read(&SC64_CART->SAVE_ADDR);
}

void sc64_save_eeprom_pi_access(bool is_enabled) {
    sc64_helpers_reg_write(&SC64_CART->SCR, SC64_CART_SCR_EEPROM_PI_ENABLE, is_enabled);
}

void sc64_save_write(void *ram_address) {
    uint32_t pi_address;
    size_t length;
    bool is_eeprom;
    uint32_t scr;
    bool eeprom_pi_access;
    bool sdram_writable;

    if (sc64_save_address_length_get(&pi_address, &length, &is_eeprom)) {
        scr = sc64_io_read(&SC64_CART->SCR);
        eeprom_pi_access = scr & SC64_CART_SCR_EEPROM_PI_ENABLE;
        sdram_writable = scr & SC64_CART_SCR_SDRAM_WRITABLE;
        
        if (is_eeprom && !eeprom_pi_access) {
            sc64_save_eeprom_pi_access(true);
        } else if (!sdram_writable) {
            sc64_control_sdram_writable(true);
        }

        sc64_dma_write(ram_address, (volatile uint32_t *) pi_address, length);

        if (is_eeprom && !eeprom_pi_access) {
            sc64_save_eeprom_pi_access(false);
        } else if (!sdram_writable) {
            sc64_control_sdram_writable(false);
        }
    }
}

void sc64_save_read(void *ram_address) {
    uint32_t pi_address;
    size_t length;
    bool is_eeprom;
    bool eeprom_pi_access;

    if (sc64_save_address_length_get(&pi_address, &length, &is_eeprom)) {
        eeprom_pi_access = sc64_io_read(&SC64_CART->SCR) & SC64_CART_SCR_EEPROM_PI_ENABLE;

        if (is_eeprom && !eeprom_pi_access) {
            sc64_save_eeprom_pi_access(true);
        }

        sc64_dma_read(ram_address, (volatile uint32_t *) pi_address, length);

        if (is_eeprom && !eeprom_pi_access) {
            sc64_save_eeprom_pi_access(false);
        }
    }
}

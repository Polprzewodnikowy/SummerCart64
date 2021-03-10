#include "control.h"
#include "helpers.h"
#include "io_dma.h"
#include "registers.h"


void sc64_control_sdram_writable(bool is_enabled) {
    sc64_helpers_reg_write(&SC64_CART->SCR, SC64_CART_SCR_SDRAM_WRITABLE, is_enabled);
}

void sc64_control_embedded_flash_access(bool is_enabled) {
    sc64_helpers_reg_write(&SC64_CART->SCR, SC64_CART_SCR_ROM_SWITCH, !is_enabled);
}

uint32_t sc64_control_version_get(void) {
    return sc64_io_read(&SC64_CART->VERSION);
}

void sc64_control_ddipl_access(bool is_enabled) {
    sc64_helpers_reg_write(&SC64_CART->SCR, SC64_CART_SCR_DDIPL_ENABLE, is_enabled);
}

void sc64_control_ddipl_address_set(uint32_t address) {
    sc64_io_write(&SC64_CART->DDIPL_ADDR, address);
}

uint32_t sc64_control_ddipl_address_get(void) {
    return sc64_io_read(&SC64_CART->DDIPL_ADDR);
}

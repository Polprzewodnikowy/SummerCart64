#include "boot.h"
#include "helpers.h"
#include "io_dma.h"
#include "registers.h"


void sc64_boot_skip_bootloader(bool is_enabled) {
    sc64_helpers_reg_write(&SC64_CART->SCR, SC64_CART_SCR_SKIP_BOOTLOADER, is_enabled);
}

void sc64_boot_config_set(sc64_boot_config_t *boot_config) {
    sc64_io_write(&SC64_CART->BOOT, boot_config->_packed_value);
}

void sc64_boot_config_get(sc64_boot_config_t *boot_config) {
    boot_config->_packed_value = sc64_io_read(&SC64_CART->BOOT);
}

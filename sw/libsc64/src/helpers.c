#include "helpers.h"
#include "io_dma.h"
#include "registers.h"


void sc64_helpers_reg_write(volatile uint32_t *pi_address, uint32_t mask, bool mask_mode) {
    uint32_t reg = sc64_io_read(pi_address);

    if (mask_mode) {
        reg |= mask;
    } else {
        reg &= ~mask;
    }

    sc64_io_write(pi_address, reg);
}

void sc64_helpers_reg_set(volatile uint32_t *pi_address, uint32_t mask) {
    sc64_helpers_reg_write(pi_address, mask, true);
}

void sc64_helpers_reg_clear(volatile uint32_t *pi_address, uint32_t mask) {
    sc64_helpers_reg_write(pi_address, mask, false);
}

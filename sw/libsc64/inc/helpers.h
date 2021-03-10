#ifndef SC64_HELPERS_H__
#define SC64_HELPERS_H__


#include <stdbool.h>
#include <stdint.h>


void sc64_helpers_reg_write(volatile uint32_t *pi_address, uint32_t mask, bool mask_mode);

void sc64_helpers_reg_set(volatile uint32_t *pi_address, uint32_t mask);
void sc64_helpers_reg_clear(volatile uint32_t *pi_address, uint32_t mask);


#endif

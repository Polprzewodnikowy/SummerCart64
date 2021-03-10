#ifndef SC64_CONTROL_H__
#define SC64_CONTROL_H__


#include <stdbool.h>
#include <stdint.h>


void sc64_control_sdram_writable(bool is_enabled);
void sc64_control_embedded_flash_access(bool is_enabled);

uint32_t sc64_control_version_get(void);

void sc64_control_ddipl_access(bool is_enabled);

void sc64_control_ddipl_address_set(uint32_t address);
uint32_t sc64_control_ddipl_address_get(void);


#endif

#ifndef SC64_SAVE_H__
#define SC64_SAVE_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum sc64_save_type_e {
    SC64_SAVE_DISABLED,
    SC64_SAVE_EEPROM_4K,
    SC64_SAVE_EEPROM_16K,
    SC64_SAVE_SRAM_256K,
    SC64_SAVE_SRAM_768K,
    SC64_SAVE_FLASHRAM_1M,
} sc64_save_type_t;


void sc64_save_type_set(sc64_save_type_t save_type);
sc64_save_type_t sc64_save_type_get(void);

void sc64_save_address_set(uint32_t address);
uint32_t sc64_save_address_get(void);

void sc64_save_eeprom_pi_access(bool is_enabled);

void sc64_save_write(void *ram_address);
void sc64_save_read(void *ram_address);


#endif

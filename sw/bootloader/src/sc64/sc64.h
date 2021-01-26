#ifndef SC64_H__
#define SC64_H__


#include "sc64_regs.h"


void sc64_enable_sd(void);
void sc64_disable_sd(void);
void sc64_enable_eeprom_pi(void);
void sc64_disable_eeprom_pi(void);
void sc64_enable_eeprom(uint8_t mode_16k);
void sc64_disable_eeprom(void);
void sc64_enable_ddipl(void);
void sc64_disable_ddipl(void);
void sc64_enable_rom_switch(void);
void sc64_disable_rom_switch(void);
void sc64_enable_sdram_writable(void);
void sc64_disable_sdram_writable(void);
uint32_t sc64_get_boot_mode(void);
void sc64_set_boot_mode(uint32_t boot);
uint32_t sc64_get_version(void);
void sc64_set_ddipl_address(uint32_t address);


#endif

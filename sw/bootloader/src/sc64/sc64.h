#ifndef SC64_H__
#define SC64_H__


#include "sc64_regs.h"


#define SC64_SDRAM_SIZE     (64 * 1024 * 1024)


uint32_t sc64_get_scr(void);
void sc64_set_scr(uint32_t scr);
void sc64_enable_skip_bootloader(void);
void sc64_disable_skip_bootloader(void);
void sc64_enable_flashram(void);
void sc64_disable_flashram(void);
void sc64_enable_sram(bool mode_768k);
void sc64_disable_sram(void);
void sc64_enable_sd(void);
void sc64_disable_sd(void);
void sc64_enable_eeprom_pi(void);
void sc64_disable_eeprom_pi(void);
void sc64_enable_eeprom(bool mode_16k);
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
uint32_t sc64_get_ddipl_address(void);
void sc64_set_ddipl_address(uint32_t address);
uint32_t sc64_get_sram_address(void);
void sc64_set_sram_address(uint32_t address);


#endif

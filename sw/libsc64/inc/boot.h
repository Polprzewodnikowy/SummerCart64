#ifndef SC64_BOOT_H__
#define SC64_BOOT_H__


#include <stdbool.h>
#include <stdint.h>


typedef union sc64_boot_config_s {
    uint32_t _packed_value;
    struct {
        uint16_t _padding: 16;
        bool skip_menu: 1;
        bool cic_seed_override: 1;
        bool tv_type_override: 1;
        bool ddipl_override: 1;
        bool rom_loaded: 1;
        uint8_t tv_type: 2;
        uint8_t os_version: 1;
        uint8_t cic_seed: 8;
    };
} sc64_boot_config_t;


void sc64_boot_config_set(sc64_boot_config_t *boot_config);
void sc64_boot_config_get(sc64_boot_config_t *boot_config);

void sc64_boot_skip_bootloader(bool is_enabled);


#endif

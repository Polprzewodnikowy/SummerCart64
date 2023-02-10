#ifndef CFG_H__
#define CFG_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    SAVE_TYPE_NONE = 0,
    SAVE_TYPE_EEPROM_4K = 1,
    SAVE_TYPE_EEPROM_16K = 2,
    SAVE_TYPE_SRAM = 3,
    SAVE_TYPE_FLASHRAM = 4,
    SAVE_TYPE_SRAM_BANKED = 5
} save_type_t;


uint32_t cfg_get_version (void);
bool cfg_query (uint32_t *args);
bool cfg_update (uint32_t *args);
bool cfg_query_setting (uint32_t *args);
bool cfg_update_setting (uint32_t *args);
bool cfg_set_rom_write_enable (bool value);
save_type_t cfg_get_save_type (void);
void cfg_get_time (uint32_t *args);
void cfg_set_time (uint32_t *args);
void cfg_reset_state (void);
void cfg_init (void);
void cfg_process (void);


#endif

#ifndef SC64_H__
#define SC64_H__


#include "platform.h"


#define SC64_CFG_SCR_CPU_READY              (1 << 31)
#define SC64_CFG_SCR_CPU_BUSY               (1 << 30)

#define SC64_CMD_CONFIG                     'C'
#define SC64_CMD_QUERY                      'Q'

enum cfg_id {
    CFG_ID_SCR,
    CFG_ID_SDRAM_SWITCH,
    CFG_ID_SDRAM_WRITABLE,
    CFG_ID_DD_ENABLE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_SAVE_OFFEST,
    CFG_ID_DD_OFFEST,
    CFG_ID_SKIP_BOOTLOADER,
};


typedef struct sc64_config {
    union {
        uint32_t ___raw_data[2];
        struct {
            uint8_t ___unused_1[2];
            uint8_t save_type;
            uint8_t ___unused_2: 1;
            uint8_t dd_enable: 1;
            uint8_t sdram_writable: 1;
            uint8_t sdram_switch: 1;
            uint8_t ___unused_3;
            uint8_t tv_type;
            uint16_t cic_type;
        };
    };
} sc64_config_t;


bool sc64_check_presence(void);
void sc64_wait_cpu_ready(void);
void sc64_wait_cpu_busy(void);
void sc64_perform_cmd(uint8_t cmd, uint32_t *args);
void sc64_set_config(uint32_t type, uint32_t value);


#endif

#include "sc64.h"


typedef struct sc64_cart_registers {
    __IO reg_t SR_CMD;
    __IO reg_t DATA[2];
    __IO reg_t VERSION;
} sc64_cfg_registers_t;

#define SC64_CFG_BASE   (0x1FFF0000)
#define SC64_CFG        ((__IO sc64_cfg_registers_t *) SC64_CFG_BASE)



bool sc64_check_presence(void) {
    uint32_t version = platform_pi_io_read(&SC64_CFG->VERSION);
    return (version == 0x53437632);
}

void sc64_wait_cpu_ready(void) {
    uint32_t sr;
    do {
        sr = platform_pi_io_read(&SC64_CFG->SR_CMD);
    } while (sr & SC64_CFG_SCR_CPU_READY);
}

void sc64_wait_cpu_busy(void) {
    uint32_t sr;
    do {
        sr = platform_pi_io_read(&SC64_CFG->SR_CMD);
    } while (sr & SC64_CFG_SCR_CPU_BUSY);
}

void sc64_perform_cmd(uint8_t cmd, uint32_t *args) {
    for (int i = 0; i < 2; i++) {
        platform_pi_io_write(&SC64_CFG->DATA[i], args[i]);
    }
    platform_pi_io_write(&SC64_CFG->SR_CMD, (uint32_t) cmd);
    sc64_wait_cpu_busy();
    for (int i = 0; i < 2; i++) {
        args[i] = platform_pi_io_read(&SC64_CFG->DATA[i]);
    }
}

void sc64_set_config(uint32_t type, uint32_t value) {
    uint32_t args[2] = { type, value };
    sc64_perform_cmd(SC64_CMD_CONFIG, args);
}

void sc64_get_config(sc64_config_t *config) {
    uint32_t args[2] = { SC64_CONFIG_NOP, 0 };
    sc64_perform_cmd(SC64_CMD_CONFIG, args);
    config->___raw_data[0] = args[0];
    config->___raw_data[1] = args[1];
}

void sc64_print_debug(uint32_t a1, uint32_t a2, uint32_t a3) {
    // uint32_t args[3] = { a1, a2, a3 };
    // sc64_perform_cmd(SC64_CMD_DEBUG, args);
}

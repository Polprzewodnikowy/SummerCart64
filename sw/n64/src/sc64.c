#include "sc64.h"


bool sc64_check_presence (void) {
    uint32_t version = pi_io_read(&SC64->VERSION);
    return (version == SC64_VERSION_2);
}

void sc64_wait_cpu_ready (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (!(sr & SC64_SR_CPU_READY));
}

bool sc64_wait_cpu_busy (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (sr & SC64_SR_CPU_BUSY);
    return sr & SC64_SR_CMD_ERROR;
}

bool sc64_perform_cmd (uint8_t cmd, uint32_t *args, uint32_t *result) {
    if (args != NULL) {
        pi_io_write(&SC64->DATA[0], args[0]);
        pi_io_write(&SC64->DATA[1], args[1]);
    }
    pi_io_write(&SC64->SR_CMD, ((uint32_t) (cmd)) & 0xFF);
    bool error = sc64_wait_cpu_busy();
    if (result != NULL) {
        result[0] = pi_io_read(&SC64->DATA[0]);
        result[1] = pi_io_read(&SC64->DATA[1]);
    }
    return error;
}

uint32_t sc64_query_config (cfg_id_t id) {
    uint32_t args[2] = { id, 0 };
    uint32_t result[2];
    sc64_perform_cmd(SC64_CMD_QUERY, args, result);
    return result[1];
}

void sc64_change_config (cfg_id_t id, uint32_t value) {
    uint32_t args[2] = { id, value };
    sc64_perform_cmd(SC64_CMD_CONFIG, args, NULL);
}

void sc64_get_info (sc64_info_t *info) {
    info->cic_seed = (uint16_t) sc64_query_config(CFG_ID_CIC_SEED);
    info->tv_type = (tv_type_t) sc64_query_config(CFG_ID_TV_TYPE);
    info->boot_mode = (boot_mode_t) sc64_query_config(CFG_ID_BOOT_MODE);
}

void sc64_uart_put_char (char c) {
#ifdef DEBUG
    uint32_t args[2] = { (uint32_t) (c), 0 };
    sc64_perform_cmd(SC64_CMD_UART_PUT, args, NULL);
#endif
}

void sc64_uart_print_string (const char *text) {
    while (*text != '\0') {
        sc64_uart_put_char (*text++);
    }
}

void sc64_init (void) {
    while (!sc64_check_presence());
    sc64_wait_cpu_ready();
    sc64_change_config(CFG_ID_SDRAM_SWITCH, true);
    sc64_uart_print_string("\033c");
}

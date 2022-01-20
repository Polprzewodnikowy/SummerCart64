#include "sc64.h"


#define SECTOR_SIZE     (512)


bool sc64_check_presence (void) {
    uint32_t version = pi_io_read(&SC64->VERSION);
    return (version == SC64_VERSION_2);
}

static void sc64_wait_cpu_ready (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (!(sr & SC64_SR_CPU_READY));
}

static bool sc64_wait_cpu_busy (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (sr & SC64_SR_CPU_BUSY);
    return sr & SC64_SR_CMD_ERROR;
}

static bool sc64_perform_cmd (uint8_t cmd, uint32_t *args, uint32_t *result) {
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

static void sc64_uart_put_char (char c) {
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

static bool sc64_wait_drive_ready (uint8_t drive) {
    bool error;
    uint32_t args[2] = { drive, 0 };
    uint32_t result[2];
    do {
        error = sc64_perform_cmd(SC64_CMD_DRIVE_BUSY, args, result);
    } while (!result[0]);
    return error;
}

static bool sc64_drive_read_sector (uint8_t drive, void *buffer, uint32_t sector) {
    io32_t *src = SC64->BUFFER;
    uint8_t *dst = (uint8_t *) (buffer);
    uint32_t args[2] = { drive, sector };
    if (sc64_perform_cmd(SC64_CMD_DRIVE_READ, args, NULL)) {
        return true;
    }
    if (sc64_wait_drive_ready(drive)) {
        return true;
    }
    for (int i = 0; i < SECTOR_SIZE; i += sizeof(uint32_t)) {
        uint32_t data = pi_io_read(src++);
        *dst++ = ((data >> 24) & 0xFF);
        *dst++ = ((data >> 16) & 0xFF);
        *dst++ = ((data >> 8) & 0xFF);
        *dst++ = ((data >> 0) & 0xFF);
    }
    return false;
}

bool sc64_storage_read (uint8_t drive, void *buffer, uint32_t sector, uint32_t count) {
    if (sc64_wait_drive_ready(drive)) {
        return true;
    }
    for (int i = 0; i < count; i++) {
        if (sc64_drive_read_sector(drive, buffer, (sector + i))) {
            return true;
        }
        buffer += SECTOR_SIZE;
    }
    return false;
}

static bool sc64_drive_write_sector (uint8_t drive, void *buffer, uint32_t sector) {
    uint8_t *src = (uint8_t *) (buffer);
    io32_t *dst = SC64->BUFFER;
    uint32_t args[2] = { drive, sector };
    uint32_t data;
    if (sc64_wait_drive_ready(drive)) {
        return true;
    }
    for (int i = 0; i < SECTOR_SIZE; i += sizeof(uint32_t)) {
        data = ((*src++) << 24);
        data |= ((*src++) << 16);
        data |= ((*src++) << 8);
        data |= ((*src++) << 0);
        pi_io_write(dst++, data);
    }
    if (sc64_perform_cmd(SC64_CMD_DRIVE_WRITE, args, NULL)) {
        return true;
    }
    return false;
}

bool sc64_storage_write (uint8_t drive, void *buffer, uint32_t sector, uint32_t count) {
    for (int i = 0; i < count; i++) {
        if (sc64_drive_write_sector(drive, buffer, (sector + i))) {
            return true;
        }
        buffer += SECTOR_SIZE;
    }
    return false;
}

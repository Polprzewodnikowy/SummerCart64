#include "sc64.h"


#define SECTOR_SIZE     (512)


bool sc64_check_presence (void) {
    uint32_t version = pi_io_read(&SC64->VERSION);
    return (version == SC64_VERSION_2);
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

void sc64_init (void) {
    sc64_change_config(CFG_ID_BOOTLOADER_SWITCH, false);
}

void sc64_get_time (rtc_time_t *t) {
    uint32_t result[2];
    sc64_perform_cmd(SC64_CMD_GET_TIME, NULL, result);
    t->second = (result[0] & 0xFF);
    t->minute = ((result[0] >> 8) & 0xFF);
    t->hour = ((result[0] >> 16) & 0xFF);
    t->weekday = ((result[1] >> 24) & 0xFF);
    t->day = (result[1] & 0xFF);
    t->month = ((result[1] >> 8) & 0xFF);
    t->year = ((result[1] >> 16) & 0xFF);
}

void sc64_set_time (rtc_time_t *t) {
    uint32_t args[2] = {
        ((t->hour << 16) | (t->minute << 8) | t->second),
        ((t->weekday << 24) | (t->year << 16) | (t->month << 8) | t->day),
    };
    sc64_perform_cmd(SC64_CMD_SET_TIME, args, NULL);
}

static uint32_t sc64_wait_drive_ready (drive_id_t drive) {
    uint32_t args[2] = { ((drive & 0x01) << 31), 0 };
    uint32_t result[2];
    do {
        sc64_perform_cmd(SC64_CMD_DRIVE_BUSY, args, result);
    } while (result[0]);
    return result[1];
}

bool sc64_storage_init (drive_id_t drive) {
    uint32_t args[2] = { ((drive & 0x01) << 31), 0 };
    if (sc64_perform_cmd(SC64_CMD_DRIVE_INIT, args, NULL)) {
        return true;
    }
    if (sc64_wait_drive_ready(drive)) {
        return true;
    }
    return false;
}

static bool sc64_drive_start_rw (drive_id_t drive, bool write, uint32_t sector, uint32_t offset) {
    uint32_t args[2] = { (((drive & 0x01) << 31) | (offset & 0x7FFFFFFF)), sector };
    if (sc64_perform_cmd(write ? SC64_CMD_DRIVE_WRITE : SC64_CMD_DRIVE_READ, args, NULL)) {
        return true;
    }
    return false;
}

bool sc64_storage_read (drive_id_t drive, void *buffer, uint32_t sector, uint32_t count) {
    io32_t *src;
    uint8_t *dst = (uint8_t *) (buffer);
    uint32_t current_offset = 0;
    uint32_t next_offset = SECTOR_SIZE;

    if (sc64_drive_start_rw(drive, false, sector++, 0x03FF0000UL)) {
        return true;
    }
    while (count > 0) {
        if (sc64_wait_drive_ready(drive)) {
            return true;
        }
        if (count > 1) {
            if (sc64_drive_start_rw(drive, false, sector++, 0x03FF0000UL + next_offset)) {
                return true;
            }
            next_offset = next_offset ? 0 : SECTOR_SIZE;
        }
        src = (io32_t *) (0x13FF0000UL + current_offset);
        for (int i = 0; i < (SECTOR_SIZE / sizeof(uint32_t)); i++) {
            uint32_t data = pi_io_read(src + i);
            *dst++ = ((data >> 24) & 0xFF);
            *dst++ = ((data >> 16) & 0xFF);
            *dst++ = ((data >> 8) & 0xFF);
            *dst++ = ((data >> 0) & 0xFF);
        }
        current_offset = current_offset ? 0 : SECTOR_SIZE;
        count -= 1;
    }

    return false;
}

bool sc64_storage_write (drive_id_t drive, const void *buffer, uint32_t sector, uint32_t count) {
    uint8_t *src = (uint8_t *) (buffer);
    io32_t *dst = (io32_t *) (0x13FF0000UL);

    while (count > 0) {
        for (int i = 0; i < (SECTOR_SIZE / sizeof(uint32_t)); i++) {
            uint32_t data = 0;
            data |= ((*src++) << 24);
            data |= ((*src++) << 16);
            data |= ((*src++) << 8);
            data |= ((*src++) << 0);
            pi_io_write((dst + i), data);
        }
        if (sc64_drive_start_rw(drive, true, sector, 0x03FF0000UL)) {
            return true;
        }
        if (sc64_wait_drive_ready(drive)) {
            return true;
        }
        sector += 1;
        count -= 1;
    }

    return false;
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

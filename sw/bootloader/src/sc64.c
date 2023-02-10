#include "io.h"
#include "sc64.h"


typedef struct {
    io32_t SR_CMD;
    io32_t DATA[2];
    io32_t VERSION;
    io32_t KEY;
} sc64_regs_t;

#define SC64_REGS_BASE              (0x1FFF0000UL)
#define SC64_REGS                   ((sc64_regs_t *) SC64_REGS_BASE)

#define SC64_SR_IRQ_PENDING         (1 << 29)
#define SC64_SR_CMD_ERROR           (1 << 30)
#define SC64_SR_CPU_BUSY            (1 << 31)

#define SC64_VERSION_2              (0x53437632)

#define SC64_KEY_RESET              (0x00000000UL)
#define SC64_KEY_UNLOCK_1           (0x5F554E4CUL)
#define SC64_KEY_UNLOCK_2           (0x4F434B5FUL)
#define SC64_KEY_LOCK               (0xFFFFFFFFUL)

typedef enum {
    SC64_CMD_VERSION_GET        = 'v',
    SC64_CMD_CONFIG_GET         = 'c',
    SC64_CMD_CONFIG_SET         = 'C',
    SC64_CMD_SETTING_GET        = 'a',
    SC64_CMD_SETTING_SET        = 'A',
    SC64_CMD_TIME_GET           = 't',
    SC64_CMD_TIME_SET           = 'T',
    SC64_CMD_USB_READ           = 'm',
    SC64_CMD_USB_WRITE          = 'M',
    SC64_CMD_USB_READ_STATUS    = 'u',
    SC64_CMD_USB_WRITE_STATUS   = 'U',
    SC64_CMD_SD_CARD_OP         = 'i',
    SC64_CMD_SD_SECTOR_SET      = 'I',
    SC64_CMD_SD_READ            = 's',
    SC64_CMD_SD_WRITE           = 'S',
    SC64_CMD_DD_SD_INFO         = 'D',
    SC64_CMD_WRITEBACK_SD_INFO  = 'W',
    SC64_CMD_FLASH_PROGRAM      = 'K',
    SC64_CMD_FLASH_WAIT_BUSY    = 'p',
    SC64_CMD_FLASH_ERASE_BLOCK  = 'P',
    SC64_CMD_DEBUG_GET          = '?',
} cmd_id_t;

typedef enum {
    SD_CARD_OP_DEINIT = 0,
    SD_CARD_OP_INIT = 1,
    SD_CARD_OP_GET_STATUS = 2,
    SD_CARD_OP_GET_INFO = 3,
} sd_card_op_t;


static bool sc64_wait_cpu_busy (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64_REGS->SR_CMD);
    } while (sr & SC64_SR_CPU_BUSY);
    return (sr & SC64_SR_CMD_ERROR);
}

static bool sc64_execute_cmd (uint8_t cmd, uint32_t *args, uint32_t *result) {
    if (args != NULL) {
        pi_io_write(&SC64_REGS->DATA[0], args[0]);
        pi_io_write(&SC64_REGS->DATA[1], args[1]);
    }
    pi_io_write(&SC64_REGS->SR_CMD, ((uint32_t) (cmd)) & 0xFF);
    bool error = sc64_wait_cpu_busy();
    if (result != NULL) {
        result[0] = pi_io_read(&SC64_REGS->DATA[0]);
        result[1] = pi_io_read(&SC64_REGS->DATA[1]);
    }
    return error;
}


sc64_error_t sc64_get_error (void) {
    if (pi_io_read(&SC64_REGS->SR_CMD) & SC64_SR_CMD_ERROR) {
        return (sc64_error_t) (pi_io_read(&SC64_REGS->DATA[0]));
    }
    return SC64_OK;
}

void sc64_unlock (void) {
    pi_io_write(&SC64_REGS->KEY, SC64_KEY_RESET);
    pi_io_write(&SC64_REGS->KEY, SC64_KEY_UNLOCK_1);
    pi_io_write(&SC64_REGS->KEY, SC64_KEY_UNLOCK_2);
}

void sc64_lock (void) {
    pi_io_write(&SC64_REGS->KEY, SC64_KEY_RESET);
    pi_io_write(&SC64_REGS->KEY, SC64_KEY_LOCK);
}

bool sc64_check_presence (void) {
    uint32_t version = pi_io_read(&SC64_REGS->VERSION);
    if (version == SC64_VERSION_2) {
        sc64_wait_cpu_busy();
        return true;
    }
    return false;
}

bool sc64_irq_pending (void) {
    if (pi_io_read(&SC64_REGS->SR_CMD) & SC64_SR_IRQ_PENDING) {
        return true;
    }
    return false;
}

void sc64_irq_clear (void) {
    pi_io_write(&SC64_REGS->VERSION, 0);
}

uint32_t sc64_get_config (sc64_cfg_id_t id) {
    uint32_t args[2] = { id, 0 };
    uint32_t result[2];
    sc64_execute_cmd(SC64_CMD_CONFIG_GET, args, result);
    return result[1];
}

void sc64_set_config (sc64_cfg_id_t id, uint32_t value) {
    uint32_t args[2] = { id, value };
    sc64_execute_cmd(SC64_CMD_CONFIG_SET, args, NULL);
}

uint32_t sc64_get_setting (sc64_setting_id_t id) {
    uint32_t args[2] = { id, 0 };
    uint32_t result[2];
    sc64_execute_cmd(SC64_CMD_SETTING_GET, args, result);
    return result[1];
}

void sc64_set_setting (sc64_setting_id_t id, uint32_t value) {
    uint32_t args[2] = { id, value };
    sc64_execute_cmd(SC64_CMD_SETTING_SET, args, NULL);
}

void sc64_get_boot_info (sc64_boot_info_t *info) {
    info->boot_mode = (sc64_boot_mode_t) sc64_get_config(CFG_ID_BOOT_MODE);
    info->cic_seed = (sc64_cic_seed_t) sc64_get_config(CFG_ID_CIC_SEED);
    info->tv_type = (sc64_tv_type_t) sc64_get_config(CFG_ID_TV_TYPE);
}

void sc64_get_time (sc64_rtc_time_t *t) {
    uint32_t result[2];
    sc64_execute_cmd(SC64_CMD_TIME_GET, NULL, result);
    t->second = (result[0] & 0xFF);
    t->minute = ((result[0] >> 8) & 0xFF);
    t->hour = ((result[0] >> 16) & 0xFF);
    t->weekday = ((result[1] >> 24) & 0xFF);
    t->day = (result[1] & 0xFF);
    t->month = ((result[1] >> 8) & 0xFF);
    t->year = ((result[1] >> 16) & 0xFF);
}

void sc64_set_time (sc64_rtc_time_t *t) {
    uint32_t args[2] = {
        ((t->hour << 16) | (t->minute << 8) | t->second),
        ((t->weekday << 24) | (t->year << 16) | (t->month << 8) | t->day),
    };
    sc64_execute_cmd(SC64_CMD_TIME_SET, args, NULL);
}

bool sc64_usb_read_ready (uint8_t *type, uint32_t *length) {
    uint32_t result[2];
    sc64_execute_cmd(SC64_CMD_USB_READ_STATUS, NULL, result);
    if (type != NULL) {
        *type = result[0] & 0xFF;
    }
    if (length != NULL) {
        *length = result[1];
    }
    return (result[1] > 0);
}

bool sc64_usb_read (void *address, uint32_t length) {
    uint32_t args[2] = { (uint32_t) (address), length };
    uint32_t result[2];
    if (sc64_execute_cmd(SC64_CMD_USB_READ, args, NULL)) {
        return true;
    }
    do {
        sc64_execute_cmd(SC64_CMD_USB_READ_STATUS, NULL, result);
    } while(result[0] & (1 << 31));
    return false;
}

bool sc64_usb_write_ready (void) {
    uint32_t result[2];
    sc64_execute_cmd(SC64_CMD_USB_WRITE_STATUS, NULL, result);
    return (!(result[0] & (1 << 31)));
}

bool sc64_usb_write (void *address, uint8_t type, uint32_t length) {
    while (!sc64_usb_write_ready());
    uint32_t args[2] = { (uint32_t) (address), ((type << 24) | (length & 0xFFFFFF)) };
    return sc64_execute_cmd(SC64_CMD_USB_WRITE, args, NULL);
}

bool sc64_sd_card_init (void) {
    uint32_t args[2] = { (uint32_t) (NULL), SD_CARD_OP_INIT };
    if (sc64_execute_cmd(SC64_CMD_SD_CARD_OP, args, NULL)) {
        return true;
    }
    return false;
}

bool sc64_sd_card_deinit (void) {
    uint32_t args[2] = { (uint32_t) (NULL), SD_CARD_OP_DEINIT };
    if (sc64_execute_cmd(SC64_CMD_SD_CARD_OP, args, NULL)) {
        return true;
    }
    return false;
}

sc64_sd_card_status_t sc64_sd_card_get_status (void) {
    uint32_t args[2] = { (uint32_t) (NULL), SD_CARD_OP_GET_STATUS };
    uint32_t result[2];
    if (sc64_execute_cmd(SC64_CMD_SD_CARD_OP, args, result)) {
        return false;
    }
    return (sc64_sd_card_status_t) (result[1]);
}

bool sc64_sd_card_get_info (void *address) {
    uint32_t args[2] = { (uint32_t) (address), SD_CARD_OP_GET_INFO };
    if (sc64_execute_cmd(SC64_CMD_SD_CARD_OP, args, NULL)) {
        return true;
    }
    return false;
}

bool sc64_sd_read_sectors (void *address, uint32_t sector, uint32_t count) {
    uint32_t sector_set_args[2] = { sector, 0 };
    uint32_t read_args[2] = { (uint32_t) (address), count };
    if (sc64_execute_cmd(SC64_CMD_SD_SECTOR_SET, sector_set_args, NULL)) {
        return true;
    }
    return sc64_execute_cmd(SC64_CMD_SD_READ, read_args, NULL);
}

bool sc64_sd_write_sectors (void *address, uint32_t sector, uint32_t count) {
    uint32_t sector_set_args[2] = { sector, 0 };
    uint32_t write_args[2] = { (uint32_t) (address), count };
    if (sc64_execute_cmd(SC64_CMD_SD_SECTOR_SET, sector_set_args, NULL)) {
        return true;
    }
    return sc64_execute_cmd(SC64_CMD_SD_WRITE, write_args, NULL);
}

bool sc64_dd_set_sd_info (void *address, uint32_t length) {
    uint32_t args[2] = { (uint32_t) (address), length };
    if (sc64_execute_cmd(SC64_CMD_DD_SD_INFO, args, NULL)) {
        return true;
    }
    return false;
}

bool sc64_writeback_enable (void *address) {
    uint32_t args[2] = { (uint32_t) (address), 0 };
    if (sc64_execute_cmd(SC64_CMD_WRITEBACK_SD_INFO, args, NULL)) {
        return true;
    }
    return false;
}

bool sc64_flash_program (void *address, uint32_t length) {
    uint32_t args[2] = { (uint32_t) (address), length };
    return sc64_execute_cmd(SC64_CMD_FLASH_PROGRAM, args, NULL);
}

void sc64_flash_wait_busy (void) {
    uint32_t args[2] = { true, 0 };
    sc64_execute_cmd(SC64_CMD_FLASH_WAIT_BUSY, args, NULL);
}

uint32_t sc64_flash_get_erase_block_size (void) {
    uint32_t args[2] = { false, 0 };
    uint32_t result[2];
    sc64_execute_cmd(SC64_CMD_FLASH_WAIT_BUSY, args, result);
    return result[0];
}

bool sc64_flash_erase_block (void *address) {
    uint32_t args[2] = { (uint32_t) (address), 0 };
    return sc64_execute_cmd(SC64_CMD_FLASH_ERASE_BLOCK, args, NULL);
}

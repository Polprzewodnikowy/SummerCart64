#include "io.h"
#include "sc64.h"


typedef struct {
    io32_t SR_CMD;
    io32_t DATA[2];
    io32_t IDENTIFIER;
    io32_t KEY;
} sc64_regs_t;

#define SC64_REGS_BASE              (0x1FFF0000UL)
#define SC64_REGS                   ((sc64_regs_t *) SC64_REGS_BASE)


#define SC64_SR_IRQ_PENDING         (1 << 29)
#define SC64_SR_CMD_ERROR           (1 << 30)
#define SC64_SR_CPU_BUSY            (1 << 31)

#define SC64_V2_IDENTIFIER          (0x53437632)

#define SC64_KEY_RESET              (0x00000000UL)
#define SC64_KEY_UNLOCK_1           (0x5F554E4CUL)
#define SC64_KEY_UNLOCK_2           (0x4F434B5FUL)
#define SC64_KEY_LOCK               (0xFFFFFFFFUL)


typedef enum {
    CMD_ID_IDENTIFIER_GET       = 'v',
    CMD_ID_VERSION_GET          = 'V',
    CMD_ID_CONFIG_GET           = 'c',
    CMD_ID_CONFIG_SET           = 'C',
    CMD_ID_SETTING_GET          = 'a',
    CMD_ID_SETTING_SET          = 'A',
    CMD_ID_TIME_GET             = 't',
    CMD_ID_TIME_SET             = 'T',
    CMD_ID_USB_READ             = 'm',
    CMD_ID_USB_WRITE            = 'M',
    CMD_ID_USB_READ_STATUS      = 'u',
    CMD_ID_USB_WRITE_STATUS     = 'U',
    CMD_ID_SD_CARD_OP           = 'i',
    CMD_ID_SD_SECTOR_SET        = 'I',
    CMD_ID_SD_READ              = 's',
    CMD_ID_SD_WRITE             = 'S',
    CMD_ID_DD_SD_INFO           = 'D',
    CMD_ID_WRITEBACK_PENDING    = 'w',
    CMD_ID_WRITEBACK_SD_INFO    = 'W',
    CMD_ID_FLASH_PROGRAM        = 'K',
    CMD_ID_FLASH_WAIT_BUSY      = 'p',
    CMD_ID_FLASH_ERASE_BLOCK    = 'P',
} sc64_cmd_id_t;

typedef enum {
    SD_CARD_OP_DEINIT = 0,
    SD_CARD_OP_INIT = 1,
    SD_CARD_OP_GET_STATUS = 2,
    SD_CARD_OP_GET_INFO = 3,
    SD_CARD_OP_BYTE_SWAP_ON = 4,
    SD_CARD_OP_BYTE_SWAP_OFF = 5,
} sd_card_op_t;

typedef struct {
    sc64_cmd_id_t id;
    uint32_t arg[2];
    uint32_t rsp[2];
} sc64_cmd_t;


static sc64_error_t sc64_execute_cmd (sc64_cmd_t *cmd) {
    pi_io_write(&SC64_REGS->DATA[0], cmd->arg[0]);
    pi_io_write(&SC64_REGS->DATA[1], cmd->arg[1]);

    pi_io_write(&SC64_REGS->SR_CMD, (cmd->id & 0xFF));

    uint32_t sr;
    do {
        sr = pi_io_read(&SC64_REGS->SR_CMD);
    } while (sr & SC64_SR_CPU_BUSY);

    if (sr & SC64_SR_CMD_ERROR) {
        return (sc64_error_t) (pi_io_read(&SC64_REGS->DATA[0]));
    }

    cmd->rsp[0] = pi_io_read(&SC64_REGS->DATA[0]);
    cmd->rsp[1] = pi_io_read(&SC64_REGS->DATA[1]);

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
    bool detected = (pi_io_read(&SC64_REGS->IDENTIFIER) == SC64_V2_IDENTIFIER);
    if (detected) {
        while (pi_io_read(&SC64_REGS->SR_CMD) & SC64_SR_CPU_BUSY);
    }
    return detected;
}


bool sc64_irq_pending (void) {
    return (pi_io_read(&SC64_REGS->SR_CMD) & SC64_SR_IRQ_PENDING);
}

void sc64_irq_clear (void) {
    pi_io_write(&SC64_REGS->IDENTIFIER, 0);
}


sc64_error_t sc64_get_identifier (uint32_t *identifier) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_IDENTIFIER_GET
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *identifier = cmd.rsp[0];
    return error;
}

sc64_error_t sc64_get_version (uint16_t *major, uint16_t *minor, uint32_t *revision) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_VERSION_GET
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *major = ((cmd.rsp[0] >> 16) & 0xFFFF);
    *minor = (cmd.rsp[0] & 0xFFFF);
    *revision = cmd.rsp[1];
    return error;
}


sc64_error_t sc64_get_config (sc64_cfg_id_t id, uint32_t *value) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_CONFIG_GET,
        .arg = { id }
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *value = cmd.rsp[1];
    return error;
}

sc64_error_t sc64_set_config (sc64_cfg_id_t id, uint32_t value) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_CONFIG_SET,
        .arg = { id, value }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_get_setting (sc64_setting_id_t id, uint32_t *value) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SETTING_GET,
        .arg = { id }
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *value = cmd.rsp[1];
    return error;
}

sc64_error_t sc64_set_setting (sc64_setting_id_t id, uint32_t value) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SETTING_SET,
        .arg = { id, value }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_get_boot_info (sc64_boot_info_t *info) {
    sc64_error_t error;
    uint32_t value;

    if ((error = sc64_get_config(CFG_ID_BOOT_MODE, &value)) != SC64_OK) {
        return error;
    }
    info->boot_mode = value;

    if ((error = sc64_get_config(CFG_ID_CIC_SEED, &value)) != SC64_OK) {
        return error;
    }
    info->cic_seed = value;

    if ((error = sc64_get_config(CFG_ID_TV_TYPE, &value)) != SC64_OK) {
        return error;
    }
    info->tv_type = value;

    return SC64_OK;
}


sc64_error_t sc64_get_time (sc64_rtc_time_t *t) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_TIME_GET
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    t->hour = ((cmd.rsp[0] >> 16) & 0xFF);
    t->minute = ((cmd.rsp[0] >> 8) & 0xFF);
    t->second = (cmd.rsp[0] & 0xFF);
    t->weekday = ((cmd.rsp[1] >> 24) & 0xFF);
    t->year = ((cmd.rsp[1] >> 16) & 0xFF);
    t->month = ((cmd.rsp[1] >> 8) & 0xFF);
    t->day = (cmd.rsp[1] & 0xFF);
    return error;
}

sc64_error_t sc64_set_time (sc64_rtc_time_t *t) {
    uint32_t time[2] = {(
        ((t->hour << 16) & 0xFF) |
        ((t->minute << 8) & 0xFF) |
        (t->second & 0xFF)
    ), (
        ((t->weekday << 24) & 0xFF) |
        ((t->year << 16) & 0xFF) |
        ((t->month << 8) & 0xFF) |
        (t->day & 0xFF)
    )};
    sc64_cmd_t cmd = {
        .id = CMD_ID_TIME_SET,
        .arg = { time[0], time[1] }
    };
    return sc64_execute_cmd(&cmd);
}


sc64_error_t sc64_usb_get_status (bool *reset_state, bool *cable_unplugged) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_USB_READ_STATUS,
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *reset_state = (bool) (cmd.rsp[0] & (1 << 30));
    *cable_unplugged = (bool) (cmd.rsp[0] & (1 << 29));
    return error;
}

sc64_error_t sc64_usb_read_info (uint8_t *type, uint32_t *length) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_USB_READ_STATUS,
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *type = (cmd.rsp[0] & 0xFF);
    *length = cmd.rsp[1];
    return error;
}

sc64_error_t sc64_usb_read_busy (bool *read_busy) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_USB_READ_STATUS,
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *read_busy = (bool) (cmd.rsp[0] & (1 << 31));
    return error;
}

sc64_error_t sc64_usb_write_busy (bool *write_busy) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_USB_WRITE_STATUS,
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *write_busy = (bool) (cmd.rsp[0] & (1 << 31));
    return error;
}

sc64_error_t sc64_usb_read (void *address, uint32_t length) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_USB_READ,
        .arg = { (uint32_t) (address), length }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_usb_write (void *address, uint8_t type, uint32_t length) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_USB_WRITE,
        .arg = { (uint32_t) (address), ((type << 24) | (length & 0xFFFFFF)) }
    };
    return sc64_execute_cmd(&cmd);
}


sc64_error_t sc64_sd_card_init (void) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_CARD_OP,
        .arg = { (uint32_t) (NULL), SD_CARD_OP_INIT }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_sd_card_deinit (void) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_CARD_OP,
        .arg = { (uint32_t) (NULL), SD_CARD_OP_DEINIT }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_sd_card_get_status (sc64_sd_card_status_t *sd_card_status) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_CARD_OP,
        .arg = { (uint32_t) (NULL), SD_CARD_OP_GET_STATUS }
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *sd_card_status = (sc64_sd_card_status_t) (cmd.rsp[1]);
    return error;
}

sc64_error_t sc64_sd_card_get_info (void *address) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_CARD_OP,
        .arg = { (uint32_t) (address), SD_CARD_OP_GET_INFO }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_sd_set_byte_swap (bool enabled) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_CARD_OP,
        .arg = { (uint32_t) (NULL), enabled ? SD_CARD_OP_BYTE_SWAP_ON : SD_CARD_OP_BYTE_SWAP_OFF }
    };
    return sc64_execute_cmd(&cmd);
}

static sc64_error_t sc64_sd_sector_set (uint32_t sector) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_SECTOR_SET,
        .arg = { sector }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_sd_read_sectors (void *address, uint32_t sector, uint32_t count) {
    sc64_error_t error;
    if ((error = sc64_sd_sector_set(sector)) != SC64_OK) {
        return error;
    }
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_READ,
        .arg = { (uint32_t) (address), count }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_sd_write_sectors (void *address, uint32_t sector, uint32_t count) {
    sc64_error_t error;
    if ((error = sc64_sd_sector_set(sector)) != SC64_OK) {
        return error;
    }
    sc64_cmd_t cmd = {
        .id = CMD_ID_SD_WRITE,
        .arg = { (uint32_t) (address), count }
    };
    return sc64_execute_cmd(&cmd);
}


sc64_error_t sc64_dd_set_sd_info (void *address, uint32_t length) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_DD_SD_INFO,
        .arg = { (uint32_t) (address), length }
    };
    return sc64_execute_cmd(&cmd);
}


sc64_error_t sc64_writeback_pending (bool *pending) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_WRITEBACK_PENDING
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *pending = (cmd.rsp[0] != 0);
    return error;
}

sc64_error_t sc64_writeback_enable (void *address) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_WRITEBACK_SD_INFO,
        .arg = { (uint32_t) (address) }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_writeback_disable (void) {
    sc64_error_t error;
    uint32_t save_type;
    error = sc64_get_config(CFG_ID_SAVE_TYPE, &save_type);
    if (error != SC64_OK) {
        return error;
    }
    return sc64_set_config(CFG_ID_SAVE_TYPE, save_type);
}


sc64_error_t sc64_flash_program (void *address, uint32_t length) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_FLASH_PROGRAM,
        .arg = { (uint32_t) (address), length }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_flash_wait_busy (void) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_FLASH_WAIT_BUSY,
        .arg = { true }
    };
    return sc64_execute_cmd(&cmd);
}

sc64_error_t sc64_flash_get_erase_block_size (size_t *erase_block_size) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_FLASH_WAIT_BUSY,
        .arg = { false }
    };
    sc64_error_t error = sc64_execute_cmd(&cmd);
    *erase_block_size = (size_t) (cmd.rsp[0]);
    return error;
}

sc64_error_t sc64_flash_erase_block (void *address) {
    sc64_cmd_t cmd = {
        .id = CMD_ID_FLASH_ERASE_BLOCK,
        .arg = { (uint32_t) (address) }
    };
    return sc64_execute_cmd(&cmd);
}

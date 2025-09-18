#include "button.h"
#include "cfg.h"
#include "cic.h"
#include "dd.h"
#include "flash.h"
#include "fpga.h"
#include "hw.h"
#include "isv.h"
#include "led.h"
#include "rtc.h"
#include "sd.h"
#include "usb.h"
#include "version.h"
#include "writeback.h"


#define DATA_BUFFER_ADDRESS     (0x05000000)
#define DATA_BUFFER_SIZE        (8192)


typedef enum {
    CMD_ID_IDENTIFIER_GET = 'v',
    CMD_ID_VERSION_GET = 'V',
    CMD_ID_CONFIG_GET = 'c',
    CMD_ID_CONFIG_SET = 'C',
    CMD_ID_SETTING_GET = 'a',
    CMD_ID_SETTING_SET = 'A',
    CMD_ID_TIME_GET = 't',
    CMD_ID_TIME_SET = 'T',
    CMD_ID_USB_READ = 'm',
    CMD_ID_USB_WRITE = 'M',
    CMD_ID_USB_READ_STATUS = 'u',
    CMD_ID_USB_WRITE_STATUS = 'U',
    CMD_ID_SD_CARD_OP = 'i',
    CMD_ID_SD_SECTOR_SET = 'I',
    CMD_ID_SD_READ = 's',
    CMD_ID_SD_WRITE = 'S',
    CMD_ID_DISK_MAPPING_SET = 'D',
    CMD_ID_WRITEBACK_PENDING = 'w',
    CMD_ID_WRITEBACK_SD_INFO = 'W',
    CMD_ID_FLASH_PROGRAM = 'K',
    CMD_ID_FLASH_WAIT_BUSY = 'p',
    CMD_ID_FLASH_ERASE_BLOCK = 'P',
    CMD_ID_DIAGNOSTIC_GET = '%',
} cmd_id_t;

typedef enum {
    CFG_ID_BOOTLOADER_SWITCH = 0,
    CFG_ID_ROM_WRITE_ENABLE = 1,
    CFG_ID_ROM_SHADOW_ENABLE = 2,
    CFG_ID_DD_MODE = 3,
    CFG_ID_ISV_ADDRESS = 4,
    CFG_ID_BOOT_MODE = 5,
    CFG_ID_SAVE_TYPE = 6,
    CFG_ID_CIC_SEED = 7,
    CFG_ID_TV_TYPE = 8,
    CFG_ID_DD_SD_ENABLE = 9,
    CFG_ID_DD_DRIVE_TYPE = 10,
    CFG_ID_DD_DISK_STATE = 11,
    CFG_ID_BUTTON_STATE = 12,
    CFG_ID_BUTTON_MODE = 13,
    CFG_ID_ROM_EXTENDED_ENABLE = 14,
} cfg_id_t;

typedef enum {
    SETTING_ID_LED_ENABLE = 0,
} setting_id_t;

typedef enum {
    DD_MODE_DISABLED = 0,
    DD_MODE_REGS = 1,
    DD_MODE_IPL = 2,
    DD_MODE_FULL = 3
} dd_mode_t;

typedef enum {
    BOOT_MODE_MENU = 0,
    BOOT_MODE_ROM = 1,
    BOOT_MODE_DDIPL = 2,
    BOOT_MODE_DIRECT_ROM = 3,
    BOOT_MODE_DIRECT_DDIPL = 4,
} boot_mode_t;

typedef enum {
    CIC_SEED_AUTO = 0xFFFF
} cic_seed_t;

typedef enum {
    TV_TYPE_PAL = 0,
    TV_TYPE_NTSC = 1,
    TV_TYPE_MPAL = 2,
    TV_TYPE_PASSTHROUGH = 3
} tv_type_t;

typedef enum {
    DIAGNOSTIC_ID_VOLTAGE_TEMPERATURE = 0,
} diagnostic_id_t;

typedef enum {
    SDRAM = (1 << 0),
    FLASH = (1 << 1),
    BRAM = (1 << 2),
} translate_type_t;

typedef enum {
    ERROR_TYPE_CFG = 1,
    ERROR_TYPE_SD_CARD = 2,
} error_type_t;

typedef enum {
    CFG_OK = 0,
    CFG_ERROR_UNKNOWN_COMMAND = 1,
    CFG_ERROR_INVALID_ARGUMENT = 2,
    CFG_ERROR_INVALID_ADDRESS = 3,
    CFG_ERROR_INVALID_ID = 4,
} cfg_error_t;

struct process {
    bool cmd_queued;
    cmd_id_t cmd;
    uint32_t data[2];
    boot_mode_t boot_mode;
    save_type_t save_type;
    cic_seed_t cic_seed;
    tv_type_t tv_type;
    bool usb_output_ready;
    uint32_t sd_card_sector;
};


static struct process p;


static bool cfg_cmd_check (void) {
    if (!writeback_pending() && !hw_gpio_get(GPIO_ID_N64_RESET)) {
        sd_release_lock(SD_LOCK_N64);
    }

    uint32_t reg = fpga_reg_get(REG_CFG_CMD);

    if (reg & CFG_CMD_AUX_PENDING) {
        usb_tx_info_t packet_info;
        usb_create_packet(&packet_info, PACKET_CMD_AUX_DATA);
        packet_info.data_length = 4;
        packet_info.data[0] = fpga_reg_get(REG_AUX);
        if (usb_enqueue_packet(&packet_info)) {
            fpga_reg_set(REG_CFG_CMD, CFG_CMD_AUX_DONE);
        }
    }

    if (!p.cmd_queued) {
        if (!(reg & CFG_CMD_PENDING)) {
            return true;
        }

        p.cmd_queued = true;
        p.cmd = (cmd_id_t) ((reg & CFG_CMD_MASK) >> CFG_CMD_BIT);
        p.data[0] = fpga_reg_get(REG_CFG_DATA_0);
        p.data[1] = fpga_reg_get(REG_CFG_DATA_1);
    }

    return false;
}

static void cfg_cmd_reply_success (void) {
    p.cmd_queued = false;
    fpga_reg_set(REG_CFG_DATA_0, p.data[0]);
    fpga_reg_set(REG_CFG_DATA_1, p.data[1]);
    fpga_reg_set(REG_CFG_CMD, CFG_CMD_DONE);
}

static void cfg_cmd_reply_error (error_type_t type, uint32_t error) {
    p.cmd_queued = false;
    fpga_reg_set(REG_CFG_DATA_0, ((type & 0xFF) << 24) | (error & 0xFFFFFF));
    fpga_reg_set(REG_CFG_DATA_1, 0);
    fpga_reg_set(REG_CFG_CMD, CFG_CMD_ERROR | CFG_CMD_DONE);
}

static void cfg_change_scr_bits (uint32_t mask, bool value) {
    if (value) {
        fpga_reg_set(REG_CFG_SCR, fpga_reg_get(REG_CFG_SCR) | mask);
    } else {
        fpga_reg_set(REG_CFG_SCR, fpga_reg_get(REG_CFG_SCR) & (~mask));
    }
}

static bool cfg_translate_address (uint32_t *address, uint32_t length, translate_type_t type) {
    if (length == 0) {
        return true;
    }
    *address &= 0x1FFFFFFF;
    if (type & SDRAM) {
        if (*address >= 0x06000000 && *address < 0x06400000) {
            if ((*address + length) <= 0x06400000) {
                *address = *address - 0x06000000 + 0x03BC0000;
                return false;
            }
        }
        if (*address >= 0x08000000 && *address < 0x08020000) {
            if ((*address + length) <= 0x08020000) {
                *address = *address - 0x08000000 + 0x03FE0000;
                return false;
            }
        }
        if (*address >= 0x10000000 && *address < 0x14000000) {
            if ((*address + length) <= 0x14000000) {
                *address = *address - 0x10000000 + 0x00000000;
                return false;
            }
        }
    }
    if (type & FLASH) {
        if (*address >= 0x14000000 && *address < 0x14E00000) {
            if ((*address + length) <= 0x14E00000) {
                *address = *address - 0x14000000 + 0x04000000;
                return false;
            }
        }
        if (*address >= 0x18000000 && *address < 0x18E00000) {
            if ((*address + length) <= 0x18E00000) {
                *address = *address - 0x18000000 + 0x04000000;
                return false;
            }
        }
        if (*address >= 0x1FFC0000 && *address < 0x1FFE0000) {
            if ((*address + length) <= 0x1FFE0000) {
                *address = *address - 0x1FFC0000 + 0x04FE0000;
                return false;
            }
        }
    }
    if (type & BRAM) {
        if (*address >= 0x1FFE0000 && *address < 0x1FFE2000) {
            if ((*address + length) <= 0x1FFE2000) {
                *address = *address - 0x1FFE0000 + 0x05000000;
                return false;
            }
        }
        if (*address >= 0x1FFE2000 && *address < 0x1FFE2800) {
            if ((*address + length) <= 0x1FFE2800) {
                *address = *address - 0x1FFE2000 + 0x05002000;
                return false;
            }
        }
    }
    return true;
}

static bool cfg_set_save_type (save_type_t save_type) {
    if (save_type >= __SAVE_TYPE_COUNT) {
        return true;
    }

    writeback_disable();

    uint32_t save_reset_mask = (
        CFG_SCR_EEPROM_16K |
        CFG_SCR_EEPROM_ENABLED |
        CFG_SCR_FLASHRAM_ENABLED |
        CFG_SCR_SRAM_BANKED |
        CFG_SCR_SRAM_ENABLED
    );

    cfg_change_scr_bits(save_reset_mask, false);

    switch (save_type) {
        case SAVE_TYPE_NONE:
            break;
        case SAVE_TYPE_EEPROM_4K:
            cfg_change_scr_bits(CFG_SCR_EEPROM_ENABLED, true);
            break;
        case SAVE_TYPE_EEPROM_16K:
            cfg_change_scr_bits(CFG_SCR_EEPROM_16K | CFG_SCR_EEPROM_ENABLED, true);
            break;
        case SAVE_TYPE_SRAM:
            cfg_change_scr_bits(CFG_SCR_SRAM_ENABLED, true);
            break;
        case SAVE_TYPE_FLASHRAM:
            cfg_change_scr_bits(CFG_SCR_FLASHRAM_ENABLED, true);
            break;
        case SAVE_TYPE_SRAM_BANKED:
            cfg_change_scr_bits(CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_ENABLED, true);
            break;
        case SAVE_TYPE_SRAM_1M:
            cfg_change_scr_bits(CFG_SCR_SRAM_ENABLED, true);
            break;
        case SAVE_TYPE_FLASHRAM_FAKE:
            cfg_change_scr_bits(CFG_SCR_FLASHRAM_ENABLED, true);
            break;
        default:
            save_type = SAVE_TYPE_NONE;
            break;
    }

    p.save_type = save_type;

    return false;
}

static bool cfg_read_diagnostic_data (uint32_t *args) {
    switch (args[0]) {
        case DIAGNOSTIC_ID_VOLTAGE_TEMPERATURE: {
            uint16_t voltage;
            int16_t temperature;
            hw_adc_read_voltage_temperature(&voltage, &temperature);
            args[1] = ((uint32_t) (voltage) << 16) | ((uint32_t) (temperature));
            break;
        }
        default:
            return true;
    }

    return false;
}

static void cfg_set_usb_output_ready (void) {
    p.usb_output_ready = true;
}


uint32_t cfg_get_identifier (void) {
    return fpga_reg_get(REG_CFG_IDENTIFIER);
}

bool cfg_query (uint32_t *args) {
    uint32_t scr = fpga_reg_get(REG_CFG_SCR);

    switch (args[0]) {
        case CFG_ID_BOOTLOADER_SWITCH:
            args[1] = (scr & CFG_SCR_BOOTLOADER_ENABLED);
            break;
        case CFG_ID_ROM_WRITE_ENABLE:
            args[1] = (scr & CFG_SCR_ROM_WRITE_ENABLED);
            break;
        case CFG_ID_ROM_SHADOW_ENABLE:
            args[1] = (scr & CFG_SCR_ROM_SHADOW_ENABLED);
            break;
        case CFG_ID_DD_MODE:
            args[1] = DD_MODE_DISABLED;
            if (scr & CFG_SCR_DDIPL_ENABLED) {
                args[1] |= DD_MODE_IPL;
            }
            if (scr & CFG_SCR_DD_ENABLED) {
                args[1] |= DD_MODE_REGS;
            }
            break;
        case CFG_ID_ISV_ADDRESS:
            args[1] = isv_get_address();
            break;
        case CFG_ID_BOOT_MODE:
            args[1] = p.boot_mode;
            break;
        case CFG_ID_SAVE_TYPE:
            args[1] = p.save_type;
            break;
        case CFG_ID_CIC_SEED:
            args[1] = p.cic_seed;
            break;
        case CFG_ID_TV_TYPE:
            args[1] = p.tv_type;
            break;
        case CFG_ID_DD_SD_ENABLE:
            args[1] = dd_get_sd_mode();
            break;
        case CFG_ID_DD_DRIVE_TYPE:
            args[1] = dd_get_drive_type();
            break;
        case CFG_ID_DD_DISK_STATE:
            args[1] = dd_get_disk_state();
            break;
        case CFG_ID_BUTTON_STATE:
            args[1] = button_get_state();
            break;
        case CFG_ID_BUTTON_MODE:
            args[1] = button_get_mode();
            break;
        case CFG_ID_ROM_EXTENDED_ENABLE:
            args[1] = (scr & CFG_SCR_ROM_EXTENDED_ENABLED);
            break;
        default:
            return true;
    }

    return false;
}

bool cfg_update (uint32_t *args) {
    switch (args[0]) {
        case CFG_ID_BOOTLOADER_SWITCH:
            cfg_change_scr_bits(CFG_SCR_BOOTLOADER_ENABLED, args[1]);
            break;
        case CFG_ID_ROM_WRITE_ENABLE:
            cfg_change_scr_bits(CFG_SCR_ROM_WRITE_ENABLED, args[1]);
            break;
        case CFG_ID_ROM_SHADOW_ENABLE:
            cfg_change_scr_bits(CFG_SCR_ROM_SHADOW_ENABLED, args[1]);
            break;
        case CFG_ID_DD_MODE:
            if (args[1] == DD_MODE_DISABLED) {
                cfg_change_scr_bits(CFG_SCR_DD_ENABLED | CFG_SCR_DDIPL_ENABLED, false);
            } else if (args[1] == DD_MODE_REGS) {
                cfg_change_scr_bits(CFG_SCR_DD_ENABLED, true);
                cfg_change_scr_bits(CFG_SCR_DDIPL_ENABLED, false);
            } else if (args[1] == DD_MODE_IPL) {
                cfg_change_scr_bits(CFG_SCR_DD_ENABLED, false);
                cfg_change_scr_bits(CFG_SCR_DDIPL_ENABLED, true);
            } else if (args[1] == DD_MODE_FULL) {
                cfg_change_scr_bits(CFG_SCR_DD_ENABLED | CFG_SCR_DDIPL_ENABLED, true);
            } else {
                return true;
            }
            break;
        case CFG_ID_ISV_ADDRESS:
            return isv_set_address(args[1]);
            break;
        case CFG_ID_BOOT_MODE:
            if (args[1] > BOOT_MODE_DIRECT_DDIPL) {
                return true;
            }
            p.boot_mode = args[1];
            cfg_change_scr_bits(CFG_SCR_BOOTLOADER_SKIP,
                (args[1] == BOOT_MODE_DIRECT_ROM) ||
                (args[1] == BOOT_MODE_DIRECT_DDIPL)
            );
            cic_set_dd_mode(args[1] == BOOT_MODE_DIRECT_DDIPL);
            break;
        case CFG_ID_SAVE_TYPE:
            return cfg_set_save_type((save_type_t) (args[1]));
            break;
        case CFG_ID_CIC_SEED:
            if ((args[1] != 0xFFFF) && (args[1] > 0xFF)) {
                return true;
            }
            p.cic_seed = (cic_seed_t) (args[1] & 0xFFFF);
            break;
        case CFG_ID_TV_TYPE:
            if (args[1] > TV_TYPE_PASSTHROUGH) {
                return true;
            }
            p.tv_type = (tv_type_t) (args[1] & 0x03);
            break;
        case CFG_ID_DD_SD_ENABLE:
            dd_set_sd_mode(args[1]);
            break;
        case CFG_ID_DD_DRIVE_TYPE:
            return dd_set_drive_type(args[1]);
            break;
        case CFG_ID_DD_DISK_STATE:
            return dd_set_disk_state(args[1]);
            break;
        case CFG_ID_BUTTON_STATE:
            return true;
            break;
        case CFG_ID_BUTTON_MODE:
            return button_set_mode(args[1]);
            break;
        case CFG_ID_ROM_EXTENDED_ENABLE:
            cfg_change_scr_bits(CFG_SCR_ROM_EXTENDED_ENABLED, args[1]);
            break;
        default:
            return true;
    }

    return false;
}

bool cfg_query_setting (uint32_t *args) {
    rtc_settings_t *settings = rtc_get_settings();

    switch (args[0]) {
        case SETTING_ID_LED_ENABLE:
            args[1] = settings->led_enabled;
            break;
        default:
            return true;
    }

    return false;
}

bool cfg_update_setting (uint32_t *args) {
    rtc_settings_t *settings = rtc_get_settings();

    switch (args[0]) {
        case SETTING_ID_LED_ENABLE:
            settings->led_enabled = args[1];
            break;
        default:
            return true;
    }

    rtc_save_settings();

    return false;
}

void cfg_set_rom_write_enable (bool value) {
    cfg_change_scr_bits(CFG_SCR_ROM_WRITE_ENABLED, value);
}

save_type_t cfg_get_save_type (void) {
    return p.save_type;
}

void cfg_get_time (uint32_t *args) {
    rtc_real_time_t t;
    rtc_get_time(&t);
    args[0] = ((t.weekday << 24) | (t.hour << 16) | (t.minute << 8) | t.second);
    args[1] = ((t.century << 24) | (t.year << 16) | (t.month << 8) | t.day);
}

void cfg_set_time (uint32_t *args) {
    rtc_real_time_t t;
    t.second = (args[0] & 0xFF);
    t.minute = ((args[0] >> 8) & 0xFF);
    t.hour = ((args[0] >> 16) & 0xFF);
    t.weekday = ((args[0] >> 24) & 0xFF);
    t.day = (args[1] & 0xFF);
    t.month = ((args[1] >> 8) & 0xFF);
    t.year = ((args[1] >> 16) & 0xFF);
    t.century = ((args[1] >> 24) & 0xFF);
    rtc_set_time(&t);
}

void cfg_reset_state (void) {
    uint32_t mask = (
        CFG_SCR_BOOTLOADER_SKIP |
        CFG_SCR_ROM_WRITE_ENABLED |
        CFG_SCR_ROM_SHADOW_ENABLED |
        CFG_SCR_DD_ENABLED |
        CFG_SCR_DDIPL_ENABLED |
        CFG_SCR_ROM_EXTENDED_ENABLED
    );
    cfg_change_scr_bits(mask, false);
    cfg_set_save_type(SAVE_TYPE_NONE);
    button_set_mode(BUTTON_MODE_NONE);
    dd_set_drive_type(DD_DRIVE_TYPE_RETAIL);
    dd_set_disk_state(DD_DISK_STATE_EJECTED);
    dd_set_sd_mode(false);
    isv_set_address(0);
    p.cic_seed = CIC_SEED_AUTO;
    p.tv_type = TV_TYPE_PASSTHROUGH;
    p.boot_mode = BOOT_MODE_MENU;
}


void cfg_init (void) {
    fpga_reg_set(REG_CFG_SCR, CFG_SCR_BOOTLOADER_ENABLED);
    cfg_reset_state();
    p.cmd_queued = false;
    p.usb_output_ready = true;
}


void cfg_process (void) {
    if (cfg_cmd_check()) {
        return;
    }

    switch (p.cmd) {
        case CMD_ID_IDENTIFIER_GET:
            p.data[0] = cfg_get_identifier();
            break;

        case CMD_ID_VERSION_GET:
            version_firmware(&p.data[0], &p.data[1]);
            break;

        case CMD_ID_CONFIG_GET:
            if (cfg_query(p.data)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ID);
            }
            break;

        case CMD_ID_CONFIG_SET: {
            uint32_t prev[2] = { p.data[0], 0 };
            cfg_query(prev);
            if (cfg_update(p.data)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ID);
            }
            p.data[1] = prev[1];
            break;
        }

        case CMD_ID_SETTING_GET:
            if (cfg_query_setting(p.data)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ID);
            }
            break;

        case CMD_ID_SETTING_SET:
            if (cfg_update_setting(p.data)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ID);
            }
            break;

        case CMD_ID_TIME_GET:
            cfg_get_time(p.data);
            break;

        case CMD_ID_TIME_SET:
            cfg_set_time(p.data);
            break;

        case CMD_ID_USB_READ:
            if (cfg_translate_address(&p.data[0], p.data[1], (SDRAM | BRAM))) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ADDRESS);
            }
            if (!usb_prepare_read(p.data)) {
                return;
            }
            break;

        case CMD_ID_USB_WRITE: {
            if (cfg_translate_address(&p.data[0], p.data[1] & 0xFFFFFF, (SDRAM | BRAM))) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ADDRESS);
            }
            usb_tx_info_t packet_info;
            usb_create_packet(&packet_info, PACKET_CMD_DEBUG_OUTPUT);
            packet_info.data_length = 4;
            packet_info.data[0] = p.data[1];
            packet_info.dma_length = (p.data[1] & 0xFFFFFF);
            packet_info.dma_address = p.data[0];
            packet_info.done_callback = cfg_set_usb_output_ready;
            if (usb_enqueue_packet(&packet_info)) {
                p.usb_output_ready = false;
            } else {
                return;
            }
            break;
        }

        case CMD_ID_USB_READ_STATUS:
            usb_get_read_info(p.data);
            break;

        case CMD_ID_USB_WRITE_STATUS:
            p.data[0] = p.usb_output_ready ? 0 : (1 << 31);
            break;

        case CMD_ID_SD_CARD_OP: {
            sd_error_t error = SD_OK;
            switch (p.data[1]) {
                case SD_OP_DEINIT:
                    error = sd_try_lock(SD_LOCK_N64);
                    if (error == SD_OK) {
                        sd_card_deinit();
                        sd_release_lock(SD_LOCK_N64);
                    }
                    break;

                case SD_OP_INIT:
                    error = sd_try_lock(SD_LOCK_N64);
                    if (error == SD_OK) {
                        led_activity_on();
                        error = sd_card_init();
                        led_activity_off();
                        if (error != SD_OK) {
                            sd_release_lock(SD_LOCK_N64);
                        }
                    }
                    break;

                case SD_OP_GET_STATUS:
                    p.data[1] = sd_card_get_status();
                    break;

                case SD_OP_GET_INFO:
                    if (cfg_translate_address(&p.data[0], SD_CARD_INFO_SIZE, (SDRAM | BRAM))) {
                        return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, SD_ERROR_INVALID_ADDRESS);
                    }
                    error = sd_get_lock(SD_LOCK_N64);
                    if (error == SD_OK) {
                        error = sd_card_get_info(p.data[0]);
                    }
                    break;

                case SD_OP_BYTE_SWAP_ON:
                    error = sd_get_lock(SD_LOCK_N64);
                    if (error == SD_OK) {
                        error = sd_set_byte_swap(true);
                    }
                    break;

                case SD_OP_BYTE_SWAP_OFF:
                    error = sd_get_lock(SD_LOCK_N64);
                    if (error == SD_OK) {
                        error = sd_set_byte_swap(false);
                    }
                    break;

                default:
                    error = SD_ERROR_INVALID_OPERATION;
                    break;
            }
            if (error != SD_OK) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, error);
            }
            break;
        }

        case CMD_ID_SD_SECTOR_SET: {
            sd_error_t error = sd_get_lock(SD_LOCK_N64);
            if (error != SD_OK) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, error);
            }
            p.sd_card_sector = p.data[0];
            break;
        }

        case CMD_ID_SD_READ: {
            if (p.data[1] >= 0x800000) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, SD_ERROR_INVALID_ARGUMENT);
            }
            if (cfg_translate_address(&p.data[0], (p.data[1] * SD_SECTOR_SIZE), (SDRAM | FLASH | BRAM))) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, SD_ERROR_INVALID_ADDRESS);
            }
            sd_error_t error = sd_get_lock(SD_LOCK_N64);
            if (error == SD_OK) {
                led_activity_on();
                error = sd_read_sectors(p.data[0], p.sd_card_sector, p.data[1]);
                led_activity_off();
            }
            if (error != SD_OK) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, error);
            }
            p.sd_card_sector += p.data[1];
            break;
        }

        case CMD_ID_SD_WRITE: {
            if (p.data[1] >= 0x800000) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, SD_ERROR_INVALID_ARGUMENT);
            }
            if (cfg_translate_address(&p.data[0], (p.data[1] * SD_SECTOR_SIZE), (SDRAM | FLASH | BRAM))) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, SD_ERROR_INVALID_ADDRESS);
            }
            sd_error_t error = sd_get_lock(SD_LOCK_N64);
            if (error == SD_OK) {
                led_activity_on();
                error = sd_write_sectors(p.data[0], p.sd_card_sector, p.data[1]);
                led_activity_off();
            }
            if (error != SD_OK) {
                return cfg_cmd_reply_error(ERROR_TYPE_SD_CARD, error);
            }
            p.sd_card_sector += p.data[1];
            break;
        }

        case CMD_ID_DISK_MAPPING_SET:
            if (cfg_translate_address(&p.data[0], p.data[1], (SDRAM | BRAM))) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ADDRESS);
            }
            dd_set_disk_mapping(p.data[0], p.data[1]);
            break;

        case CMD_ID_WRITEBACK_PENDING:
            p.data[0] = writeback_pending();
            break;

        case CMD_ID_WRITEBACK_SD_INFO:
            if (cfg_translate_address(&p.data[0], WRITEBACK_SECTOR_TABLE_SIZE, (SDRAM | BRAM))) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ADDRESS);
            }
            writeback_load_sector_table(p.data[0]);
            writeback_enable(WRITEBACK_SD);
            break;

        case CMD_ID_FLASH_PROGRAM:
            if (p.data[1] >= DATA_BUFFER_SIZE) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ARGUMENT);
            }
            if (cfg_translate_address(&p.data[0], p.data[1], FLASH)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ADDRESS);
            }
            if (flash_program(DATA_BUFFER_ADDRESS, p.data[0], p.data[1])) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ARGUMENT);
            }
            break;

        case CMD_ID_FLASH_WAIT_BUSY:
            if (p.data[0]) {
                flash_wait_busy();
            }
            p.data[0] = FLASH_ERASE_BLOCK_SIZE;
            break;

        case CMD_ID_FLASH_ERASE_BLOCK:
            if (cfg_translate_address(&p.data[0], FLASH_ERASE_BLOCK_SIZE, FLASH)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ADDRESS);
            }
            if (flash_erase_block(p.data[0])) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ARGUMENT);
            }
            break;

        case CMD_ID_DIAGNOSTIC_GET:
            if (cfg_read_diagnostic_data(p.data)) {
                return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_INVALID_ID);
            }
            break;

        default:
            return cfg_cmd_reply_error(ERROR_TYPE_CFG, CFG_ERROR_UNKNOWN_COMMAND);
    }

    cfg_cmd_reply_success();
}

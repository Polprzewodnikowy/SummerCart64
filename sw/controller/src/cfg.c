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
    CFG_ERROR_OK = 0,
    CFG_ERROR_BAD_ARGUMENT = 1,
    CFG_ERROR_BAD_ADDRESS = 2,
    CFG_ERROR_BAD_CONFIG_ID = 3,
    CFG_ERROR_TIMEOUT = 4,
    CFG_ERROR_SD_CARD = 5,
    CFG_ERROR_UNKNOWN_CMD = -1,
} cfg_error_t;

typedef enum {
    SD_CARD_OP_DEINIT = 0,
    SD_CARD_OP_INIT = 1,
    SD_CARD_OP_GET_STATUS = 2,
    SD_CARD_OP_GET_INFO = 3,
    SD_CARD_OP_BYTE_SWAP_ON = 4,
    SD_CARD_OP_BYTE_SWAP_OFF = 5,
} sd_card_op_t;

typedef enum {
    DIAGNOSTIC_ID_VOLTAGE_TEMPERATURE = 0,
} diagnostic_id_t;

typedef enum {
    SDRAM = (1 << 0),
    FLASH = (1 << 1),
    BRAM = (1 << 2),
} translate_type_t;


struct process {
    boot_mode_t boot_mode;
    save_type_t save_type;
    cic_seed_t cic_seed;
    tv_type_t tv_type;
    bool usb_output_ready;
    uint32_t sd_card_sector;
};


static struct process p;


static void cfg_set_usb_output_ready (void) {
    p.usb_output_ready = true;
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

static void cfg_set_error (cfg_error_t error) {
    fpga_reg_set(REG_CFG_DATA_0, error);
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
    rtc_time_t t;
    rtc_get_time(&t);
    args[0] = ((t.hour << 16) | (t.minute << 8) | t.second);
    args[1] = ((t.weekday << 24) | (t.year << 16) | (t.month << 8) | t.day);
}

void cfg_set_time (uint32_t *args) {
    rtc_time_t t;
    t.second = (args[0] & 0xFF);
    t.minute = ((args[0] >> 8) & 0xFF);
    t.hour = ((args[0] >> 16) & 0xFF);
    t.weekday = ((args[0] >> 24) & 0xFF);
    t.day = (args[1] & 0xFF);
    t.month = ((args[1] >> 8) & 0xFF);
    t.year = ((args[1] >> 16) & 0xFF);
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
    p.usb_output_ready = true;
}


void cfg_process (void) {
    uint32_t reg;
    uint32_t args[2];
    uint32_t prev_cfg[2];
    usb_tx_info_t packet_info;

    reg = fpga_reg_get(REG_CFG_CMD);

    if (reg & CFG_CMD_PENDING) {
        args[0] = fpga_reg_get(REG_CFG_DATA_0);
        args[1] = fpga_reg_get(REG_CFG_DATA_1);
        char cmd = (char) ((reg & CFG_CMD_MASK) >> CFG_CMD_BIT);

        switch (cmd) {
            case 'v':
                args[0] = cfg_get_identifier();
                break;

            case 'V':
                version_firmware(&args[0], &args[1]);
                break;

            case 'c':
                if (cfg_query(args)) {
                    cfg_set_error(CFG_ERROR_BAD_CONFIG_ID);
                    return;
                }
                break;

            case 'C':
                prev_cfg[0] = args[0];
                cfg_query(prev_cfg);
                if (cfg_update(args)) {
                    cfg_set_error(CFG_ERROR_BAD_CONFIG_ID);
                    return;
                }
                args[1] = prev_cfg[1];
                break;

            case 'a':
                if (cfg_query_setting(args)) {
                    cfg_set_error(CFG_ERROR_BAD_CONFIG_ID);
                    return;
                }
                break;

            case 'A':
                if (cfg_update_setting(args)) {
                    cfg_set_error(CFG_ERROR_BAD_CONFIG_ID);
                    return;
                }
                break;

            case 't':
                cfg_get_time(args);
                break;

            case 'T':
                cfg_set_time(args);
                break;

            case 'm':
                if (cfg_translate_address(&args[0], args[1], (SDRAM | BRAM))) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                if (!usb_prepare_read(args)) {
                    return;
                }
                break;

            case 'M':
                if (cfg_translate_address(&args[0], args[1] & 0xFFFFFF, (SDRAM | BRAM))) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                usb_create_packet(&packet_info, PACKET_CMD_DEBUG_OUTPUT);
                packet_info.data_length = 4;
                packet_info.data[0] = args[1];
                packet_info.dma_length = (args[1] & 0xFFFFFF);
                packet_info.dma_address = args[0];
                packet_info.done_callback = cfg_set_usb_output_ready;
                if (usb_enqueue_packet(&packet_info)) {
                    p.usb_output_ready = false;
                } else {
                    return;
                }
                break;

            case 'u':
                usb_get_read_info(args);
                break;

            case 'U':
                args[0] = p.usb_output_ready ? 0 : (1 << 31);
                break;

            case 'i':
                switch (args[1]) {
                    case SD_CARD_OP_DEINIT:
                        sd_card_deinit();
                        break;
                    case SD_CARD_OP_INIT: {
                        led_activity_on();
                        bool error = sd_card_init();
                        led_activity_off();
                        if (error) {
                            cfg_set_error(CFG_ERROR_SD_CARD);
                            return;
                        }
                        break;
                    }
                    case SD_CARD_OP_GET_STATUS:
                        args[1] = sd_card_get_status();
                        break;
                    case SD_CARD_OP_GET_INFO:
                        if (cfg_translate_address(&args[0], SD_CARD_INFO_SIZE, (SDRAM | BRAM))) {
                            cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                            return;
                        }
                        if (sd_card_get_info(args[0])) {
                            cfg_set_error(CFG_ERROR_SD_CARD);
                            return;
                        }
                        break;
                    case SD_CARD_OP_BYTE_SWAP_ON:
                        if (sd_set_byte_swap(true)) {
                            cfg_set_error(CFG_ERROR_SD_CARD);
                            return;
                        }
                        break;
                    case SD_CARD_OP_BYTE_SWAP_OFF:
                        if (sd_set_byte_swap(false)) {
                            cfg_set_error(CFG_ERROR_SD_CARD);
                            return;
                        }
                        break;
                    default:
                        cfg_set_error(CFG_ERROR_BAD_ARGUMENT);
                        return;
                }
                break;

            case 'I':
                p.sd_card_sector = args[0];
                break;

            case 's': {
                if (args[1] >= 0x800000) {
                    cfg_set_error(CFG_ERROR_BAD_ARGUMENT);
                    return;
                }
                if (cfg_translate_address(&args[0], args[1] * SD_SECTOR_SIZE, (SDRAM | FLASH | BRAM))) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                led_activity_on();
                bool error = sd_read_sectors(args[0], p.sd_card_sector, args[1]);
                led_activity_off();
                if (error) {
                    cfg_set_error(CFG_ERROR_SD_CARD);
                    return;
                }
                p.sd_card_sector += args[1];
                break;
            }

            case 'S': {
                if (args[1] >= 0x800000) {
                    cfg_set_error(CFG_ERROR_BAD_ARGUMENT);
                    return;
                }
                if (cfg_translate_address(&args[0], args[1] * SD_SECTOR_SIZE, (SDRAM | FLASH | BRAM))) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                led_activity_on();
                bool error = sd_write_sectors(args[0], p.sd_card_sector, args[1]);
                led_activity_off();
                if (error) {
                    cfg_set_error(CFG_ERROR_SD_CARD);
                    return;
                }
                p.sd_card_sector += args[1];
                break;
            }

            case 'D':
                if (cfg_translate_address(&args[0], args[1], (SDRAM | BRAM))) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                dd_set_disk_mapping(args[0], args[1]);
                break;

            case 'w':
                args[0] = writeback_pending();
                break;

            case 'W':
                if (cfg_translate_address(&args[0], WRITEBACK_SECTOR_TABLE_SIZE, (SDRAM | BRAM))) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                writeback_load_sector_table(args[0]);
                writeback_enable(WRITEBACK_SD);
                break;

            case 'K':
                if (args[1] >= DATA_BUFFER_SIZE) {
                    cfg_set_error(CFG_ERROR_BAD_ARGUMENT);
                    return;
                }
                if (cfg_translate_address(&args[0], args[1], FLASH)) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                if (flash_program(DATA_BUFFER_ADDRESS, args[0], args[1])) {
                    cfg_set_error(CFG_ERROR_BAD_ARGUMENT);
                    return;
                }
                break;

            case 'p':
                if (args[0]) {
                    flash_wait_busy();
                }
                args[0] = FLASH_ERASE_BLOCK_SIZE;
                break;

            case 'P':
                if (cfg_translate_address(&args[0], FLASH_ERASE_BLOCK_SIZE, FLASH)) {
                    cfg_set_error(CFG_ERROR_BAD_ADDRESS);
                    return;
                }
                if (flash_erase_block(args[0])) {
                    cfg_set_error(CFG_ERROR_BAD_ARGUMENT);
                    return;
                }
                break;

            case '%':
                if (cfg_read_diagnostic_data(args)) {
                    cfg_set_error(CFG_ERROR_BAD_CONFIG_ID);
                    return;
                }
                break;

            default:
                cfg_set_error(CFG_ERROR_UNKNOWN_CMD);
                return;
        }

        fpga_reg_set(REG_CFG_DATA_0, args[0]);
        fpga_reg_set(REG_CFG_DATA_1, args[1]);
        fpga_reg_set(REG_CFG_CMD, CFG_CMD_DONE);
    }
}

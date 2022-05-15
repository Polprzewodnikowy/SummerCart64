#include <stdbool.h>
#include "cfg.h"
#include "flash.h"
#include "fpga.h"
#include "rtc.h"


typedef enum {
    CFG_ID_BOOTLOADER_SWITCH,
    CFG_ID_ROM_WRITE_ENABLE,
    CFG_ID_ROM_SHADOW_ENABLE,
    CFG_ID_DD_ENABLE,
    CFG_ID_ISV_ENABLE,
    CFG_ID_BOOT_MODE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_FLASH_ERASE_BLOCK,
} cfg_id_t;

typedef enum {
    SAVE_TYPE_NONE = 0,
    SAVE_TYPE_EEPROM_4K = 1,
    SAVE_TYPE_EEPROM_16K = 2,
    SAVE_TYPE_SRAM = 3,
    SAVE_TYPE_FLASHRAM = 4,
    SAVE_TYPE_SRAM_BANKED = 5
} save_type_t;

typedef enum {
    BOOT_MODE_MENU_SD = 0,
    BOOT_MODE_MENU_USB = 1,
    BOOT_MODE_ROM = 2,
    BOOT_MODE_DD = 3,
    BOOT_MODE_DIRECT = 4
} boot_mode_t;


struct process {
    boot_mode_t boot_mode;
    save_type_t save_type;
    uint16_t cic_seed;
    uint8_t tv_type;
};

static struct process p;


static void change_scr_bits (uint32_t mask, bool value) {
    if (value) {
        fpga_reg_set(REG_CFG_SCR, fpga_reg_get(REG_CFG_SCR) | mask);
    } else {
        fpga_reg_set(REG_CFG_SCR, fpga_reg_get(REG_CFG_SCR) & (~mask));
    }
}

static void set_save_type (save_type_t save_type) {
    uint32_t save_reset_mask = (
        CFG_SCR_EEPROM_16K |
        CFG_SCR_EEPROM_ENABLED |
        CFG_SCR_FLASHRAM_ENABLED |
        CFG_SCR_SRAM_BANKED |
        CFG_SCR_SRAM_ENABLED
    );

    change_scr_bits(save_reset_mask, false);

    switch (save_type) {
        case SAVE_TYPE_NONE:
            break;
        case SAVE_TYPE_EEPROM_4K:
            change_scr_bits(CFG_SCR_EEPROM_ENABLED, true);
            break;
        case SAVE_TYPE_EEPROM_16K:
            change_scr_bits(CFG_SCR_EEPROM_16K | CFG_SCR_EEPROM_ENABLED, true);
            break;
        case SAVE_TYPE_SRAM:
            change_scr_bits(CFG_SCR_SRAM_ENABLED, true);
            break;
        case SAVE_TYPE_FLASHRAM:
            change_scr_bits(CFG_SCR_FLASHRAM_ENABLED, true);
            break;
        case SAVE_TYPE_SRAM_BANKED:
            change_scr_bits(CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_ENABLED, true);
            break;
        default:
            save_type = SAVE_TYPE_NONE;
            break;
    }

    p.save_type = save_type;
}


uint32_t cfg_get_version (void) {
    return fpga_reg_get(REG_CFG_VERSION);
}

void cfg_query (uint32_t *args) {
    switch (args[0]) {
        case CFG_ID_BOOTLOADER_SWITCH:
            args[1] = (fpga_reg_get(REG_CFG_SCR) & CFG_SCR_BOOTLOADER_ENABLED);
            break;
        case CFG_ID_ROM_WRITE_ENABLE:
            args[1] = (fpga_reg_get(REG_CFG_SCR) & CFG_SCR_ROM_WRITE_ENABLED);
            break;
        case CFG_ID_ROM_SHADOW_ENABLE:
            args[1] = (fpga_reg_get(REG_CFG_SCR) & CFG_SCR_ROM_SHADOW_ENABLED);
            break;
        case CFG_ID_DD_ENABLE:
            args[1] = (fpga_reg_get(REG_CFG_SCR) & CFG_SCR_DD_ENABLED);
            break;
        case CFG_ID_ISV_ENABLE:
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
        case CFG_ID_FLASH_ERASE_BLOCK:
            break;
    }
}

void cfg_update (uint32_t *args) {
    switch (args[0]) {
        case CFG_ID_BOOTLOADER_SWITCH:
            change_scr_bits(CFG_SCR_BOOTLOADER_ENABLED, args[1]);
            break;
        case CFG_ID_ROM_WRITE_ENABLE:
            change_scr_bits(CFG_SCR_ROM_WRITE_ENABLED, args[1]);
            break;
        case CFG_ID_ROM_SHADOW_ENABLE:
            change_scr_bits(CFG_SCR_ROM_SHADOW_ENABLED, args[1]);
            break;
        case CFG_ID_DD_ENABLE:
            change_scr_bits(CFG_SCR_DD_ENABLED, args[1]);
            break;
        case CFG_ID_ISV_ENABLE:
            break;
        case CFG_ID_BOOT_MODE:
            p.boot_mode = args[1];
            change_scr_bits(CFG_SCR_BOOTLOADER_SKIP, (args[1] == BOOT_MODE_DIRECT));
            break;
        case CFG_ID_SAVE_TYPE:
            set_save_type((save_type_t) (args[1]));
            break;
        case CFG_ID_CIC_SEED:
            p.cic_seed = (uint16_t) (args[1] & 0xFFFF);
            break;
        case CFG_ID_TV_TYPE:
            p.tv_type = (uint8_t) (args[1] & 0x03);
            break;
        case CFG_ID_FLASH_ERASE_BLOCK:
            flash_erase_block(args[1]);
            break;
    }
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
    t.weekday = ((args[1] >> 24) & 0xFF);
    t.day = (args[1] & 0xFF);
    t.month = ((args[1] >> 8) & 0xFF);
    t.year = ((args[1] >> 16) & 0xFF);
    rtc_set_time(&t);
}


void cfg_init (void) {
    // fpga_reg_set(REG_CFG_SCR, 0);
    set_save_type(SAVE_TYPE_NONE);

    p.cic_seed = 0xFFFF;
    p.tv_type = 0x03;
    p.boot_mode = BOOT_MODE_MENU_SD;
}

#include "stm32g030xx.h"

void uart_print (const char *str) {
    while (*str != '\0') {
        while (!(USART1->ISR & USART_ISR_TXE_TXFNF));
        USART1->TDR = *str++;
    }
}

void cfg_process (void) {
    uint32_t args[2];

    if (fpga_reg_get(REG_STATUS) & STATUS_CFG_PENDING) {
        args[0] = fpga_reg_get(REG_CFG_DATA_0);
        args[1] = fpga_reg_get(REG_CFG_DATA_1);
        char cmd = (char) fpga_reg_get(REG_CFG_CMD);
        uart_print("GOT_CMD");
        char tmp[2] = {cmd, 0};
        uart_print(tmp);
        uart_print("\n");
        switch (cmd) {
            case 'v':
                args[0] = cfg_get_version();
                break;

            case 'c':
                cfg_query(args);
                break;

            case 'C':
                cfg_update(args);
                break;

            case 't':
                cfg_get_time(args);
                break;

            case 'T':
                cfg_set_time(args);
                break;

            case 'U':
                // uart_put((char) (args[0] & 0xFF));
                break;

            default:
                fpga_reg_set(REG_CFG_CMD, CFG_CMD_ERROR | CFG_CMD_DONE);
                return;
        }

        fpga_reg_set(REG_CFG_DATA_0, args[0]);
        fpga_reg_set(REG_CFG_DATA_1, args[1]);
        fpga_reg_set(REG_CFG_CMD, CFG_CMD_DONE);
    }
}

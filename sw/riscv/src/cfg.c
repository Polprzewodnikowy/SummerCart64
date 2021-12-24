#include "cfg.h"
#include "dd.h"
#include "flash.h"
#include "joybus.h"
#include "usb.h"


#define SAVE_SIZE_EEPROM_4K     (512)
#define SAVE_SIZE_EEPROM_16K    (2048)
#define SAVE_SIZE_SRAM          (32 * 1024)
#define SAVE_SIZE_FLASHRAM      (128 * 1024)
#define SAVE_SIZE_SRAM_BANKED   (3 * 32 * 1024)

#define SAVE_OFFSET_PKST2       (0x01608000UL)

#define DEFAULT_SAVE_OFFSET     (0x03FE0000UL)
#define DEFAULT_DDIPL_OFFSET    (0x03BE0000UL)


enum cfg_id {
    CFG_ID_SCR,
    CFG_ID_SDRAM_SWITCH,
    CFG_ID_SDRAM_WRITABLE,
    CFG_ID_DD_ENABLE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_SAVE_OFFEST,
    CFG_ID_DDIPL_OFFEST,
    CFG_ID_BOOT_MODE,
    CFG_ID_FLASH_SIZE,
    CFG_ID_FLASH_READ,
    CFG_ID_FLASH_PROGRAM,
    CFG_ID_RECONFIGURE,
    CFG_ID_DD_SETTING,
    CFG_ID_DD_THB_TABLE_OFFSET,
};

enum save_type {
    SAVE_TYPE_NONE = 0,
    SAVE_TYPE_EEPROM_4K = 1,
    SAVE_TYPE_EEPROM_16K = 2,
    SAVE_TYPE_SRAM = 3,
    SAVE_TYPE_FLASHRAM = 4,
    SAVE_TYPE_SRAM_BANKED = 5,
    SAVE_TYPE_FLASHRAM_PKST2 = 6,
};

enum boot_mode {
    BOOT_MODE_MENU = 0,
    BOOT_MODE_ROM = 1,
    BOOT_MODE_DD = 2,
    BOOT_MODE_DIRECT = 3,
};

enum dd_setting {
    DD_SETTING_DISK_EJECTED = 0,
    DD_SETTING_DISK_INSERTED = 1,
    DD_SETTING_DISK_CHANGED = 2,
    DD_SETTING_DRIVE_RETAIL = 3,
    DD_SETTING_DRIVE_DEVELOPMENT = 4,
    DD_SETTING_SET_BLOCK_READY = 5,
};


struct process {
    enum save_type save_type;
    uint16_t cic_seed;
    uint8_t tv_type;
    enum boot_mode boot_mode;
};

static struct process p;


static void change_scr_bits (uint32_t mask, bool value) {
    if (value) {
        CFG->SCR |= mask;
    } else {
        CFG->SCR &= ~(mask);
    }
}

static void set_save_type (enum save_type save_type) {
    uint32_t save_offset = DEFAULT_SAVE_OFFSET;

    change_scr_bits(CFG_SCR_FLASHRAM_EN | CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_EN, false);
    joybus_set_eeprom(EEPROM_NONE);

    switch (save_type) {
        case SAVE_TYPE_NONE:
            break;
        case SAVE_TYPE_EEPROM_4K:
            save_offset = SDRAM_SIZE - SAVE_SIZE_EEPROM_4K;
            joybus_set_eeprom(EEPROM_4K);
            break;
        case SAVE_TYPE_EEPROM_16K:
            save_offset = SDRAM_SIZE - SAVE_SIZE_EEPROM_16K;
            joybus_set_eeprom(EEPROM_16K);
            break;
        case SAVE_TYPE_SRAM:
            save_offset = SDRAM_SIZE - SAVE_SIZE_SRAM;
            change_scr_bits(CFG_SCR_SRAM_EN, true);
            break;
        case SAVE_TYPE_FLASHRAM:
            save_offset = SDRAM_SIZE - SAVE_SIZE_FLASHRAM;
            change_scr_bits(CFG_SCR_FLASHRAM_EN, true);
            break;
        case SAVE_TYPE_SRAM_BANKED:
            save_offset = SDRAM_SIZE - SAVE_SIZE_SRAM_BANKED;
            change_scr_bits(CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_EN, true);
            break;
        case SAVE_TYPE_FLASHRAM_PKST2:
            save_offset = SAVE_OFFSET_PKST2;
            change_scr_bits(CFG_SCR_FLASHRAM_EN, true);
            break;
        default:
            save_type = SAVE_TYPE_NONE;
            break;
    }

    p.save_type = save_type;

    CFG->SAVE_OFFSET = save_offset;
}

static void set_dd_setting (enum dd_setting setting) {
    switch (setting) {
        case DD_SETTING_DISK_EJECTED:
            dd_set_disk_state(DD_DISK_EJECTED);
            break;
        case DD_SETTING_DISK_INSERTED:
            dd_set_disk_state(DD_DISK_INSERTED);
            break;
        case DD_SETTING_DISK_CHANGED:
            dd_set_disk_state(DD_DISK_CHANGED);
            break;
        case DD_SETTING_DRIVE_RETAIL:
            dd_set_drive_type_development(false);
            break;
        case DD_SETTING_DRIVE_DEVELOPMENT:
            dd_set_drive_type_development(true);
            break;
        case DD_SETTING_SET_BLOCK_READY:
            dd_set_block_ready(true);
            break;
    }
}


uint32_t cfg_get_version (void) {
    return CFG->VERSION;
}

void cfg_update (uint32_t *args) {
    switch (args[0]) {
        case CFG_ID_SCR:
            CFG->SCR = args[1];
            break;
        case CFG_ID_SDRAM_SWITCH:
            change_scr_bits(CFG_SCR_SDRAM_SWITCH, args[1]);
            break;
        case CFG_ID_SDRAM_WRITABLE:
            change_scr_bits(CFG_SCR_SDRAM_WRITABLE, args[1]);
            break;
        case CFG_ID_DD_ENABLE:
            change_scr_bits(CFG_SCR_DD_EN, args[1]);
            break;
        case CFG_ID_SAVE_TYPE:
            set_save_type((enum save_type) (args[1]));
            break;
        case CFG_ID_CIC_SEED:
            p.cic_seed = (uint16_t) (args[1] & 0xFFFF);
            break;
        case CFG_ID_TV_TYPE:
            p.tv_type = (uint8_t) (args[1] & 0x03);
            break;
        case CFG_ID_SAVE_OFFEST:
            CFG->SAVE_OFFSET = args[1];
            break;
        case CFG_ID_DDIPL_OFFEST:
            CFG->DDIPL_OFFSET = args[1];
            break;
        case CFG_ID_BOOT_MODE:
            p.boot_mode = args[1];
            change_scr_bits(CFG_SCR_SKIP_BOOTLOADER, args[1] == BOOT_MODE_DIRECT);
            break;
        case CFG_ID_FLASH_READ:
            flash_read(args[1]);
            break;
        case CFG_ID_FLASH_PROGRAM:
            flash_program(args[1]);
            break;
        case CFG_ID_RECONFIGURE:
            if (args[1] == CFG->RECONFIGURE) {
                CFG->RECONFIGURE = args[1];
                asm volatile (
                    "ebreak \n"
                );
            }
            break;
        case CFG_ID_DD_SETTING:
            set_dd_setting(args[1]);
            break;
    }
}

void cfg_query (uint32_t *args) {
    switch (args[0]) {
        case CFG_ID_SCR:
            args[1] = CFG->SCR;
            break;
        case CFG_ID_SDRAM_SWITCH:
            args[1] = CFG->SCR & CFG_SCR_SDRAM_SWITCH;
            break;
        case CFG_ID_SDRAM_WRITABLE:
            args[1] = CFG->SCR & CFG_SCR_SDRAM_WRITABLE;
            break;
        case CFG_ID_DD_ENABLE:
            args[1] = CFG->SCR & CFG_SCR_DD_EN;
            break;
        case CFG_ID_SAVE_TYPE:
            args[1] = (uint32_t) (p.save_type);
            break;
        case CFG_ID_CIC_SEED:
            args[1] = (uint32_t) (p.cic_seed);
            break;
        case CFG_ID_TV_TYPE:
            args[1] = (uint32_t) (p.tv_type);
            break;
        case CFG_ID_SAVE_OFFEST:
            args[1] = CFG->SAVE_OFFSET;
            break;
        case CFG_ID_DDIPL_OFFEST:
            args[1] = CFG->DDIPL_OFFSET;
            break;
        case CFG_ID_BOOT_MODE:
            args[1] = p.boot_mode;
            break;
        case CFG_ID_FLASH_SIZE:
            args[1] = flash_size();
            break;
        case CFG_ID_RECONFIGURE:
            args[1] = CFG->RECONFIGURE;
            break;
        case CFG_ID_DD_THB_TABLE_OFFSET:
            args[1] = dd_get_thb_table_offset();
            break;
    }
}


void cfg_init (void) {
    set_save_type(SAVE_TYPE_NONE);

    CFG->DDIPL_OFFSET = DEFAULT_DDIPL_OFFSET;
    CFG->SCR = CFG_SCR_CPU_READY | CFG_SCR_SDRAM_SWITCH;

    p.cic_seed = 0xFFFF;
    p.tv_type = 0x03;
    p.boot_mode = BOOT_MODE_MENU;
}


void process_cfg (void) {
    uint32_t args[2];

    if (CFG->SCR & CFG_SCR_CPU_BUSY) {
        change_scr_bits(CFG_SCR_CMD_ERROR, false);

        args[0] = CFG->DATA[0];
        args[1] = CFG->DATA[1];

        switch (CFG->CMD) {
            case 'C':
                cfg_update(args);
                break;

            case 'Q':
                cfg_query(args);
                break;

            case 'S':
                args[0] = usb_debug_tx_ready();
                break;

            case 'D':
                if (!usb_debug_tx_data(args[0], (size_t) args[1])) {
                    change_scr_bits(CFG_SCR_CMD_ERROR, true);
                }
                break;

            case 'A':
                if (!usb_debug_rx_ready(&args[0], (size_t *) (&args[1]))) {
                    args[0] = 0;
                    args[1] = 0;
                }
                break;

            case 'F':
                args[0] = usb_debug_rx_busy();
                break;
            
            case 'E':
                if (!usb_debug_rx_data(args[0], (size_t) args[1])) {
                    change_scr_bits(CFG_SCR_CMD_ERROR, true);
                }
                break;

            case 'B':
                usb_debug_reset();
                break;

            default:
                change_scr_bits(CFG_SCR_CMD_ERROR, true);
                break;
        }

        CFG->DATA[0] = args[0];
        CFG->DATA[1] = args[1];

        change_scr_bits(CFG_SCR_CPU_BUSY, false);
    }
}

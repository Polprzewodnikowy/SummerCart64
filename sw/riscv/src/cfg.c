#include "cfg.h"
#include "dd.h"
#include "flash.h"
#include "isv.h"
#include "joybus.h"
#include "rtc.h"
#include "uart.h"
#include "usb.h"


#define SAVE_SIZE_EEPROM_4K     (512)
#define SAVE_SIZE_EEPROM_16K    (2048)
#define SAVE_SIZE_SRAM          (32 * 1024)
#define SAVE_SIZE_FLASHRAM      (128 * 1024)
#define SAVE_SIZE_SRAM_BANKED   (3 * 32 * 1024)

#define ISV_SIZE                (64 * 1024)

#define SAVE_OFFSET_PKST2       (0x01608000UL)

#define DEFAULT_SAVE_OFFSET     (0x03FD0000UL)
#define DEFAULT_DDIPL_OFFSET    (0x03BD0000UL)


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
    CFG_ID_DD_DRIVE_ID,
    CFG_ID_DD_DISK_STATE,
    CFG_ID_DD_THB_TABLE_OFFSET,
    CFG_ID_IS_VIEWER_ENABLE,
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
    BOOT_MODE_MENU_SD = 0,
    BOOT_MODE_MENU_USB = 1,
    BOOT_MODE_ROM = 2,
    BOOT_MODE_DD = 3,
    BOOT_MODE_DIRECT = 4,
};


struct process {
    enum save_type save_type;
    uint16_t cic_seed;
    uint8_t tv_type;
    enum boot_mode boot_mode;
    bool usb_drive_busy;
};

static struct process p;


static void change_scr_bits (uint32_t mask, bool value) {
    if (value) {
        CFG->SCR |= mask;
    } else {
        CFG->SCR &= ~(mask);
    }
}

static void set_usb_drive_not_busy (void) {
    p.usb_drive_busy = false;
}

static void set_save_type (enum save_type save_type) {
    uint32_t save_offset = DEFAULT_SAVE_OFFSET;

    change_scr_bits(CFG_SCR_FLASHRAM_EN | CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_EN, false);
    joybus_set_eeprom(EEPROM_NONE);

    switch (save_type) {
        case SAVE_TYPE_NONE:
            break;
        case SAVE_TYPE_EEPROM_4K:
            save_offset = SDRAM_SIZE - SAVE_SIZE_EEPROM_4K - ISV_SIZE;
            joybus_set_eeprom(EEPROM_4K);
            break;
        case SAVE_TYPE_EEPROM_16K:
            save_offset = SDRAM_SIZE - SAVE_SIZE_EEPROM_16K - ISV_SIZE;
            joybus_set_eeprom(EEPROM_16K);
            break;
        case SAVE_TYPE_SRAM:
            save_offset = SDRAM_SIZE - SAVE_SIZE_SRAM - ISV_SIZE;
            change_scr_bits(CFG_SCR_SRAM_EN, true);
            break;
        case SAVE_TYPE_FLASHRAM:
            save_offset = SDRAM_SIZE - SAVE_SIZE_FLASHRAM - ISV_SIZE;
            change_scr_bits(CFG_SCR_FLASHRAM_EN, true);
            break;
        case SAVE_TYPE_SRAM_BANKED:
            save_offset = SDRAM_SIZE - SAVE_SIZE_SRAM_BANKED - ISV_SIZE;
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


uint32_t cfg_get_version (void) {
    return CFG->VERSION;
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
        case CFG_ID_DD_DISK_STATE:
            args[1] = dd_get_disk_state();
            break;
        case CFG_ID_DD_DRIVE_ID:
            args[1] = dd_get_drive_id();
            break;
        case CFG_ID_DD_THB_TABLE_OFFSET:
            args[1] = dd_get_thb_table_offset();
            break;
        case CFG_ID_IS_VIEWER_ENABLE:
            args[1] = isv_get_enabled();
            break;
    }
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
        case CFG_ID_DD_DISK_STATE:
            dd_set_disk_state(args[1]);
            break;
        case CFG_ID_DD_DRIVE_ID:
            dd_set_drive_id((uint16_t) (args[1]));
            break;
        case CFG_ID_DD_THB_TABLE_OFFSET:
            dd_set_thb_table_offset(args[1]);
            break;
        case CFG_ID_IS_VIEWER_ENABLE:
            isv_set_enabled(args[1]);
            break;
    }
}

void cfg_get_time (uint32_t *args) {
    rtc_time_t *t = rtc_get_time();
    args[0] = ((t->hour << 16) | (t->minute << 8) | t->second);
    args[1] = ((t->weekday << 24) | (t->year << 16) | (t->month << 8) | t->day);
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
    set_save_type(SAVE_TYPE_NONE);

    CFG->DDIPL_OFFSET = DEFAULT_DDIPL_OFFSET;
    CFG->SCR = CFG_SCR_CPU_READY;

    p.cic_seed = 0xFFFF;
    p.tv_type = 0x03;
    p.boot_mode = BOOT_MODE_MENU_SD;
    p.usb_drive_busy = false;
}


void process_cfg (void) {
    uint32_t args[2];

    if (CFG->SCR & CFG_SCR_CPU_BUSY) {
        change_scr_bits(CFG_SCR_CMD_ERROR, false);

        args[0] = CFG->DATA[0];
        args[1] = CFG->DATA[1];

        switch (CFG->CMD) {
            case 'V':
                args[0] = cfg_get_version();
                break;

            case 'Q':
                cfg_query(args);
                break;

            case 'C':
                cfg_update(args);
                break;

            case 0xEE:
                cfg_get_time(args);
                break;

            case 0xEF:
                cfg_set_time(args);
                break;

            case 0xF0:
                if (args[0] & (1 << 31)) {
                    p.usb_drive_busy = false;
                } else {
                    change_scr_bits(CFG_SCR_CMD_ERROR, true);
                }
                break;

            case 0xF1:
                if (args[0] & (1 << 31)) {
                    args[0] = p.usb_drive_busy;
                } else {
                    args[0] = 0;
                    change_scr_bits(CFG_SCR_CMD_ERROR, true);
                }
                args[1] = 0;
                break;

            case 0xF2:
                if (args[0] & (1 << 31)) {
                    if (!p.usb_drive_busy) {
                        usb_event_t event;
                        event.id = EVENT_ID_FSD_READ;
                        event.trigger = CALLBACK_SDRAM_WRITE;
                        event.callback = set_usb_drive_not_busy;
                        uint32_t data[3] = { args[1], (args[0] & 0x7FFFFFFF), 1 };
                        if (usb_put_event(&event, data, sizeof(data))) {
                            p.usb_drive_busy = true;
                        } else {
                            return;
                        }
                    } else {
                        change_scr_bits(CFG_SCR_CMD_ERROR, true);
                    }
                } else {
                    change_scr_bits(CFG_SCR_CMD_ERROR, true);
                }
                break;

            case 0xF3:
                if (args[0] & (1 << 31)) {
                    if (!p.usb_drive_busy) {
                        usb_event_t event;
                        event.id = EVENT_ID_FSD_WRITE;
                        event.trigger = CALLBACK_SDRAM_READ;
                        event.callback = set_usb_drive_not_busy;
                        uint32_t data[3] = { args[1], (args[0] & 0x7FFFFFFF), 1 };
                        if (usb_put_event(&event, data, sizeof(data))) {
                            p.usb_drive_busy = true;
                        } else {
                            return;
                        }
                    } else {
                        change_scr_bits(CFG_SCR_CMD_ERROR, true);
                    }
                } else {
                    change_scr_bits(CFG_SCR_CMD_ERROR, true);
                }
                break;

            case 0xFF:
                uart_put((char) (args[0] & 0xFF));
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

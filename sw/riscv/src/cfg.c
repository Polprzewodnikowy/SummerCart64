#include "cfg.h"
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
#define DEFAULT_DD_OFFSET       (0x03BE0000UL)


enum cfg_id {
    CFG_ID_SCR,
    CFG_ID_SDRAM_SWITCH,
    CFG_ID_SDRAM_WRITABLE,
    CFG_ID_DD_ENABLE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_SAVE_OFFEST,
    CFG_ID_DD_OFFEST,
    CFG_ID_SKIP_BOOTLOADER,
    CFG_ID_FLASH_OPERATION,
    CFG_ID_RECONFIGURE,
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


struct process {
    enum save_type save_type;
    uint16_t cic_seed;
    uint8_t tv_type;
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
        case CFG_ID_DD_OFFEST:
            CFG->DD_OFFSET = args[1];
            break;
        case CFG_ID_SKIP_BOOTLOADER:
            change_scr_bits(CFG_SCR_SKIP_BOOTLOADER, args[1]);
            break;
        case CFG_ID_FLASH_OPERATION:
            flash_program(args[1]);
            break;
        case CFG_ID_RECONFIGURE:
            if (args[1] == CFG->RECONFIGURE) {
                CFG->RECONFIGURE = args[1];
                __asm__ volatile (
                    "ebreak \n"
                );
            }
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
        case CFG_ID_DD_OFFEST:
            args[1] = CFG->DD_OFFSET;
            break;
        case CFG_ID_SKIP_BOOTLOADER:
            args[1] = CFG->SCR & CFG_SCR_SKIP_BOOTLOADER;
            break;
        case CFG_ID_FLASH_OPERATION:
            args[1] = flash_read(args[1]);
            break;
        case CFG_ID_RECONFIGURE:
            args[1] = CFG->RECONFIGURE;
            break;
    }
}


void cfg_init (void) {
    set_save_type(SAVE_TYPE_NONE);

    CFG->DD_OFFSET = DEFAULT_DD_OFFSET;
    CFG->SCR = CFG_SCR_CPU_READY | CFG_SCR_SDRAM_SWITCH;

    p.cic_seed = 0xFFFF;
    p.tv_type = 0x03;
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

#include "cfg.h"
#include "fpga.h"
#include "sd.h"
#include "timer.h"
#include "usb.h"
#include "writeback.h"


#define SAVE_MAX_SECTOR_COUNT       (256)
#define EEPROM_ADDRESS              (0x05002000)
#define SRAM_FLASHRAM_ADDRESS       (0x03FE0000)
#define EEPROM_4K_LENGTH            (512)
#define EEPROM_16K_LENGTH           (2048)
#define SRAM_LENGTH                 (32 * 1024)
#define FLASHRAM_LENGTH             (128 * 1024)
#define SRAM_BANKED_LENGTH          (3 * 32 * 1024)
#define SRAM_1M_LENGTH              (128 * 1024)
#define WRITEBACK_DELAY_TICKS       (100)


struct process {
    bool enabled;
    bool pending;
    writeback_mode_t mode;
    uint16_t last_save_count;
    uint32_t sectors[SAVE_MAX_SECTOR_COUNT];
};


static struct process p;


static save_type_t writeback_get_address_length (uint32_t *address, uint32_t *length) {
    save_type_t save = cfg_get_save_type();

    switch (save) {
        case SAVE_TYPE_EEPROM_4K:
            *address = EEPROM_ADDRESS;
            *length = EEPROM_4K_LENGTH;
            break;
        case SAVE_TYPE_EEPROM_16K:
            *address = EEPROM_ADDRESS;
            *length = EEPROM_16K_LENGTH;
            break;
        case SAVE_TYPE_SRAM:
            *address = SRAM_FLASHRAM_ADDRESS;
            *length = SRAM_LENGTH;
            break;
        case SAVE_TYPE_FLASHRAM:
            *address = SRAM_FLASHRAM_ADDRESS;
            *length = FLASHRAM_LENGTH;
            break;
        case SAVE_TYPE_SRAM_BANKED:
            *address = SRAM_FLASHRAM_ADDRESS;
            *length = SRAM_BANKED_LENGTH;
            break;
        case SAVE_TYPE_SRAM_1M:
            *address = SRAM_FLASHRAM_ADDRESS;
            *length = SRAM_1M_LENGTH;
            break;
        default:
            *address = 0;
            *length = 0;
            break;
    }

    return save;
}

static void writeback_save_to_sd (void) {
    save_type_t save;
    uint32_t address;
    uint32_t length;

    save = writeback_get_address_length(&address, &length);
    if (save == SAVE_TYPE_NONE) {
        writeback_disable();
        return;
    }

    if(sd_optimize_sectors(address, p.sectors, length / SD_SECTOR_SIZE, sd_write_sectors)) {
        writeback_disable();
    }
}

static bool writeback_save_to_usb (void) {
    save_type_t save;
    uint32_t address;
    uint32_t length;

    save = writeback_get_address_length(&address, &length);
    if (save == SAVE_TYPE_NONE) {
        writeback_disable();
        return true;
    }

    usb_tx_info_t packet_info;
    usb_create_packet(&packet_info, PACKET_CMD_SAVE_WRITEBACK);
    packet_info.data_length = 4;
    packet_info.data[0] = save;
    packet_info.dma_length = length;
    packet_info.dma_address = address;
    return usb_enqueue_packet(&packet_info);
}


void writeback_load_sector_table (uint32_t address) {
    fpga_mem_read(address, sizeof(p.sectors), (uint8_t *) (p.sectors));
    for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
        p.sectors[i] = SWAP32(p.sectors[i]);
    }
}

void writeback_enable (writeback_mode_t mode) {
    p.enabled = true;
    p.pending = false;
    p.mode = mode;
    p.last_save_count = fpga_reg_get(REG_SAVE_COUNT);
}

void writeback_disable (void) {
    p.enabled = false;
    p.pending = false;
    timer_set(TIMER_ID_WRITEBACK, 0);
}

bool writeback_pending (void) {
    return p.enabled && p.pending;
}

void writeback_init (void) {
    p.enabled = false;
    p.pending = false;
    p.mode = WRITEBACK_SD;
    for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
        p.sectors[i] = 0;
    }
}

void writeback_process (void) {
    if (p.enabled && (p.mode == WRITEBACK_SD) && !sd_card_is_inserted()) {
        writeback_disable();
    }

    if (p.enabled) {
        uint16_t save_count = fpga_reg_get(REG_SAVE_COUNT);
        if (save_count != p.last_save_count) {
            p.pending = true;
            p.last_save_count = save_count;
            timer_set(TIMER_ID_WRITEBACK, WRITEBACK_DELAY_TICKS);
        }
    }

    if (p.pending && (timer_get(TIMER_ID_WRITEBACK) == 0)) {
        switch (p.mode) {
            case WRITEBACK_SD:
                writeback_save_to_sd();
                p.pending = false;
                break;
            case WRITEBACK_USB:
                if (writeback_save_to_usb()) {
                    p.pending = false;
                }
                break;
            default:
                writeback_disable();
                break;
        }
    }
}

#include "cfg.h"
#include "fpga.h"
#include "sd.h"
#include "timer.h"
#include "writeback.h"


#define SAVE_MAX_SECTOR_COUNT       (256)
#define EEPROM_ADDRESS              (0x05002000)
#define SRAM_FLASHRAM_ADDRESS       (0x03FE0000)
#define EEPROM_4K_SECTOR_COUNT      (1)
#define EEPROM_16K_SECTOR_COUNT     (4)
#define SRAM_SECTOR_COUNT           (64)
#define FLASHRAM_SECTOR_COUNT       (256)
#define SRAM_BANKED_SECTOR_COUNT    (192)
#define WRITEBACK_DELAY_TICKS       (100)


struct process {
    bool enabled;
    bool pending;
    uint16_t last_save_count;
    uint32_t sectors[SAVE_MAX_SECTOR_COUNT];
};


static struct process p;


static void writeback_save_to_sd (void) {
    uint32_t address;
    uint32_t count;

    switch (cfg_get_save_type()) {
        case SAVE_TYPE_EEPROM_4K:
            address = EEPROM_ADDRESS;
            count = EEPROM_4K_SECTOR_COUNT;
            break;
        case SAVE_TYPE_EEPROM_16K:
            address = EEPROM_ADDRESS;
            count = EEPROM_16K_SECTOR_COUNT;
            break;
        case SAVE_TYPE_SRAM:
            address = SRAM_FLASHRAM_ADDRESS;
            count = SRAM_SECTOR_COUNT;
            break;
        case SAVE_TYPE_FLASHRAM:
            address = SRAM_FLASHRAM_ADDRESS;
            count = FLASHRAM_SECTOR_COUNT;
            break;
        case SAVE_TYPE_SRAM_BANKED:
            address = SRAM_FLASHRAM_ADDRESS;
            count = SRAM_BANKED_SECTOR_COUNT;
            break;
        default:
            writeback_disable();
            return;
    }

    if(sd_optimize_sectors(address, p.sectors, count, sd_write_sectors)) {
        writeback_disable();
    }
}


void writeback_load_sector_table (uint32_t address) {
    fpga_mem_read(address, sizeof(p.sectors), (uint8_t *) (p.sectors));
    for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
        p.sectors[i] = SWAP32(p.sectors[i]);
    }
}

void writeback_enable (void) {
    p.enabled = true;
    p.pending = false;
    p.last_save_count = fpga_reg_get(REG_SAVE_COUNT);
}

void writeback_disable (void) {
    p.enabled = false;
    p.pending = false;
    timer_set(TIMER_ID_WRITEBACK, 0);
}

void writeback_init (void) {
    p.enabled = false;
    p.pending = false;
    for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
        p.sectors[i] = 0;
    }
}

void writeback_process (void) {
    if (p.enabled && !sd_card_is_inserted()) {
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
        p.pending = false;
        writeback_save_to_sd();
    }
}

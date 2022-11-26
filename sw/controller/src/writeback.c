#include "fpga.h"
#include "sd.h"
#include "timer.h"
#include "writeback.h"
#include "led.h"


#define SAVE_MAX_SECTOR_COUNT   (256)
#define SRAM_FLASHRAM_ADDRESS   (0x03FE0000)
#define EEPROM_ADDRESS          (0x05002000)
#define WRITEBACK_DELAY_TICKS   (100)


struct process {
    bool enabled;
    bool pending;
    uint16_t last_save_count;
    uint32_t sectors[SAVE_MAX_SECTOR_COUNT];
};


static struct process p;


static void writeback_save_to_sd (void) {
    uint32_t save_address = SRAM_FLASHRAM_ADDRESS;
    if (fpga_reg_get(REG_CFG_SCR) & CFG_SCR_EEPROM_ENABLED) {
        save_address = EEPROM_ADDRESS;
    }
    for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
        uint32_t sector = p.sectors[i];
        if (sector == 0) {
            break;
        }
        if (sd_write_sectors(save_address, sector, 1)) {
            break;
        }
        save_address += SD_SECTOR_SIZE;
    }
}


void writeback_set_sd_info (uint32_t address, bool enabled) {
    p.enabled = enabled;
    p.pending = false;
    p.last_save_count = fpga_reg_get(REG_SAVE_COUNT);
    if (p.enabled) {
        fpga_mem_read(address, sizeof(p.sectors), (uint8_t *) (p.sectors));
        for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
            p.sectors[i] = SWAP32(p.sectors[i]);
        }
    } else {
        for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
            p.sectors[i] = 0;
        }
    }
}

void writeback_init (void) {
    p.enabled = false;
    p.pending = false;
    for (int i = 0; i < SAVE_MAX_SECTOR_COUNT; i++) {
        p.sectors[i] = 0;
    }
}

void writeback_process (void) {
    if (p.enabled) {
        uint16_t save_count = fpga_reg_get(REG_SAVE_COUNT);
        if (save_count != p.last_save_count) {
            p.pending = true;
            timer_set(TIMER_ID_WRITEBACK, WRITEBACK_DELAY_TICKS);
            p.last_save_count = save_count;
        }
    }
    if (p.pending) {
        if (timer_get(TIMER_ID_WRITEBACK) == 0) {
            p.pending = false;
            writeback_save_to_sd();
        }
    }
}

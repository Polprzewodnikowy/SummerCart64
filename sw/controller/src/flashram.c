#include <stdint.h>
#include "cfg.h"
#include "fpga.h"
#include "hw.h"
#include "timer.h"


#define FLASHRAM_SIZE               (128 * 1024)
#define FLASHRAM_SECTOR_SIZE        (16 * 1024)
#define FLASHRAM_PAGE_SIZE          (128)
#define FLASHRAM_ADDRESS            (0x03FE0000UL)
#define FLASHRAM_BUFFER_ADDRESS     (0x05002C00UL)

#define FLASHRAM_WRITE_TIMING_MS    (3)
#define FLASHRAM_ERASE_TIMING_MS    (200)


typedef enum {
    OP_NONE,
    OP_ERASE_ALL,
    OP_ERASE_SECTOR,
    OP_WRITE_PAGE
} flashram_op_t;

struct process {
    bool pending;
};


static struct process p;


static flashram_op_t flashram_operation_type (uint32_t scr) {
    if (!(scr & FLASHRAM_SCR_PENDING)) {
        return OP_NONE;
    }

    if (scr & FLASHRAM_SCR_WRITE_OR_ERASE) {
        if (scr & FLASHRAM_SCR_SECTOR_OR_ALL) {
            return OP_ERASE_ALL;
        } else {
            return OP_ERASE_SECTOR;
        }
    } else {
        return OP_WRITE_PAGE;
    }
}


void flashram_init (void) {
    if (fpga_reg_get(REG_FLASHRAM_SCR) & FLASHRAM_SCR_PENDING) {
        fpga_reg_set(REG_FLASHRAM_SCR, FLASHRAM_SCR_DONE);
    }
    p.pending = false;
}


void flashram_process (void) {
    uint32_t scr = fpga_reg_get(REG_FLASHRAM_SCR);

    flashram_op_t op = flashram_operation_type(scr);

    if (op == OP_NONE) {
        return;
    }

    uint8_t write_buffer[FLASHRAM_PAGE_SIZE];

    uint32_t page = ((scr & FLASHRAM_SCR_PAGE_MASK) >> FLASHRAM_SCR_PAGE_BIT);
    const bool full_emulation = (cfg_get_save_type() != SAVE_TYPE_FLASHRAM_FAKE);

    if (p.pending) {
        if (timer_countdown_elapsed(TIMER_ID_FLASHRAM)) {
            p.pending = false;
        } else {
            return;
        }
    } else if (op == OP_WRITE_PAGE) {
        uint8_t page_buffer[FLASHRAM_PAGE_SIZE];

        uint32_t address = (FLASHRAM_ADDRESS + (page * FLASHRAM_PAGE_SIZE));

        fpga_mem_read(FLASHRAM_BUFFER_ADDRESS, FLASHRAM_PAGE_SIZE, page_buffer);
        fpga_mem_read(address, FLASHRAM_PAGE_SIZE, write_buffer);

        for (int i = 0; i < FLASHRAM_PAGE_SIZE; i++) {
            if (full_emulation) {
                write_buffer[i] &= page_buffer[i];
            } else {
                write_buffer[i] = page_buffer[i];
            }
        }

        fpga_mem_write(address, FLASHRAM_PAGE_SIZE, write_buffer);

        if (full_emulation) {
            hw_delay_ms(FLASHRAM_WRITE_TIMING_MS);
        }
    } else if ((op == OP_ERASE_SECTOR) || (op == OP_ERASE_ALL)) {        
        if (full_emulation) {
            p.pending = true;
            timer_countdown_start(TIMER_ID_FLASHRAM, FLASHRAM_ERASE_TIMING_MS);
        }

        for (int i = 0; i < FLASHRAM_PAGE_SIZE; i++) {
            write_buffer[i] = 0xFF;
        }

        page &= ~((FLASHRAM_SECTOR_SIZE / FLASHRAM_PAGE_SIZE) - 1);

        uint32_t erase_size = (op == OP_ERASE_ALL) ? FLASHRAM_SIZE : FLASHRAM_SECTOR_SIZE;
        uint32_t address = (FLASHRAM_ADDRESS + (page * FLASHRAM_PAGE_SIZE));

        for (uint32_t offset = 0; offset < erase_size; offset += FLASHRAM_PAGE_SIZE) {
            fpga_mem_write(address + offset, FLASHRAM_PAGE_SIZE, write_buffer);
        }

        if (full_emulation) {
            return;
        }
    }

    fpga_reg_set(REG_FLASHRAM_SCR, FLASHRAM_SCR_DONE);
}

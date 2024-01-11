#include <stdint.h>
#include "fpga.h"


#define FLASHRAM_SIZE           (128 * 1024)
#define FLASHRAM_SECTOR_SIZE    (16 * 1024)
#define FLASHRAM_PAGE_SIZE      (128)
#define FLASHRAM_ADDRESS        (0x03FE0000UL)
#define FLASHRAM_BUFFER_ADDRESS (0x05002900UL)


enum operation {
    OP_NONE,
    OP_ERASE_ALL,
    OP_ERASE_SECTOR,
    OP_WRITE_PAGE
};


static enum operation flashram_operation_type (uint32_t scr) {
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
}

void flashram_process (void) {
    uint32_t scr = fpga_reg_get(REG_FLASHRAM_SCR);
    enum operation op = flashram_operation_type(scr);
    uint8_t read_buffer[FLASHRAM_PAGE_SIZE];
    uint8_t write_buffer[FLASHRAM_PAGE_SIZE];
    uint32_t address = FLASHRAM_ADDRESS;
    uint32_t erase_size = (op == OP_ERASE_SECTOR) ? FLASHRAM_SECTOR_SIZE : FLASHRAM_SIZE;
    uint32_t page = (op != OP_ERASE_ALL) ? ((scr & FLASHRAM_SCR_PAGE_MASK) >> FLASHRAM_SCR_PAGE_BIT) : 0;
    address += page * FLASHRAM_PAGE_SIZE;

    switch (op) {
        case OP_ERASE_ALL:
        case OP_ERASE_SECTOR:
            for (int i = 0; i < FLASHRAM_PAGE_SIZE; i++) {
                write_buffer[i] = 0xFF;
            }
            for (int i = 0; i < erase_size; i += FLASHRAM_PAGE_SIZE) {
                fpga_mem_write(address + i, FLASHRAM_PAGE_SIZE, write_buffer);
            }
            fpga_reg_set(REG_FLASHRAM_SCR, FLASHRAM_SCR_DONE);
            break;

        case OP_WRITE_PAGE:
            fpga_mem_read(FLASHRAM_BUFFER_ADDRESS, FLASHRAM_PAGE_SIZE, read_buffer);
            fpga_mem_read(address, FLASHRAM_PAGE_SIZE, write_buffer);
            for (int i = 0; i < FLASHRAM_PAGE_SIZE; i++) {
                write_buffer[i] &= read_buffer[i];
            }
            fpga_mem_write(address, FLASHRAM_PAGE_SIZE, write_buffer);
            fpga_reg_set(REG_FLASHRAM_SCR, FLASHRAM_SCR_DONE);
            break;

        case OP_NONE:
        default:
            break;
    }
}

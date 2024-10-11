#include <stdint.h>
#include "fpga.h"


#define FLASHRAM_SIZE           (128 * 1024)
#define FLASHRAM_SECTOR_SIZE    (16 * 1024)
#define FLASHRAM_PAGE_SIZE      (128)
#define FLASHRAM_ADDRESS        (0x03FE0000UL)
#define FLASHRAM_BUFFER_ADDRESS (0x05002C00UL)


typedef enum {
    OP_NONE,
    OP_ERASE_ALL,
    OP_ERASE_SECTOR,
    OP_WRITE_PAGE
} flashram_op_t;


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
}


void flashram_process (void) {
    uint32_t scr = fpga_reg_get(REG_FLASHRAM_SCR);

    flashram_op_t op = flashram_operation_type(scr);

    if (op == OP_NONE) {
        return;
    }

    uint8_t write_buffer[FLASHRAM_PAGE_SIZE];

    uint32_t page = ((scr & FLASHRAM_SCR_PAGE_MASK) >> FLASHRAM_SCR_PAGE_BIT);

    if (op == OP_WRITE_PAGE) {
        uint8_t page_buffer[FLASHRAM_PAGE_SIZE];

        uint32_t address = (FLASHRAM_ADDRESS + (page * FLASHRAM_PAGE_SIZE));

        fpga_mem_read(FLASHRAM_BUFFER_ADDRESS, FLASHRAM_PAGE_SIZE, page_buffer);
        fpga_mem_read(address, FLASHRAM_PAGE_SIZE, write_buffer);

        for (int i = 0; i < FLASHRAM_PAGE_SIZE; i++) {
            write_buffer[i] &= page_buffer[i];
        }

        fpga_mem_write(address, FLASHRAM_PAGE_SIZE, write_buffer);
    } else if ((op == OP_ERASE_SECTOR) || (op == OP_ERASE_ALL)) {
        for (int i = 0; i < FLASHRAM_PAGE_SIZE; i++) {
            write_buffer[i] = 0xFF;
        }

        page &= ~((FLASHRAM_SECTOR_SIZE / FLASHRAM_PAGE_SIZE) - 1);

        uint32_t erase_size = (op == OP_ERASE_ALL) ? FLASHRAM_SIZE : FLASHRAM_SECTOR_SIZE;
        uint32_t address = (FLASHRAM_ADDRESS + (page * FLASHRAM_PAGE_SIZE));

        for (uint32_t offset = 0; offset < erase_size; offset += FLASHRAM_PAGE_SIZE) {
            fpga_mem_write(address + offset, FLASHRAM_PAGE_SIZE, write_buffer);
        }
    }

    fpga_reg_set(REG_FLASHRAM_SCR, FLASHRAM_SCR_DONE);
}

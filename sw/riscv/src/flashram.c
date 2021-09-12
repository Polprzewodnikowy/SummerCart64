#include "flashram.h"


#define FLASHRAM_SIZE           (128 * 1024)
#define FLASHRAM_SECTOR_SIZE    (16 * 1024)
#define FLASHRAM_PAGE_SIZE      (128)
#define FLASHRAM_ERASE_VALUE    (0xFFFFFFFF)

enum operation {
    OP_NONE,
    OP_ERASE_ALL,
    OP_ERASE_SECTOR,
    OP_WRITE_PAGE
};


static enum operation get_operation_type (void) {
    uint32_t scr = FLASHRAM->SCR;

    if (!(scr & FLASHRAM_OPERATION_PENDING)) {
        return OP_NONE;
    }

    if (scr & FLASHRAM_WRITE_OR_ERASE) {
        if (scr & FLASHRAM_SECTOR_OR_ALL) {
            return OP_ERASE_ALL;
        } else {
            return OP_ERASE_SECTOR;
        }
    } else {
        return OP_WRITE_PAGE;
    }
}

static size_t get_operation_length (enum operation op) {
    switch (op) {
        case OP_ERASE_ALL: return FLASHRAM_SIZE;
        case OP_ERASE_SECTOR: return FLASHRAM_SECTOR_SIZE;
        case OP_WRITE_PAGE: return FLASHRAM_PAGE_SIZE;
        default: return 0;
    }
}


void flashram_init (void) {
    FLASHRAM->SCR = FLASHRAM_OPERATION_DONE;
}


void process_flashram (void) {
    enum operation op = get_operation_type();
    size_t length;
    io32_t *save_data;

    if (op != OP_NONE) {
        length = get_operation_length(op);
        save_data = (io32_t *)(SDRAM_BASE + CFG->SAVE_OFFSET + ((FLASHRAM->SCR >> FLASHRAM_PAGE_BIT) * FLASHRAM_PAGE_SIZE));

        for (uint32_t i = 0; i < (length / 4); i++) {
            if (op == OP_WRITE_PAGE) {
                *save_data++ &= FLASHRAM->BUFFER[i];       
            } else {
                *save_data++ = FLASHRAM_ERASE_VALUE;
            }
        }

        FLASHRAM->SCR = FLASHRAM_OPERATION_DONE;
    }
}

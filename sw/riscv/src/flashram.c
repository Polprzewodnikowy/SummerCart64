#include "sys.h"


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


struct process {
    bool save_in_progress;
    enum operation op;
    io32_t *save_pointer;
    uint32_t num_words;
    uint32_t current_word;
};

static struct process p;


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

    p.save_in_progress = false;
}


void process_flashram (void) {
    if (!p.save_in_progress) {
        p.op = get_operation_type();

        if (p.op != OP_NONE) {
            uint32_t sdram_address = SDRAM_BASE + SAVE_OFFSET;

            p.save_in_progress = true;
            if (p.op != OP_ERASE_ALL) {
                sdram_address += (FLASHRAM->SCR >> FLASHRAM_PAGE_BIT) * FLASHRAM_PAGE_SIZE;
            }
            p.save_pointer = (io32_t *) (sdram_address);
            p.num_words = get_operation_length(p.op) / sizeof(uint32_t);
            p.current_word = 0;
        }
    } else {
        if (p.op == OP_WRITE_PAGE) {
            *p.save_pointer++ &= FLASHRAM->BUFFER[p.current_word];
        } else {
            *p.save_pointer++ = FLASHRAM_ERASE_VALUE;
        }

        p.current_word += 1;

        if (p.current_word >= p.num_words) {
            p.save_in_progress = false;
            FLASHRAM->SCR = FLASHRAM_OPERATION_DONE;
        }
    }
}

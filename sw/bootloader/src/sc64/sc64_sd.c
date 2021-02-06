#include "sc64.h"
#include "sc64_sd.h"


#define CMD8_ARG_SUPPLY_VOLTAGE_27_36_V (1 << 8)
#define CMD8_ARG_CHECK_PATTERN_AA       (0xAA << 0)

#define ACMD41_ARG_HCS                  (1 << 30)

#define R3_CCS                          (1 << 30)
#define R3_BUSY                         (1 << 31)

#define R7_SUPPLY_VOLTAGE_27_36_V       (1 << 8)
#define R7_CHECK_PATTERN_AA             (0xAA << 0)

#define SD_BLOCK_SIZE                   (512)


typedef enum sc64_sd_clock_e {
    CLOCK_STOP,
    CLOCK_400_KHZ,
    CLOCK_25_MHZ,
    CLOCK_50_MHZ,
} sc64_sd_clock_t;

typedef enum sc64_sd_dat_width_e {
    DAT_WIDTH_1BIT,
    DAT_WIDTH_4BIT,
} sc64_sd_dat_width_t;

typedef enum sc64_sd_cmd_flags_e {
    NO_FLAGS        = 0,
    ACMD            = (1 << 0),
    SKIP_RESPONSE   = (1 << 1),
    LONG_RESPONSE   = (1 << 2),
    IGNORE_CRC      = (1 << 3),
    IGNORE_INDEX    = (1 << 4),
} sc64_sd_cmd_flags_t;

typedef enum sc64_sd_dat_direction_e {
    DAT_DIR_RX,
    DAT_DIR_TX,
} sc64_sd_dat_direction_t;


static bool sd_card_initialized = false;
static bool sd_card_type_block = false;
static bool sd_card_selected = false;
static uint32_t rca = 0;


static void sc64_sd_set_clock(sc64_sd_clock_t clock) {
    uint32_t scr = platform_pi_io_read(&SC64_SD->SCR);

    scr &= ~SC64_SD_SCR_CLK_MASK;

    switch (clock) {
        case CLOCK_400_KHZ:
            scr |= SC64_SD_SCR_CLK_400_KHZ;
            break;
        case CLOCK_25_MHZ:
            scr |= SC64_SD_SCR_CLK_25_MHZ;
            break;
        case CLOCK_50_MHZ:
            scr |= SC64_SD_SCR_CLK_50_MHZ;
            break;
        default:
            break;
    }

    platform_pi_io_write(&SC64_SD->SCR, scr);
}

static void sc64_sd_set_dat_width(sc64_sd_dat_width_t dat_width) {
    uint32_t scr = platform_pi_io_read(&SC64_SD->SCR);
    
    scr &= ~SC64_SD_SCR_DAT_WIDTH;

    if (dat_width == DAT_WIDTH_4BIT) {
        scr |= SC64_SD_SCR_DAT_WIDTH;
    }

    platform_pi_io_write(&SC64_SD->SCR, scr);
}

static void sc64_sd_hw_init(void) {
    sc64_enable_sd();

    platform_pi_io_write(&SC64_SD->DMA_SCR, SC64_SD_DMA_SCR_STOP);
    platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_TX_FIFO_FLUSH | SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);
    platform_pi_io_write(&SC64_SD->SCR, 0);

    sc64_sd_set_clock(CLOCK_400_KHZ);
    sc64_sd_set_dat_width(DAT_WIDTH_1BIT);
}

static void sc64_sd_hw_deinit(void) {
    if (sc64_get_scr() & SC64_CART_SCR_SD_ENABLE) {
        while (platform_pi_io_read(&SC64_SD->CMD) & SC64_SD_CMD_BUSY);
        platform_pi_io_write(&SC64_SD->DMA_SCR, SC64_SD_DMA_SCR_STOP);
        platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_TX_FIFO_FLUSH | SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);
        platform_pi_io_write(&SC64_SD->SCR, 0);
    }

    sc64_disable_sd();
}

static sc64_sd_err_t sc64_sd_cmd_send(uint8_t index, uint32_t arg, sc64_sd_cmd_flags_t flags, uint32_t *response) {
    sc64_sd_err_t error;
    uint32_t reg;

    if (flags & ACMD) {
        error = sc64_sd_cmd_send(55, rca, NO_FLAGS, response);

        if (error != E_OK) {
            return error;
        }
    }

    platform_pi_io_write(&SC64_SD->ARG, arg);

    reg = SC64_SD_CMD_START | SC64_SD_CMD_INDEX(index);

    if (flags & SKIP_RESPONSE) {
        reg |= SC64_SD_CMD_SKIP_RESPONSE;
    }

    if (flags & LONG_RESPONSE) {
        reg |= SC64_SD_CMD_LONG_RESPONSE;
    }

    platform_pi_io_write(&SC64_SD->CMD, reg);

    do {
        reg = platform_pi_io_read(&SC64_SD->CMD);
    } while (reg & SC64_SD_CMD_BUSY);

    *response = platform_pi_io_read(&SC64_SD->RSP);

    if (reg & SC64_SD_CMD_TIMEOUT) {
        return E_TIMEOUT;
    }

    if ((!(flags & IGNORE_CRC)) && (!(flags & SKIP_RESPONSE)) && (reg & SC64_SD_CMD_RESPONSE_CRC_ERROR)) {
        return E_CRC_ERROR;
    }

    if ((!(flags & SKIP_RESPONSE)) && (!(flags & IGNORE_INDEX)) && (SC64_SD_CMD_INDEX_GET(reg) != index)) {
        return E_BAD_INDEX;
    }

    return E_OK;
}

static void sc64_sd_dat_prepare(size_t num_blocks, size_t block_size, sc64_sd_dat_direction_t direction) {
    platform_pi_io_write(&SC64_SD->DAT, (
        SC64_SD_DAT_NUM_BLOCKS(num_blocks) |
        SC64_SD_DAT_BLOCK_SIZE(block_size) |
        (direction ? SC64_SD_DAT_DIRECTION : 0) |
        SC64_SD_DAT_START
    ));
}

static void sc64_sd_dat_abort(void) {
    platform_pi_io_write(&SC64_SD->DAT, (
        SC64_SD_DAT_TX_FIFO_FLUSH |
        SC64_SD_DAT_RX_FIFO_FLUSH |
        SC64_SD_DAT_STOP
    ));
}

static sc64_sd_err_t sc64_sd_dat_read(size_t block_size, void *buffer) {
    int timeout;
    uint32_t reg;

    timeout = 100000;
    do {
        reg = platform_pi_io_read(&SC64_SD->DAT);
        if (SC64_SD_DAT_RX_FIFO_ITEMS_GET(reg) >= block_size) {
            break;
        }
    } while ((reg & SC64_SD_DAT_BUSY) && (timeout--));

    if (reg & SC64_SD_DAT_CRC_ERROR) {
        return E_CRC_ERROR;
    }

    if (reg & SC64_SD_DAT_RX_FIFO_OVERRUN) {
        platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH);

        return E_FIFO_ERROR;
    }

    if (timeout == 0) {
        sc64_sd_dat_abort();

        return E_TIMEOUT;
    }

    platform_pi_dma_read(buffer, &SC64_SD->FIFO, block_size);
    platform_cache_invalidate(buffer, block_size);

    return E_OK;
}


bool sc64_sd_init(void) {
    sc64_sd_err_t error;
    uint32_t response;
    uint32_t argument;
    bool sd_version_2_or_later;
    uint8_t buffer[64] __attribute__((aligned(16)));

    if (sd_card_initialized) {
        return true;
    }

    sc64_sd_hw_init();

    do {
        error = sc64_sd_cmd_send(0, 0, SKIP_RESPONSE, &response);

        argument = CMD8_ARG_SUPPLY_VOLTAGE_27_36_V | CMD8_ARG_CHECK_PATTERN_AA;
        error = sc64_sd_cmd_send(8, argument, NO_FLAGS, &response);
        sd_version_2_or_later = (error == E_OK);
        if (sd_version_2_or_later && (response != (R7_SUPPLY_VOLTAGE_27_36_V | R7_CHECK_PATTERN_AA))) {
            break;
        }

        argument = (sd_version_2_or_later ? ACMD41_ARG_HCS : 0) | 0x00FF8000;
        for (int i = 0; i < 4000; i++) {
            error = sc64_sd_cmd_send(41, argument, ACMD | IGNORE_CRC | IGNORE_INDEX, &response);
            if ((error != E_OK) || (response & R3_BUSY)) {
                break;
            }
        }
        if ((error != E_OK) || ((response & 0x00FF8000) == 0)) {
            break;
        }
        sd_card_type_block = (response & R3_CCS) ? true : false;

        error = sc64_sd_cmd_send(2, 0, LONG_RESPONSE | IGNORE_INDEX, &response);
        if (error != E_OK) {
            break;
        }

        error = sc64_sd_cmd_send(3, 0, NO_FLAGS, &response);
        if (error != E_OK) {
            break;
        }
        rca = response & 0xFFFF0000;

        error = sc64_sd_cmd_send(7, rca, NO_FLAGS, &response);
        if (error != E_OK) {
            break;
        }
        sd_card_selected = true;

        error = sc64_sd_cmd_send(6, 2, ACMD, &response);
        if (error != E_OK) {
            break;
        }

        sc64_sd_set_clock(CLOCK_25_MHZ);
        sc64_sd_set_dat_width(DAT_WIDTH_4BIT);

        sc64_sd_dat_prepare(1, 64, DAT_DIR_RX);
        error = sc64_sd_cmd_send(6, 0x00000001, NO_FLAGS, &response);
        if (error != E_OK) {
            sc64_sd_dat_abort();
            break;
        }
        error = sc64_sd_dat_read(64, buffer);
        if (error != E_OK) {
            break;
        }
        if (buffer[13] & 0x02) {
            sc64_sd_dat_prepare(1, 64, DAT_DIR_RX);
            error = sc64_sd_cmd_send(6, 0x80000001, NO_FLAGS, &response);
            if (error != E_OK) {
                sc64_sd_dat_abort();
                break;
            }
            error = sc64_sd_dat_read(64, buffer);
            if (error != E_OK) {
                break;
            }

            sc64_sd_set_clock(CLOCK_50_MHZ);
        }

        sd_card_initialized = true;
    
        return true;
    } while(0);

    sc64_sd_deinit();

    return false;
}

void sc64_sd_deinit(void) {
    uint32_t response;

    if (sd_card_selected) {
        sc64_sd_cmd_send(7, rca, NO_FLAGS, &response);

        sd_card_selected = false;
    }

    sc64_sd_cmd_send(0, 0, SKIP_RESPONSE, &response);

    sc64_sd_hw_deinit();
}

bool sc64_sd_get_status(void) {
    return sd_card_initialized;
}

sc64_sd_err_t sc64_sd_read_sectors(uint32_t starting_sector, size_t count, void *buffer) {
    sc64_sd_err_t error;
    uint32_t response;
    uint32_t current_sector;
    uint32_t reg;
    int timeout;

    if (!sd_card_initialized) {
        return E_NO_INIT;
    }

    if ((count == 0) || (buffer == NULL)) {
        return E_PAR_ERROR;
    }

    current_sector = starting_sector;
    if (!sd_card_type_block) {
        current_sector *= SD_BLOCK_SIZE;
    }

    for (size_t i = 0; i < count; i++) {
        platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_NUM_BLOCKS(1) | SC64_SD_DAT_BLOCK_SIZE(SD_BLOCK_SIZE) | SC64_SD_DAT_START);
        
        error = sc64_sd_cmd_send(17, current_sector, NO_FLAGS, &response);
        if (error != E_OK) {
            platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);

            return error;
        }

        timeout = 100000;
        do {
            reg = platform_pi_io_read(&SC64_SD->DAT);
        } while ((reg & SC64_SD_DAT_BUSY) && (timeout--));

        if (reg & SC64_SD_DAT_CRC_ERROR) {
            return E_CRC_ERROR;
        }

        if (reg & SC64_SD_DAT_RX_FIFO_OVERRUN) {
            platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH);

            return E_FIFO_ERROR;
        }

        if (timeout == 0) {
            platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);

            return E_TIMEOUT;
        }

        platform_pi_dma_read(buffer, &SC64_SD->FIFO, SD_BLOCK_SIZE);
        platform_cache_invalidate(buffer, SD_BLOCK_SIZE);

        buffer += SD_BLOCK_SIZE;
        current_sector += sd_card_type_block ? 1 : SD_BLOCK_SIZE;
    }

    return E_OK;
}

sc64_sd_err_t sc64_sd_read_sectors_dma(uint32_t starting_sector, size_t count, uint8_t bank, uint32_t address) {
    size_t num_transfers;
    size_t sectors_left;
    uint32_t current_sector;
    uint32_t current_address;
    uint32_t num_blocks;
    uint32_t reg;
    sc64_sd_err_t error;
    uint32_t response;
    int timeout;

    if (!sd_card_initialized) {
        return E_NO_INIT;
    }

    if (count == 0) {
        return E_PAR_ERROR;
    }

    num_transfers = (count / 2048) + 1;
    sectors_left = count;
    current_sector = starting_sector;
    if (!sd_card_type_block) {
        current_sector *= SD_BLOCK_SIZE;
    }
    current_address = address;

    for (size_t i = 0; i < num_transfers; i++) {
        num_blocks = (sectors_left > 2048) ? 2048 : sectors_left;

        platform_pi_io_write(&SC64_SD->DMA_ADDR, SC64_SD_DMA_BANK_ADDR(bank, current_address));
        platform_pi_io_write(&SC64_SD->DMA_LEN, SC64_SD_DMA_LEN(num_blocks * SD_BLOCK_SIZE));
        platform_pi_io_write(&SC64_SD->DMA_SCR, SC64_SD_DMA_SCR_DIRECTION | SC64_SD_DMA_SCR_START);
        platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_NUM_BLOCKS(num_blocks) | SC64_SD_DAT_BLOCK_SIZE(SD_BLOCK_SIZE) | SC64_SD_DAT_START);

        error = sc64_sd_cmd_send(18, current_sector, NO_FLAGS, &response);
        if (error != E_OK) {
            platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);

            return error;
        }

        timeout = 1000000;
        do {
            reg = platform_pi_io_read(&SC64_SD->DAT);
        } while ((reg & SC64_SD_DAT_BUSY) && (timeout--));

        error = sc64_sd_cmd_send(12, 0, NO_FLAGS, &response);
        if (error != E_OK) {
            return error;
        }

        if (reg & SC64_SD_DAT_CRC_ERROR) {
            return E_CRC_ERROR;
        }

        if (reg & SC64_SD_DAT_RX_FIFO_OVERRUN) {
            platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH);
            platform_pi_io_write(&SC64_SD->DMA_SCR, SC64_SD_DMA_SCR_STOP);

            return E_FIFO_ERROR;
        }

        if (timeout == 0) {
            platform_pi_io_write(&SC64_SD->DAT, SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);
            platform_pi_io_write(&SC64_SD->DMA_SCR, SC64_SD_DMA_SCR_STOP);

            return E_TIMEOUT;
        }

        while (platform_pi_io_read(&SC64_SD->DMA_SCR) & SC64_SD_DMA_SCR_BUSY);

        current_sector += sd_card_type_block ? 1 : SD_BLOCK_SIZE;
        current_address += num_blocks * SD_BLOCK_SIZE;
    }

    return E_OK;
}

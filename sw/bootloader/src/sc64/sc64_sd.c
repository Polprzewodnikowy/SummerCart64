#include <string.h>

#include "sc64.h"
#include "sc64_sd.h"


static const size_t SC64_SD_BUFFER_SIZE = sizeof(SC64_SD->buffer);

static uint8_t sd_spi_dma_buffer[sizeof(SC64_SD->buffer)] __attribute__((aligned(16)));
static uint8_t sd_card_initialized = FALSE;
static uint8_t sd_card_type_block = FALSE;


static void sc64_sd_spi_set_prescaler(uint8_t prescaler) {
    uint32_t scr = platform_pi_io_read(&SC64_SD->scr);

    scr = (scr & ~SC64_SD_SCR_PRESCALER_MASK) | (prescaler & SC64_SD_SCR_PRESCALER_MASK);

    platform_pi_io_write(&SC64_SD->scr, scr);
}

static void sc64_sd_spi_set_chip_select(uint8_t select) {
    platform_pi_io_write(&SC64_SD->cs, (select == TRUE) ? 0 : SC64_SD_CS_PIN);
}

static uint8_t sc64_sd_spi_exchange(uint8_t data) {
    uint32_t dr;

    platform_pi_io_write(&SC64_SD->dr, (uint32_t) (data & SC64_SD_DR_DATA_MASK));

    do {
        dr = platform_pi_io_read(&SC64_SD->dr);
    } while (dr & SC64_SD_DR_BUSY);

    return (uint8_t) SC64_SD_DR_DATA(dr);
}

static uint8_t sc64_sd_spi_read(void) {
    return sc64_sd_spi_exchange(0xFF);
}

static void sc64_sd_spi_release(void) {
    sc64_sd_spi_set_chip_select(FALSE);
    sc64_sd_spi_read();
}

static void sc64_sd_spi_multi_write(uint8_t *data, size_t length) {
    size_t remaining = length;
    size_t offset = 0;

    do {
        size_t block_size = remaining < SC64_SD_BUFFER_SIZE ? remaining : SC64_SD_BUFFER_SIZE;
        uint32_t dma_transfer_length = block_size + ((block_size % 4) ? (4 - (block_size % 4)) : 0);

        memcpy(sd_spi_dma_buffer, data + offset, block_size);

        platform_cache_writeback(sd_spi_dma_buffer, dma_transfer_length);
        platform_pi_dma_write(sd_spi_dma_buffer, &SC64_SD->buffer, dma_transfer_length);

        platform_pi_io_write(&SC64_SD->multi, SC64_SD_MULTI(FALSE, FALSE, block_size));

        while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);

        remaining -= block_size;
        offset += block_size;
    } while (remaining > 0);
}

static void sc64_sd_spi_multi_read(uint8_t *data, size_t length, uint8_t is_buffer_aligned) {
    size_t remaining = length;
    size_t offset = 0;

    do {
        size_t block_size = remaining < SC64_SD_BUFFER_SIZE ? remaining : SC64_SD_BUFFER_SIZE;
        uint32_t dma_transfer_length = block_size + ((block_size % 4) ? (4 - (block_size % 4)) : 0);
        uint8_t *buffer_pointer = is_buffer_aligned ? (data + offset) : sd_spi_dma_buffer;
        
        platform_pi_io_write(&SC64_SD->multi, SC64_SD_MULTI(FALSE, TRUE, block_size));

        while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);
        
        platform_pi_dma_read(buffer_pointer, &SC64_SD->buffer, dma_transfer_length);
        platform_cache_invalidate(buffer_pointer, dma_transfer_length);

        if (!is_buffer_aligned) {
            memcpy(data + offset, sd_spi_dma_buffer, block_size);
        }

        remaining -= block_size;
        offset += block_size;
    } while (remaining > 0);
}

static void sc64_sd_spi_multi_read_dma(size_t length) {
    size_t remaining = length;

    do {
        size_t block_size = remaining < SC64_SD_BUFFER_SIZE ? remaining : SC64_SD_BUFFER_SIZE;

        platform_pi_io_write(&SC64_SD->multi, SC64_SD_MULTI(TRUE, TRUE, block_size));

        while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);

        remaining -= block_size;
    } while (remaining > 0);
}

static uint8_t sc64_sd_wait_ready(void) {
    uint8_t response;

    for (size_t attempts = 10000; attempts > 0; --attempts) {
        response = sc64_sd_spi_read();

        if (response == SD_NOT_BUSY_RESPONSE) {
            break;
        }
    }

    return response;
}

uint8_t sc64_sd_init(void) {
    uint32_t arg;
    uint8_t success;
    uint8_t response[5];
    uint8_t *short_response = &response[0];
    uint32_t *extended_response = (uint32_t *) &response[1];
    uint8_t sd_version_2_or_later;
    uint8_t status_data[64] __attribute__((aligned(16)));

    if (!sd_card_initialized) {
        sc64_enable_sd();

        platform_pi_io_write(&SC64_SD->dma_scr, SC64_SD_DMA_SCR_FLUSH);

        sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_256);
        sc64_sd_spi_release();

        sc64_sd_spi_set_chip_select(TRUE);

        for (uint8_t dummy_clocks = 10; dummy_clocks > 0; --dummy_clocks) {
            sc64_sd_spi_read();
        }

        do {
            success = sc64_sd_cmd_send(CMD0, 0, response);
            if (!(success && *short_response == R1_IN_IDLE_STATE)) break;

            arg = CMD8_ARG_SUPPLY_VOLTAGE_27_36_V | CMD8_ARG_CHECK_PATTERN_AA;
            success = sc64_sd_cmd_send(CMD8, CMD8_ARG_SUPPLY_VOLTAGE_27_36_V | CMD8_ARG_CHECK_PATTERN_AA, response);
            if (!success) {
                break;
            } else if (*short_response & R1_ILLEGAL_COMMAND) {
                sd_version_2_or_later = FALSE;
            } else if (
                ((*extended_response & R7_VOLTAGE_ACCEPTED_MASK) == R7_SUPPLY_VOLTAGE_27_36_V) &&
                ((*extended_response & R7_CHECK_PATTERN_MASK) == R7_CHECK_PATTERN_AA)
            ) {
                sd_version_2_or_later = TRUE;
            } else {
                break;
            }

            success = sc64_sd_cmd_send(CMD58, 0, response);
            if (!success || (!(*extended_response & R3_27_36_V_MASK))) break;

            arg = sd_version_2_or_later ? ACMD41_ARG_HCS : 0;
            for (size_t attempts = 0; attempts < 10000; attempts++) {
                success = sc64_sd_cmd_send(ACMD41, arg, response);
                if (!success || (!(*short_response & R1_IN_IDLE_STATE))) {
                    break;
                }
            }
            if (!success) break;

            success = sc64_sd_cmd_send(CMD58, 0, response);
            if (!success) break;
            sd_card_type_block = (*extended_response & R3_CCS) ? TRUE : FALSE;

            sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_4);

            // Set high speed mode
            success = sc64_sd_cmd_send(CMD6, 0x00000001, response);
            if (!success) break;
            success = sc64_sd_block_read(status_data, 64, FALSE);
            if (!success) break;
            if (status_data[13] & 0x02) {
                success = sc64_sd_cmd_send(CMD6, 0x80000001, response);
                if (!success) break;
                success = sc64_sd_block_read(status_data, 64, FALSE);
                if (!success) break;
                sc64_sd_spi_read();
                sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_2);
            }

            sd_card_initialized = TRUE;

            return TRUE;
        } while (0);

        sc64_sd_deinit();

        return FALSE;
    }

    return TRUE;
}

void sc64_sd_deinit(void) {
    sd_card_initialized = FALSE;

    sc64_sd_spi_release();

    while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);

    platform_pi_io_write(&SC64_SD->dma_scr, SC64_SD_DMA_SCR_FLUSH);
    
    sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_256);

    sc64_disable_sd();
}

uint8_t sc64_sd_get_status(void) {
    return sd_card_initialized ? TRUE : FALSE;
}

void sc64_sd_dma_wait_for_finish(void) {
    while (!(platform_pi_io_read(&SC64_SD->dma_scr) & SC64_SD_DMA_SCR_FIFO_EMPTY));
}

void sc64_sd_dma_set_address(uint8_t bank, uint32_t address) {
    platform_pi_io_write(&SC64_SD->dma_addr, SC64_SD_DMA_ADDR(bank, address));
}

uint8_t sc64_sd_cmd_send(uint8_t cmd, uint32_t arg, uint8_t *response) {
    uint8_t cmd_data[7];
    uint8_t cmd_data_length;
    uint8_t is_long_response;

    if (cmd & ACMD_MARK) {
        sc64_sd_cmd_send(CMD55, 0, response);

        if (*response & ~(R1_IN_IDLE_STATE)) {
            return FALSE;
        }
    }

    if (sc64_sd_wait_ready() != SD_NOT_BUSY_RESPONSE) {
        return FALSE;
    }

    if ((cmd == CMD17 || cmd == CMD18) && sd_card_type_block) {
        
    }

    cmd_data[0] = cmd & ~ACMD_MARK;         // Command index
    cmd_data[5] = 0x01;                     // CRC7 and stop bit
    cmd_data[6] = 0xFF;                     // CMD12 stuff byte

    cmd_data_length = 6;
    is_long_response = FALSE;

    switch (cmd) {
        case CMD0:
            cmd_data[5] = 0x95;
            break;
        case CMD8:
            cmd_data[5] = 0x87;
            is_long_response = TRUE;
            break;
        case CMD12:
            cmd_data_length = 7;
            break;
        case CMD17:
        case CMD18:
            if (!sd_card_type_block) {
                arg *= SD_BLOCK_SIZE;
            }
            break;
        case CMD58:
            is_long_response = TRUE;
            break;
    }

    cmd_data[1] = (uint8_t) (arg >> 24);    // Command argument
    cmd_data[2] = (uint8_t) (arg >> 16);
    cmd_data[3] = (uint8_t) (arg >> 8);
    cmd_data[4] = (uint8_t) (arg >> 0);

    sc64_sd_spi_multi_write(cmd_data, cmd_data_length);

    for (uint32_t response_timeout = 0; response_timeout < 8; response_timeout++) {
        *response = sc64_sd_spi_read();

        if (!(*response & RESPONSE_START_BIT)) {
            if (is_long_response) {
                sc64_sd_spi_multi_read(response + 1, 4, FALSE);
            }

            return TRUE;
        }
    }

    return FALSE;
}

uint8_t sc64_sd_block_read(uint8_t *buffer, size_t length, uint8_t dma) {
    uint8_t token;
    uint8_t crc[2];

    for (size_t attempts = 10000; attempts > 0; --attempts) {
        token = sc64_sd_spi_read();

        if (token != SD_NOT_BUSY_RESPONSE) {
            break;
        }
    }

    if (token != DATA_TOKEN_BLOCK_READ) {
        return FALSE;
    }

    if (dma) {
        sc64_sd_spi_multi_read_dma(length);
    } else {
        sc64_sd_spi_multi_read(buffer, length, TRUE);
    }
    sc64_sd_spi_multi_read(crc, 2, FALSE);

    return TRUE;
}

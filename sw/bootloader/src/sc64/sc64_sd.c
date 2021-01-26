#include <string.h>

#include "sc64.h"
#include "sc64_sd.h"


static const size_t SC64_SD_BUFFER_SIZE = sizeof(SC64_SD->buffer);

static uint8_t sd_spi_dma_buffer[sizeof(SC64_SD->buffer)] __attribute__((aligned(16)));
static uint8_t sd_card_initialized = FALSE;
static uint8_t sd_card_type = 0;


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

    return (uint8_t) (dr & SC64_SD_DR_DATA_MASK);
}

static void sc64_sd_spi_write(uint8_t data) {
    sc64_sd_spi_exchange(data);
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

        platform_pi_io_write(&SC64_SD->multi, ((block_size - 1) << SC64_SD_MULTI_LENGTH_BIT) & SC64_SD_MULTI_LENGTH_MASK);

        while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);

        remaining -= block_size;
        offset += block_size;
    } while (remaining > 0);
}

static void sc64_sd_spi_multi_read(uint8_t *data, size_t length) {
    size_t remaining = length;
    size_t offset = 0;

    do {
        size_t block_size = remaining < SC64_SD_BUFFER_SIZE ? remaining : SC64_SD_BUFFER_SIZE;
        uint32_t dma_transfer_length = block_size + ((block_size % 4) ? (4 - (block_size % 4)) : 0);
        
        platform_pi_io_write(&SC64_SD->multi, SC64_SD_MULTI_RX_ONLY | (((block_size - 1) << SC64_SD_MULTI_LENGTH_BIT) & SC64_SD_MULTI_LENGTH_MASK));

        while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);
        
        platform_pi_dma_read(sd_spi_dma_buffer, &SC64_SD->buffer, dma_transfer_length);
        platform_cache_invalidate(sd_spi_dma_buffer, dma_transfer_length);

        memcpy(data + offset, sd_spi_dma_buffer, block_size);

        remaining -= block_size;
        offset += block_size;
    } while (remaining > 0);
}

static uint8_t sc64_sd_wait_ready(void) {
    uint8_t response;

    for (uint16_t attempts = 10000; attempts > 0; --attempts) {
        response = sc64_sd_spi_read();

        if (response == 0xFF) {
            break;
        }
    }

    return response;
}

uint8_t sc64_sd_init(void) {
    uint8_t cmd;
    uint8_t response;
    uint8_t ocr[4];
    uint8_t cmd6_data[64];
    uint8_t ct;

    if (!sd_card_initialized) {
        sc64_enable_sd();

        sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_256);
        sc64_sd_spi_release();

        for (uint8_t dummy_clocks = 10; dummy_clocks > 0; --dummy_clocks) {
            sc64_sd_spi_read();
        }

        ct = 0;

        if (sc64_sd_cmd_write(CMD0, 0) == 0x01) {
            if (sc64_sd_cmd_write(CMD8, 0x1AA) == 1) {
                sc64_sd_spi_multi_read(ocr, 4);

                if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                    for (uint16_t attempts = 10000; attempts > 0; --attempts) {
                        response = sc64_sd_cmd_write(ACMD41, (1 << 30));

                        if (!response) {
                            break;
                        }
                    }

                    if (!response && sc64_sd_cmd_write(CMD58, 0) == 0) {
                        sc64_sd_spi_multi_read(ocr, 4);
                        ct = (ocr[0] & 0x40) ? CARD_TYPE_SD2 | CARD_TYPE_BLOCK : CARD_TYPE_SD2;
                    }
                }
            } else {
                if (sc64_sd_cmd_write(ACMD41, 0) <= 1) {
                    ct = CARD_TYPE_SD1;
                    cmd = ACMD41;
                } else {
                    ct = CARD_TYPE_MMC;
                    cmd = CMD1;
                }
                
                for (uint16_t attempts = 10000; attempts > 0; --attempts) {
                    response = sc64_sd_cmd_write(cmd, 0);

                    if (!response) {
                        break;
                    }
                }

                if (response || sc64_sd_cmd_write(CMD16, 512) != 0) {
                    ct = 0;
                }
            }
        }

        if (ct) {
            sd_card_initialized = TRUE;
            sd_card_type = ct;

            sc64_sd_spi_release();

            sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_4);

            if (sc64_sd_cmd_write(CMD6, 0x00000001) == 0) {
                if(sc64_sd_block_read(cmd6_data, 64)) {
                    if (cmd6_data[13] & 0x02) {
                        if (sc64_sd_cmd_write(CMD6, 0x80000001) == 0) {
                            sc64_sd_block_read(cmd6_data, 64);
                            sc64_sd_spi_release();
                            sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_2);
                        }
                    }
                }
            }

            sc64_sd_spi_release();
        } else {
            sc64_sd_deinit();

            return FALSE;
        }
    }

    return TRUE;
}

void sc64_sd_deinit(void) {
    sd_card_initialized = FALSE;
    sd_card_type = 0;

    sc64_sd_spi_release();

    sc64_sd_spi_set_prescaler(SC64_SD_SCR_PRESCALER_256);

    platform_pi_io_write(&SC64_SD->multi, SC64_SD_MULTI_RX_ONLY | (((512 - 1) << SC64_SD_MULTI_LENGTH_BIT) & SC64_SD_MULTI_LENGTH_MASK));

    while (platform_pi_io_read(&SC64_SD->scr) & SC64_SD_SCR_BUSY);

    sc64_disable_sd();
}

uint8_t sc64_sd_get_status(void) {
    return sd_card_initialized ? TRUE : FALSE;
}

uint8_t sc64_sd_get_card_type(void) {
    return sd_card_type;
}

uint8_t sc64_sd_cmd_write(uint8_t cmd, uint32_t arg) {
    uint8_t response;
    uint8_t cmd_data[7];
    uint8_t cmd_data_length;

    if (cmd & 0x80) {
        cmd &= 0x7F;
        
        response = sc64_sd_cmd_write(CMD55, 0);
        
        if (response > 1) {
            return response;
        }
    }

    sc64_sd_spi_set_chip_select(FALSE);
    sc64_sd_spi_set_chip_select(TRUE);

    response = sc64_sd_wait_ready();

    if (response != 0xFF) {
        return response;
    }

    cmd_data[0] = cmd;
    cmd_data[1] = (uint8_t) (arg >> 24);
    cmd_data[2] = (uint8_t) (arg >> 16);
    cmd_data[3] = (uint8_t) (arg >> 8);
    cmd_data[4] = (uint8_t) (arg >> 0);
    cmd_data[5] = 0x01;
    cmd_data[6] = 0xFF;

    cmd_data_length = 6;

    if (cmd == CMD0) {
        cmd_data[5] = 0x95;
    }

    if (cmd == CMD8) {
        cmd_data[5] = 0x87;
    }

    if (cmd == CMD12) {
        cmd_data[5] = 0xFF;
        cmd_data_length = 7;
    }

    sc64_sd_spi_multi_write(cmd_data, cmd_data_length);

    for (uint32_t attempts = 100000; attempts > 0; --attempts) {
        response = sc64_sd_spi_read();

        if (!(response & 0x80)) {
            break;
        }
    }

    return response;
}

uint8_t sc64_sd_block_write(uint8_t *buffer, size_t length, uint8_t token) {
    uint8_t crc[2] = { 0xFF, 0xFF };

    if (sc64_sd_wait_ready() != 0xFF) {
        return FALSE;
    }

    sc64_sd_spi_write(token);

    if (token != 0xFD) {
        sc64_sd_spi_multi_write(buffer, length);
        sc64_sd_spi_multi_write(crc, 2);

        if((sc64_sd_spi_read() & 0x1F) != 0x05) {
            return FALSE;
        }
    }

    return TRUE;
}

uint8_t sc64_sd_block_read(uint8_t *buffer, size_t length) {
    uint8_t token;
    uint8_t dummy[2];

    for (uint16_t attempts = 10000; attempts > 0; --attempts) {
        token = sc64_sd_spi_read();

        if (token != 0xFF) {
            break;
        }
    }

    if (token != 0xFE) {
        return FALSE;
    }

    sc64_sd_spi_multi_read(buffer, length);
    sc64_sd_spi_multi_read(dummy, 2);

    return TRUE;    
}

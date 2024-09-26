#include "fpga.h"
#include "hw.h"
#include "sd.h"
#include "timer.h"


#define SD_INIT_BUFFER_ADDRESS          (0x05002800UL)
#define BYTE_SWAP_ADDRESS_END           (0x05000000UL)

#define CMD6_ARG_CHECK_HS               (0x00FFFFF1UL)
#define CMD6_ARG_SWITCH_HS              (0x80FFFFF1UL)
#define CMD6_DATA_LENGTH                (64)
#define CMD6_INVALID_CURRENT_LIMIT(b)   ((((b)[0] << 8) | (b)[1]) == 0)
#define CMD6_HS_SUPPORTED(b)            ((b)[13] & (1 << 1))
#define CMD6_HS_ENABLED(b)              (((b)[16] & 0x0F) == 0x01)

#define CMD8_ARG_SUPPLY_VOLTAGE_27_36_V (1 << 8)
#define CMD8_ARG_CHECK_PATTERN          (0xAA << 0)

#define ACMD6_ARG_BUS_WIDTH_4BIT        (2 << 0)

#define ACMD41_ARG_OCR                  (0x300000 << 0)
#define ACMD41_ARG_HCS                  (1 << 30)

#define R1_APP_CMD                      (1 << 5)
#define R1_ILLEGAL_COMMAND              (1 << 22)

#define R3_OCR                          (0x300000 << 0)
#define R3_CCS                          (1 << 30)
#define R3_BUSY                         (1 << 31)

#define R6_RCA_MASK                     (0xFFFF0000UL)

#define R7_SUPPLY_VOLTAGE_27_36_V       (1 << 8)
#define R7_CHECK_PATTERN                (0xAA << 0)

#define TIMEOUT_INIT_MS                 (1000)

#define DAT_CRC16_LENGTH                (8)
#define DAT_BLOCK_MAX_COUNT             (256)
#define DAT_TIMEOUT_INIT_MS             (2000)
#define DAT_TIMEOUT_DATA_MS             (5000)


typedef enum {
    CLOCK_STOP,
    CLOCK_400KHZ,
    CLOCK_25MHZ,
    CLOCK_50MHZ,
} sd_clock_t;

typedef enum {
    RSP_NONE,
    RSP_R1,
    RSP_R1b,
    RSP_R2,
    RSP_R3,
    RSP_R6,
    RSP_R7,
} rsp_type_t;

typedef enum {
    DAT_READ,
    DAT_WRITE,
} dat_mode_t;

typedef enum {
    DAT_OK,
    DAT_ERROR_IO,
    DAT_ERROR_TIMEOUT,
} dat_error_t;

typedef enum {
    CMD6_OK,
    CMD6_ERROR_ILLEGAL_CMD,
    CMD6_ERROR_IO,
    CMD6_ERROR_CRC,
    CMD6_ERROR_TIMEOUT,
} cmd6_error_t;


struct process {
    bool card_initialized;
    bool card_type_block;
    uint32_t rca;
    uint8_t csd[16];
    uint8_t cid[16];
    bool byte_swap;
    sd_lock_t lock;
};


static struct process p;


static void sd_set_clock (sd_clock_t mode) {
    fpga_reg_set(REG_SD_SCR, SD_SCR_CLOCK_MODE_OFF);

    switch (mode) {
        case CLOCK_400KHZ:
            fpga_reg_set(REG_SD_SCR, SD_SCR_CLOCK_MODE_400KHZ);
            break;
        case CLOCK_25MHZ:
            fpga_reg_set(REG_SD_SCR, SD_SCR_CLOCK_MODE_25MHZ);
            break;
        case CLOCK_50MHZ:
            fpga_reg_set(REG_SD_SCR, SD_SCR_CLOCK_MODE_50MHZ);
            break;
        default:
            break;
    }
}

static bool sd_cmd (uint8_t cmd, uint32_t arg, rsp_type_t rsp_type, void *rsp) {
    uint32_t scr;
    uint32_t cmd_data;

    cmd_data = ((cmd << SD_CMD_INDEX_BIT) & SD_CMD_INDEX_MASK);
    switch (rsp_type) {
        case RSP_NONE:
            cmd_data |= SD_CMD_SKIP_RESPONSE;
            break;
        case RSP_R2:
            cmd_data |= (SD_CMD_LONG_RESPONSE | SD_CMD_RESERVED_RESPONSE);
            break;
        case RSP_R3:
            cmd_data |= (SD_CMD_IGNORE_CRC | SD_CMD_RESERVED_RESPONSE);
            break;
        default:
            break;
    }

    fpga_reg_set(REG_SD_ARG, arg);
    fpga_reg_set(REG_SD_CMD, cmd_data);

    do {
        scr = fpga_reg_get(REG_SD_SCR);
    } while (scr & SD_SCR_CMD_BUSY);

    if (rsp != NULL) {
        if (cmd_data & SD_CMD_LONG_RESPONSE) {
            uint8_t *rsp_8 = (uint8_t *) (rsp);
            for (int i = 0; i < 4; i++) {
                uint32_t rsp_data = fpga_reg_get(REG_SD_RSP_3 - i);
                uint8_t *rsp_data_8 = (uint8_t *) (&rsp_data);
                rsp_data = SWAP32(rsp_data);
                for (int i = 0; i < 4; i++) {
                    *rsp_8++ = *rsp_data_8++;
                }
            }
        } else {
            (*(uint32_t *) (rsp)) = fpga_reg_get(REG_SD_RSP_0);
        }
    }

    if (rsp_type == RSP_R1b) {
        do {
            scr = fpga_reg_get(REG_SD_SCR);
        } while (scr & SD_SCR_CARD_BUSY);
    }

    return (scr & SD_SCR_CMD_ERROR);
}

static bool sd_acmd (uint8_t acmd, uint32_t arg, rsp_type_t rsp_type, void *rsp) {
    uint32_t acmd_rsp;
    if (sd_cmd(55, p.rca, RSP_R1, &acmd_rsp)) {
        return true;
    }
    if (!(acmd_rsp & R1_APP_CMD)) {
        return true;
    }
    if (sd_cmd(acmd, arg, rsp_type, rsp)) {
        return true;
    }
    return false;
}

static void sd_dat_prepare (uint32_t address, uint32_t count, dat_mode_t mode) {
    uint32_t length = (count * SD_SECTOR_SIZE);
    uint32_t sd_dat = (((count - 1) << SD_DAT_BLOCKS_BIT) | SD_DAT_FIFO_FLUSH);
    uint32_t sd_dma_scr = DMA_SCR_START;

    if (mode == DAT_READ) {
        sd_dat |= SD_DAT_START_READ;
        sd_dma_scr |= DMA_SCR_DIRECTION;
        if (p.byte_swap && (address < BYTE_SWAP_ADDRESS_END)) {
            sd_dma_scr |= DMA_SCR_BYTE_SWAP;
        }
    } else {
        sd_dat |= SD_DAT_START_WRITE;
    }

    fpga_reg_set(REG_SD_DAT, sd_dat);
    fpga_reg_set(REG_SD_DMA_ADDRESS, address);
    fpga_reg_set(REG_SD_DMA_LENGTH, length);
    fpga_reg_set(REG_SD_DMA_SCR, sd_dma_scr);
}

static void sd_dat_abort (void) {
    fpga_reg_set(REG_SD_DMA_SCR, DMA_SCR_STOP);
    fpga_reg_set(REG_SD_DAT, SD_DAT_STOP | SD_DAT_FIFO_FLUSH);
}

static dat_error_t sd_dat_wait (uint16_t timeout_ms) {
    timer_countdown_start(TIMER_ID_SD, timeout_ms);

    do {
        uint32_t sd_dat = fpga_reg_get(REG_SD_DAT);
        uint32_t sd_dma_scr = fpga_reg_get(REG_SD_DMA_SCR);
        if ((!(sd_dat & SD_DAT_BUSY)) && (!(sd_dma_scr & DMA_SCR_BUSY))) {
            if (sd_dat & SD_DAT_ERROR) {
                sd_dat_abort();
                return DAT_ERROR_IO;
            }
            return DAT_OK;
        }
    } while (!timer_countdown_elapsed(TIMER_ID_SD));

    sd_dat_abort();

    return DAT_ERROR_TIMEOUT;
}

static bool sd_dat_check_crc16 (uint8_t *data, uint32_t length) {
    uint16_t device_crc[4];
    uint16_t controller_crc[4];

    for (int crc = 0; crc < 4; crc++) {
        device_crc[crc] = 0;
        controller_crc[crc] = 0;
    }

    for (uint32_t index = length; index < (length + DAT_CRC16_LENGTH); index++) {
        uint8_t byte = data[index];
        for (int nibble = 0; nibble < 2; nibble++) {
            for (int crc = 0; crc < 4; crc++) {
                device_crc[crc] <<= 1;
                device_crc[crc] |= (byte >> (7 - crc)) & (1 << 0);
            }
            byte <<= 4;
        }
    }

    for (uint32_t index = 0; index < length; index++) {
        uint8_t byte = data[index];
        for (int nibble = 0; nibble < 2; nibble++) {
            for (int crc = 0; crc < 4; crc++) {
                uint8_t inv = ((controller_crc[crc] >> 15) ^ (byte >> (7 - crc))) & (1 << 0);
                controller_crc[crc] ^= (inv << 11) | (inv << 4);
                controller_crc[crc] <<= 1;
                controller_crc[crc] |= inv;
            }
            byte <<= 4;
        }
    }

    for (int crc = 0; crc < 4; crc++) {
        if (controller_crc[crc] != device_crc[crc]) {
            return true;
        }
    }

    return false;
}

static cmd6_error_t sd_cmd6 (uint32_t arg, uint8_t *buffer) {
    uint32_t rsp;
    sd_dat_prepare(SD_INIT_BUFFER_ADDRESS, 1, DAT_READ);
    if (sd_cmd(6, arg, RSP_R1, NULL)) {
        sd_dat_abort();
        if ((!sd_cmd(13, p.rca, RSP_R1, &rsp)) && (rsp & R1_ILLEGAL_COMMAND)) {
            return CMD6_ERROR_ILLEGAL_CMD;
        }
        return CMD6_ERROR_IO;
    }
    if (sd_dat_wait(DAT_TIMEOUT_INIT_MS) == DAT_ERROR_TIMEOUT) {
        return CMD6_ERROR_TIMEOUT;
    }
    fpga_mem_read(SD_INIT_BUFFER_ADDRESS, CMD6_DATA_LENGTH + DAT_CRC16_LENGTH, buffer);
    if (sd_dat_check_crc16(buffer, CMD6_DATA_LENGTH)) {
        return CMD6_ERROR_CRC;
    }
    return CMD6_OK;
}


sd_error_t sd_card_init (void) {
    uint32_t arg;
    uint32_t rsp;
    uint8_t cmd6_buffer[CMD6_DATA_LENGTH + DAT_CRC16_LENGTH];

    p.byte_swap = false;

    if (p.card_initialized) {
        return SD_OK;
    }

    if (!sd_card_is_inserted()) {
        return SD_ERROR_NO_CARD_IN_SLOT;
    }

    p.card_initialized = true;
    p.rca = 0;

    sd_set_clock(CLOCK_400KHZ);

    sd_cmd(0, 0, RSP_NONE, NULL);

    if (sd_cmd(8, (CMD8_ARG_SUPPLY_VOLTAGE_27_36_V | CMD8_ARG_CHECK_PATTERN), RSP_R7, &rsp)) {
        arg = ACMD41_ARG_OCR;
    } else {
        if (rsp != (R7_SUPPLY_VOLTAGE_27_36_V | R7_CHECK_PATTERN)) {
            sd_card_deinit();
            return SD_ERROR_CMD8_IO;
        }
        arg = (ACMD41_ARG_HCS | ACMD41_ARG_OCR);
    }

    timer_countdown_start(TIMER_ID_SD, TIMEOUT_INIT_MS);
    do {
        if (timer_countdown_elapsed(TIMER_ID_SD)) {
            sd_card_deinit();
            return SD_ERROR_ACMD41_TIMEOUT;
        }
        if (sd_acmd(41, arg, RSP_R3, &rsp)) {
            sd_card_deinit();
            return SD_ERROR_ACMD41_IO;
        }
        if (rsp & R3_BUSY) {
            if ((rsp & R3_OCR) == 0) {
                sd_card_deinit();
                return SD_ERROR_ACMD41_OCR;
            }
            p.card_type_block = (rsp & R3_CCS);
            break;
        }
    } while (true);

    sd_set_clock(CLOCK_25MHZ);

    if (sd_cmd(2, 0, RSP_R2, NULL)) {
        sd_card_deinit();
        return SD_ERROR_CMD2_IO;
    }

    if (sd_cmd(3, 0, RSP_R6, &rsp)) {
        sd_card_deinit();
        return SD_ERROR_CMD3_IO;
    }
    p.rca = (rsp & R6_RCA_MASK);

    if (sd_cmd(7, p.rca, RSP_R1b, NULL)) {
        sd_card_deinit();
        return SD_ERROR_CMD7_IO;
    }

    if (sd_acmd(6, ACMD6_ARG_BUS_WIDTH_4BIT, RSP_R1, NULL)) {
        sd_card_deinit();
        return SD_ERROR_ACMD6_IO;
    }

    switch (sd_cmd6(CMD6_ARG_CHECK_HS, cmd6_buffer)) {
        case CMD6_OK: {
            if (CMD6_INVALID_CURRENT_LIMIT(cmd6_buffer)) {
                sd_card_deinit();
                return SD_ERROR_CMD6_CHECK_RESPONSE;
            }
            if (CMD6_HS_SUPPORTED(cmd6_buffer)) {
                switch (sd_cmd6(CMD6_ARG_SWITCH_HS, cmd6_buffer)) {
                    case CMD6_OK: {
                        if (CMD6_INVALID_CURRENT_LIMIT(cmd6_buffer)) {
                            sd_card_deinit();
                            return SD_ERROR_CMD6_SWITCH_RESPONSE;
                        }
                        if (CMD6_HS_ENABLED(cmd6_buffer)) {
                            sd_set_clock(CLOCK_50MHZ);
                        }
                        break;
                    }
                    case CMD6_ERROR_IO:
                        sd_card_deinit();
                        return SD_ERROR_CMD6_SWITCH_IO;
                    case CMD6_ERROR_CRC:
                        sd_card_deinit();
                        return SD_ERROR_CMD6_SWITCH_CRC;
                    case CMD6_ERROR_TIMEOUT:
                        sd_card_deinit();
                        return SD_ERROR_CMD6_SWITCH_TIMEOUT;
                    case CMD6_ERROR_ILLEGAL_CMD:
                        break;
                }
            }
            break;
        }
        case CMD6_ERROR_IO:
            sd_card_deinit();
            return SD_ERROR_CMD6_CHECK_IO;
        case CMD6_ERROR_CRC:
            sd_card_deinit();
            return SD_ERROR_CMD6_CHECK_CRC;
        case CMD6_ERROR_TIMEOUT:
            sd_card_deinit();
            return SD_ERROR_CMD6_CHECK_TIMEOUT;
        case CMD6_ERROR_ILLEGAL_CMD:
            break;
    }

    sd_cmd(7, 0, RSP_NONE, NULL);

    if (sd_cmd(9, p.rca, RSP_R2, p.csd)) {
        sd_card_deinit();
        return SD_ERROR_CMD9_IO;
    }

    if (sd_cmd(10, p.rca, RSP_R2, p.cid)) {
        sd_card_deinit();
        return SD_ERROR_CMD10_IO;
    }

    if (sd_cmd(7, p.rca, RSP_R1b, NULL)) {
        sd_card_deinit();
        return SD_ERROR_CMD7_IO;
    }

    return SD_OK;
}

void sd_card_deinit (void) {
    if (p.card_initialized) {
        p.card_initialized = false;
        p.card_type_block = false;
        p.byte_swap = false;
        sd_set_clock(CLOCK_400KHZ);
        sd_cmd(0, 0, RSP_NONE, NULL);
        sd_set_clock(CLOCK_STOP);
    }
}

bool sd_card_is_inserted (void) {
    return (fpga_reg_get(REG_SD_SCR) & SD_SCR_CARD_INSERTED);
}

uint32_t sd_card_get_status (void) {
    uint32_t scr = fpga_reg_get(REG_SD_SCR);
    uint32_t byte_swap = p.byte_swap ? 1 : 0;
    uint32_t clock_mode_50mhz = ((scr & SD_SCR_CLOCK_MODE_MASK) == SD_SCR_CLOCK_MODE_50MHZ) ? 1 : 0;
    uint32_t card_type_block = p.card_type_block ? 1 : 0;
    uint32_t card_initialized = p.card_initialized ? 1 : 0;
    uint32_t card_inserted = (scr & SD_SCR_CARD_INSERTED) ? 1 : 0;
    return (
        (byte_swap << 4) |
        (clock_mode_50mhz << 3) |
        (card_type_block << 2) |
        (card_initialized << 1) |
        (card_inserted << 0)
    );
}

sd_error_t sd_card_get_info (uint32_t address) {
    if (!p.card_initialized) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    fpga_mem_write(address, sizeof(p.csd), p.csd);
    address += sizeof(p.csd);
    fpga_mem_write(address, sizeof(p.cid), p.cid);
    address += sizeof(p.cid);
    return SD_OK;
}

sd_error_t sd_set_byte_swap (bool enabled) {
    if (!p.card_initialized) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    p.byte_swap = enabled;
    return SD_OK;
}


sd_error_t sd_write_sectors (uint32_t address, uint32_t sector, uint32_t count) {
    if (!p.card_initialized) {
        return SD_ERROR_NOT_INITIALIZED;
    }

    if (count == 0) {
        return SD_ERROR_INVALID_ARGUMENT;
    }

    if (!p.card_type_block) {
        sector *= SD_SECTOR_SIZE;
    }

    while (count > 0) {
        uint32_t blocks = ((count > DAT_BLOCK_MAX_COUNT) ? DAT_BLOCK_MAX_COUNT : count);
        if (sd_cmd(25, sector, RSP_R1, NULL)) {
            return SD_ERROR_CMD25_IO;
        }
        sd_dat_prepare(address, blocks, DAT_WRITE);
        dat_error_t error = sd_dat_wait(DAT_TIMEOUT_DATA_MS);
        if (error != DAT_OK) {
            sd_cmd(12, 0, RSP_R1b, NULL);
            return (error == DAT_ERROR_IO) ? SD_ERROR_CMD25_CRC : SD_ERROR_CMD25_TIMEOUT;
        }
        sd_cmd(12, 0, RSP_R1b, NULL);
        address += (blocks * SD_SECTOR_SIZE);
        sector += (blocks * (p.card_type_block ? 1 : SD_SECTOR_SIZE));
        count -= blocks;
    }

    return false;
}

sd_error_t sd_read_sectors (uint32_t address, uint32_t sector, uint32_t count) {
    if (!p.card_initialized) {
        return SD_ERROR_NOT_INITIALIZED;
    }

    if (count == 0) {
        return SD_ERROR_INVALID_ARGUMENT;
    }

    if (p.byte_swap && ((address % 2) != 0)) {
        return SD_ERROR_INVALID_ARGUMENT;
    }

    if (!p.card_type_block) {
        sector *= SD_SECTOR_SIZE;
    }

    while (count > 0) {
        uint32_t blocks = ((count > DAT_BLOCK_MAX_COUNT) ? DAT_BLOCK_MAX_COUNT : count);
        sd_dat_prepare(address, blocks, DAT_READ);
        if (sd_cmd(18, sector, RSP_R1, NULL)) {
            sd_dat_abort();
            return SD_ERROR_CMD18_IO;
        }
        dat_error_t error = sd_dat_wait(DAT_TIMEOUT_DATA_MS);
        if (error != DAT_OK) {
            sd_cmd(12, 0, RSP_R1b, NULL);
            return (error == DAT_ERROR_IO) ? SD_ERROR_CMD18_CRC : SD_ERROR_CMD18_TIMEOUT;
        }
        sd_cmd(12, 0, RSP_R1b, NULL);
        address += (blocks * SD_SECTOR_SIZE);
        sector += (blocks * (p.card_type_block ? 1 : SD_SECTOR_SIZE));
        count -= blocks;
    }

    return SD_OK;
}


sd_error_t sd_optimize_sectors (uint32_t address, uint32_t *sector_table, uint32_t count, sd_process_sectors_t sd_process_sectors) {
    uint32_t starting_sector = 0;
    uint32_t sectors_to_process = 0;

    if (count == 0) {
        return SD_ERROR_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (sector_table[i] == 0) {
            return SD_ERROR_INVALID_ARGUMENT;
        }
        sectors_to_process += 1;
        if ((i < (count - 1)) && ((sector_table[i] + 1) == sector_table[i + 1])) {
            continue;
        }
        bool error = sd_process_sectors(address, sector_table[starting_sector], sectors_to_process);
        if (error) {
            return error;
        }
        address += (sectors_to_process * SD_SECTOR_SIZE);
        starting_sector += sectors_to_process;
        sectors_to_process = 0;
    }

    return SD_OK;
}

sd_error_t sd_get_lock (sd_lock_t lock) {
    if (p.lock == lock) {
        return SD_OK;
    }
    return SD_ERROR_LOCKED;
}

sd_error_t sd_try_lock (sd_lock_t lock) {
    if (p.lock == SD_LOCK_NONE) {
        p.lock = lock;
        return SD_OK;
    }
    return sd_get_lock(lock);
}

void sd_release_lock (sd_lock_t lock) {
    if (p.lock == lock) {
        p.lock = SD_LOCK_NONE;
    }
}


void sd_init (void) {
    p.card_initialized = false;
    p.byte_swap = false;
    p.lock = SD_LOCK_NONE;
    sd_set_clock(CLOCK_STOP);
}


void sd_process (void) {
    if (p.card_initialized && !sd_card_is_inserted()) {
        sd_card_deinit();
    }
}

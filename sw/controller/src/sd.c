#include <stdbool.h>
#include <stdint.h>
#include "sd.h"
#include "fpga.h"
#include "debug.h"
#include "hw.h"


#define CMD8_ARG_SUPPLY_VOLTAGE_27_36_V (1 << 8)
#define CMD8_ARG_CHECK_PATTERN          (0xAA << 0)

#define ACMD41_ARG_HCS                  (1 << 30)

#define R3_CCS                          (1 << 30)
#define R3_BUSY                         (1 << 31)

#define R7_SUPPLY_VOLTAGE_27_36_V       (1 << 8)
#define R7_CHECK_PATTERN                (0xAA << 0)


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


static bool sd_card_initialized;
static bool sd_card_type_block;
static uint32_t sd_rca = 0;
static volatile bool timeout = false;


static void sd_trigger_timeout (void) {
    timeout = true;
}

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
        fpga_reg_t rsp_regs[4] = {
            REG_SD_RSP_3,
            REG_SD_RSP_2,
            REG_SD_RSP_1,
            REG_SD_RSP_0
        };
        bool rsp_long = rsp_type & SD_CMD_LONG_RESPONSE;
        fpga_reg_t rsp_reg = (rsp_long ? 0 : (sizeof(rsp_regs) - 1));
        uint8_t *rsp_8 = (uint8_t *) (rsp);
        while (rsp_reg < sizeof(rsp_regs)) {
            uint32_t rsp_data = fpga_reg_get(rsp_regs[rsp_reg++]);
            uint8_t *rsp_data_8 = (uint8_t *) (&rsp_data);
            for (int i = 0; i < 4; i++) {
                *rsp_8++ = *rsp_data_8++;
            }
        };
    }

    if (rsp_type == RSP_R1b) {
        do {
            scr = fpga_reg_get(REG_SD_SCR);
        } while (scr & SD_SCR_CARD_BUSY);
    }

    return (scr & SD_SCR_CMD_ERROR);
}

static bool sd_acmd (uint8_t acmd, uint32_t arg, rsp_type_t rsp_type, void *rsp) {
    if (sd_cmd(55, sd_rca, RSP_R1, NULL)) {
        return true;
    }
    if (sd_cmd(acmd, arg, rsp_type, rsp)) {
        return true;
    }
    return false;
}


bool sd_read_sectors (uint32_t starting_sector, uint32_t address, uint32_t length) {
    if (!sd_card_initialized) {
        return true;
    }

    if ((length == 0) || (length % 512 != 0) || (length > (256 * 512))) {
        return true;
    }

    do {
        uint32_t blocks = (length / 512);

        timeout = false;
        hw_tim_setup(TIM_ID_GVR, 5000, sd_trigger_timeout);

        if (!sd_card_type_block) {
            starting_sector *= 512;
        }

        // fpga_reg_set(REG_SD_DMA_SCR, DMA_SCR_STOP);
        // fpga_reg_set(REG_SD_DAT, SD_DAT_STOP | SD_DAT_FIFO_FLUSH);

        fpga_reg_set(REG_SD_DMA_ADDRESS, address);
        fpga_reg_set(REG_SD_DMA_LENGTH, length);
        fpga_reg_set(REG_SD_DMA_SCR, DMA_SCR_DIRECTION | DMA_SCR_START);

        fpga_reg_set(REG_SD_DAT, (((blocks - 1) << SD_DAT_BLOCKS_BIT) | SD_DAT_START_READ | SD_DAT_FIFO_FLUSH));

        if (sd_cmd(23, blocks, RSP_R1, NULL)) {
            break;
        }

        if (sd_cmd(18, starting_sector, RSP_R1, NULL)) {
            break;
        }

        bool error = false;

        while (!timeout) {
            uint32_t sd_scr = fpga_reg_get(REG_SD_SCR);
            if (!(sd_scr & SD_SCR_CARD_INSERTED)) {
                error = true;
                break;
            }
            uint32_t sd_dat = fpga_reg_get(REG_SD_DAT);
            uint32_t sd_dma_scr = fpga_reg_get(REG_SD_DMA_SCR);
            if (sd_dat & SD_DAT_ERROR) {
                error = true;
                break;
            }
            if ((!(sd_dma_scr & DMA_SCR_BUSY)) && (!(sd_dat & SD_DAT_BUSY))) {
                break;
            }
        }

        if (timeout) {
            break;
        }

        hw_tim_stop(TIM_ID_GVR);

        if (error) {
            break;
        }

        return false;
    } while (0);

    fpga_reg_set(REG_SD_DMA_SCR, DMA_SCR_STOP);
    fpga_reg_set(REG_SD_DAT, SD_DAT_STOP | SD_DAT_FIFO_FLUSH);

    return true;
}

bool sd_card_initialize (void) {
    bool error;
    uint32_t arg;
    uint32_t rsp;
    bool version_2_or_later = false;

    if (sd_card_initialized) {
        return false;
    }

    sd_set_clock(CLOCK_400KHZ);

    do {
        sd_cmd(0, 0, RSP_NONE, NULL);

        arg = (CMD8_ARG_SUPPLY_VOLTAGE_27_36_V | CMD8_ARG_CHECK_PATTERN);
        if (!sd_cmd(8, arg, RSP_R7, &rsp)) {
            version_2_or_later = true;
            if (rsp != (R7_SUPPLY_VOLTAGE_27_36_V | R7_CHECK_PATTERN)) {
                break;
            }
        }

        arg = ((version_2_or_later ? ACMD41_ARG_HCS : 0) | 0x00FF8000);
        for (int i = 0; i < 4000; i++) {
            error = sd_acmd(41, arg, RSP_R3, &rsp);
            if (error || (rsp & R3_BUSY)) {
                break;
            }
        }
        if (error || ((rsp & 0x00FF8000) == 0)) {
            break;
        }
        sd_card_type_block = (rsp & R3_CCS);

        if (sd_cmd(2, 0, RSP_R2, NULL)) {
            break;
        }

        if (sd_cmd(3, 0, RSP_R6, &rsp)) {
            break;
        }
        sd_rca = rsp & 0xFFFF0000;

        if (sd_cmd(7, sd_rca, RSP_R1b, NULL)) {
            break;
        }

        if (sd_acmd(6, 2, RSP_R1, NULL)) {
            break;
        }

        sd_set_clock(CLOCK_50MHZ);

        sd_card_initialized = true;

        return false;
    } while (0);

    sd_rca = 0;
    sd_cmd(0, 0, RSP_NONE, NULL);
    sd_set_clock(CLOCK_STOP);

    return true;
}

void sd_init (void) {
    sd_card_initialized = false;
    sd_set_clock(CLOCK_STOP);
}

void sd_process (void) {
    if (!(fpga_reg_get(REG_SD_SCR) & SD_SCR_CARD_INSERTED)) {
        if (sd_card_initialized) {
            sd_card_initialized = false;
            sd_rca = 0;
            sd_set_clock(CLOCK_STOP);
        }
    }
}

#include <stdbool.h>
#include "fpga.h"
#include "hw.h"
#include "vendor.h"


#define VENDOR_SCR_START        (1 << 0)
#define VENDOR_SCR_BUSY         (1 << 0)
#define VENDOR_SCR_WRITE        (1 << 1)
#define VENDOR_SCR_LENGTH_BIT   (2)
#define VENDOR_SCR_DELAY        (1 << 4)
#define VENDOR_SCR_ADDRESS_BIT  (8)

#define LCMXO2_I2C_ADDR_CFG     (0x80)
#define LCMXO2_I2C_ADDR_RESET   (0x86)

#define LCMXO2_CFGCR            (0x70)
#define LCMXO2_CFGTXDR          (0x71)
#define LCMXO2_CFGRXDR          (0x73)

#define CFGCR_RSTE              (1 << 6)
#define CFGCR_WBCE              (1 << 7)

#define ISC_ERASE               (0x0E)
#define ISC_DISABLE             (0x26)
#define LSC_READ_STATUS         (0x3C)
#define LSC_INIT_ADDRESS        (0x46)
#define ISC_PROGRAM_DONE        (0x5E)
#define LSC_PROG_INCR_NV        (0x70)
#define LSC_READ_INCR_NV        (0x73)
#define ISC_ENABLE_X            (0x74)
#define LSC_REFRESH             (0x79)
#define ISC_ENABLE              (0xC6)
#define IDCODE_PUB              (0xE0)
#define LSC_PROG_FEABITS        (0xF8)
#define LSC_READ_FEABITS        (0xFB)
#define ISC_NOOP                (0xFF)

#define ISC_ERASE_FEATURE       (1 << 17)
#define ISC_ERASE_CFG           (1 << 18)
#define ISC_ERASE_UFM           (1 << 19)

#define LSC_STATUS_CFG_ENABLE   (1 << 9)
#define LSC_STATUS_BUSY         (1 << 12)
#define LSC_STATUS_FAIL         (1 << 13)

#define DEVICE_ID_SIZE          (4)

#define FLASH_PAGE_SIZE         (16)
#define FLASH_NUM_PAGES         (11260)

#define FEATBITS_SIZE           (2)


typedef enum {
    CMD_NORMAL,
    CMD_DELAYED,
    CMD_TWO_OP,
} cmd_type_t;


#ifndef LCMXO2_I2C
static void lcmxo2_reg_set (uint8_t reg, uint8_t value) {
    fpga_reg_set(REG_VENDOR_DATA, value << 24);
    fpga_reg_set(REG_VENDOR_SCR,
        (reg << VENDOR_SCR_ADDRESS_BIT) |
        (0 << VENDOR_SCR_LENGTH_BIT) |
        VENDOR_SCR_WRITE |
        VENDOR_SCR_START
    );
    while (fpga_reg_get(REG_VENDOR_SCR) & VENDOR_SCR_BUSY);
}

static void lcmxo2_read_data (uint8_t *buffer, uint32_t length) {
    while (length > 0) {
        uint32_t block_size = (length > 4) ? 4 : length;
        fpga_reg_set(REG_VENDOR_SCR,
            (LCMXO2_CFGRXDR << VENDOR_SCR_ADDRESS_BIT) |
            ((block_size - 1) << VENDOR_SCR_LENGTH_BIT) |
            VENDOR_SCR_START
        );
        while (fpga_reg_get(REG_VENDOR_SCR) & VENDOR_SCR_BUSY);
        uint32_t data = fpga_reg_get(REG_VENDOR_DATA);
        data <<= ((4 - block_size) * 8);
        for (int i = 0; i < block_size; i++) {
            *buffer++ = ((data >> 24) & 0xFF);
            data <<= 8;
            length -= 1;
        }
    }
}

static void lcmxo2_write_data (uint8_t *buffer, uint32_t length) {
    while (length > 0) {
        uint32_t block_size = (length > 4) ? 4 : length;
        uint32_t data = 0;
        for (int i = 0; i < block_size; i++) {
            data = ((data << 8) | *buffer++);
            length -= 1;
        }
        data <<= ((4 - block_size) * 8);
        fpga_reg_set(REG_VENDOR_DATA, data);
        fpga_reg_set(REG_VENDOR_SCR,
            (LCMXO2_CFGTXDR << VENDOR_SCR_ADDRESS_BIT) |
            ((block_size - 1) << VENDOR_SCR_LENGTH_BIT) |
            VENDOR_SCR_WRITE |
            VENDOR_SCR_START
        );
        while (fpga_reg_get(REG_VENDOR_SCR) & VENDOR_SCR_BUSY);
    }
}
#endif

static void lcmxo2_reset_bus (void) {
#ifdef LCMXO2_I2C
    uint8_t reset_data = 0;
    hw_i2c_trx(LCMXO2_I2C_ADDR_RESET, &reset_data, sizeof(reset_data), NULL, 0);
#else
    lcmxo2_reg_set(LCMXO2_CFGCR, CFGCR_RSTE);
    lcmxo2_reg_set(LCMXO2_CFGCR, 0);
#endif
}

static bool lcmxo2_execute_cmd (uint8_t cmd, uint32_t arg, cmd_type_t type, uint8_t *buffer, uint8_t length, bool write) {
#ifdef LCMXO2_I2C
    uint8_t packet[20] = { cmd, ((arg >> 16) & 0xFF), ((arg >> 8) & 0xFF), (arg & 0xFF) };
    int packet_length = ((type == CMD_TWO_OP) ? 3 : 4);

    if (write) {
        for (int i = 0; i < length; i++) {
            packet[packet_length + i] = buffer[i];
        }
        packet_length += length;
    }

    i2c_error_t error = hw_i2c_trx(LCMXO2_I2C_ADDR_CFG, packet, packet_length, buffer, (write ? 0 : length));

    return (error != I2C_OK);
#else
    lcmxo2_reg_set(LCMXO2_CFGCR, CFGCR_WBCE);

    uint32_t data = (cmd << 24) | (arg & 0x00FFFFFF);

    fpga_reg_set(REG_VENDOR_DATA, data);
    fpga_reg_set(REG_VENDOR_SCR,
        (LCMXO2_CFGTXDR << VENDOR_SCR_ADDRESS_BIT) |
        (type == CMD_DELAYED ? VENDOR_SCR_DELAY : 0) |
        ((type == CMD_TWO_OP ? 2 : 3) << VENDOR_SCR_LENGTH_BIT) |
        VENDOR_SCR_WRITE |
        VENDOR_SCR_START
    );

    while (fpga_reg_get(REG_VENDOR_SCR) & VENDOR_SCR_BUSY);

    if (length > 0) {
        if (write) {
            lcmxo2_write_data(buffer, length);
        } else {
            lcmxo2_read_data(buffer, length);
        }
    }

    lcmxo2_reg_set(LCMXO2_CFGCR, 0);

    return false;
#endif
}

static bool lcmxo2_read_device_id (uint8_t *id) {
    return lcmxo2_execute_cmd(IDCODE_PUB, 0, CMD_NORMAL, id, DEVICE_ID_SIZE, false);
}

static uint32_t lcmxo2_read_status (uint32_t *status) {
    uint32_t tmp = 0;
    bool error = lcmxo2_execute_cmd(LSC_READ_STATUS, 0, CMD_NORMAL, (uint8_t *) (&tmp), 4, false);
    *status = SWAP32(tmp);
    return error;
}

static bool lcmxo2_wait_busy (void) {
    bool error;
    uint32_t status;
    do {
        error = lcmxo2_read_status(&status);
    } while ((!error) && (status & LSC_STATUS_BUSY));
    return (error) || (status & LSC_STATUS_FAIL);
}

static bool lcmxo2_enable_flash (void) {
#ifdef LCMXO2_I2C
    if (lcmxo2_execute_cmd(ISC_ENABLE, 0x080000, CMD_TWO_OP, NULL, 0, false)) {
        return true;
    }
#else
    if (lcmxo2_execute_cmd(ISC_ENABLE_X, 0x080000, CMD_NORMAL, NULL, 0, false)) {
        return true;
    }
#endif
    return lcmxo2_wait_busy();
}

static bool lcmxo2_disable_flash (void) {
    if (lcmxo2_wait_busy()) {
        return true;
    }
    if (lcmxo2_execute_cmd(ISC_DISABLE, 0, CMD_TWO_OP, NULL, 0, false)) {
        return true;
    }
    return lcmxo2_execute_cmd(ISC_NOOP, 0xFFFFFF, CMD_NORMAL, NULL, 0, false);
}

static bool lcmxo2_erase_featbits (void) {
    if (lcmxo2_execute_cmd(ISC_ERASE, ISC_ERASE_FEATURE, CMD_NORMAL, NULL, 0, false)) {
        return true;
    }
    return lcmxo2_wait_busy();
}

static bool lcmxo2_erase_flash (void) {
    if (lcmxo2_execute_cmd(ISC_ERASE, (ISC_ERASE_UFM | ISC_ERASE_CFG), CMD_NORMAL, NULL, 0, false)) {
        return true;
    }
    return lcmxo2_wait_busy();
}

static bool lcmxo2_reset_flash_address (void) {
    return lcmxo2_execute_cmd(LSC_INIT_ADDRESS, 0, CMD_NORMAL, NULL, 0, false);
}

static bool lcmxo2_write_flash_page (uint8_t *buffer) {
    if (lcmxo2_execute_cmd(LSC_PROG_INCR_NV, 1, CMD_NORMAL, buffer, FLASH_PAGE_SIZE, true)) {
        return true;
    }
    return lcmxo2_wait_busy();
}

static bool lcmxo2_read_flash_page (uint8_t *buffer) {
    return lcmxo2_execute_cmd(LSC_READ_INCR_NV, 1, CMD_DELAYED, buffer, FLASH_PAGE_SIZE, false);
}

static bool lcmxo2_program_done (void) {
    if (lcmxo2_execute_cmd(ISC_PROGRAM_DONE, 0, CMD_NORMAL, NULL, 0, false)) {
        return true;
    }
    return lcmxo2_wait_busy();
}

static bool lcmxo2_write_featbits (uint8_t *buffer) {
    if (lcmxo2_execute_cmd(LSC_PROG_FEABITS, 0, CMD_NORMAL, buffer, FEATBITS_SIZE, true)) {
        return true;
    }
    return lcmxo2_wait_busy();
}

static bool lcmxo2_read_featbits (uint8_t *buffer) {
    return lcmxo2_execute_cmd(LSC_READ_FEABITS, 0, CMD_NORMAL, buffer, FEATBITS_SIZE, false);
}

static bool lcmxo2_refresh (void) {
    return lcmxo2_execute_cmd(LSC_REFRESH, 0, CMD_TWO_OP, NULL, 0, false);
}

static vendor_error_t lcmxo2_fail (vendor_error_t error) {
    lcmxo2_disable_flash();
    return error;
}


uint32_t vendor_flash_size (void) {
    return (FLASH_PAGE_SIZE * FLASH_NUM_PAGES);
}

vendor_error_t vendor_backup (uint32_t address, uint32_t *length) {
    uint8_t buffer[FLASH_PAGE_SIZE];

    *length = 0;

    lcmxo2_reset_bus();
    if (lcmxo2_enable_flash()) {
        return lcmxo2_fail(VENDOR_ERROR_INIT);
    }
    lcmxo2_reset_flash_address();
    for (int i = 0; i < (FLASH_PAGE_SIZE * FLASH_NUM_PAGES); i += FLASH_PAGE_SIZE) {
        lcmxo2_read_flash_page(buffer);
        fpga_mem_write(address + i, FLASH_PAGE_SIZE, buffer);
        *length += FLASH_PAGE_SIZE;
    }
    lcmxo2_disable_flash();

    return VENDOR_OK;
}

vendor_error_t vendor_update (uint32_t address, uint32_t length) {
    uint8_t buffer[FLASH_PAGE_SIZE];
    uint8_t verify_buffer[FLASH_PAGE_SIZE];

    if (length == 0) {
        return VENDOR_ERROR_ARGS;
    }
    if ((length % FLASH_PAGE_SIZE) != 0) {
        return VENDOR_ERROR_ARGS;
    }
    if (length > (FLASH_PAGE_SIZE * FLASH_NUM_PAGES)) {
        return VENDOR_ERROR_ARGS;
    }

    lcmxo2_reset_bus();
    if (lcmxo2_enable_flash()) {
        return lcmxo2_fail(VENDOR_ERROR_INIT);
    }
    if (lcmxo2_erase_flash()) {
        return lcmxo2_fail(VENDOR_ERROR_ERASE);
    }
    lcmxo2_reset_flash_address();
    for (int i = 0; i < length; i += FLASH_PAGE_SIZE) {
        fpga_mem_read(address + i, FLASH_PAGE_SIZE, buffer);
        if (lcmxo2_write_flash_page(buffer)) {
            return lcmxo2_fail(VENDOR_ERROR_PROGRAM);
        }
    }
    if (lcmxo2_program_done()) {
        return lcmxo2_fail(VENDOR_ERROR_PROGRAM);
    }
    lcmxo2_reset_flash_address();
    for (int i = 0; i < length; i += FLASH_PAGE_SIZE) {
        lcmxo2_read_flash_page(buffer);
        fpga_mem_read(address + i, FLASH_PAGE_SIZE, verify_buffer);
        for (int x = 0; x < FLASH_PAGE_SIZE; x++) {
            if (buffer[x] != verify_buffer[x]) {
                return lcmxo2_fail(VENDOR_ERROR_VERIFY);
            }
        }
    }
    lcmxo2_disable_flash();

    return VENDOR_OK;
}

vendor_error_t vendor_reconfigure (void) {
    lcmxo2_reset_bus();
    lcmxo2_refresh();

    return VENDOR_OK;
}


#define FEATBITS_0_SPI_OFF      (1 << 1)
#define FEATBITS_1_PROGRAMN_OFF (1 << 5)


typedef enum {
    CMD_GET_PRIMER_ID   = '?',
    CMD_PROBE_FPGA      = '#',
    CMD_RESTART         = '$',
    CMD_GET_DEVICE_ID   = 'I',
    CMD_ENABLE_FLASH    = 'E',
    CMD_ERASE_FLASH     = 'X',
    CMD_RESET_ADDRESS   = 'A',
    CMD_WRITE_PAGE      = 'W',
    CMD_READ_PAGE       = 'R',
    CMD_PROGRAM_DONE    = 'F',
    CMD_INIT_FEATBITS   = 'Q',
    CMD_REFRESH         = 'B',
} primer_cmd_e;


static bool primer_check_rx_length (primer_cmd_e cmd, size_t rx_length) {
    switch (cmd) {
        case CMD_WRITE_PAGE:
            return (rx_length != FLASH_PAGE_SIZE);
        default:
            return (rx_length != 0);
    }
    return true;
}

static bool lcmxo2_init_featbits (void) {
    uint8_t programmed[2] = { 0x00, 0x00 };
    uint8_t target[2] = { FEATBITS_0_SPI_OFF, FEATBITS_1_PROGRAMN_OFF };

    if (lcmxo2_read_featbits(programmed)) {
        return true;
    }

    if ((programmed[0] == target[0]) && (programmed[1] == target[1])) {
        return false;
    }

    if (lcmxo2_erase_featbits()) {
        return true;
    }
    if (lcmxo2_write_featbits(target)) {
        return true;
    }
    if (lcmxo2_read_featbits(programmed)) {
        return true;
    }

    if ((programmed[0] != target[0]) || (programmed[1] != target[1])) {
        return true;
    }

    return false;
}


void vendor_initial_configuration (vendor_get_cmd_t get_cmd, vendor_send_response_t send_response) {
    bool runninng = true;
    primer_cmd_e cmd;
    uint8_t buffer[256];
    uint8_t rx_length;
    uint8_t tx_length;
    bool error;

    lcmxo2_reset_bus();

    while (runninng) {
        cmd = get_cmd(buffer, &rx_length);
        tx_length = 0;
        error = false;

        if (primer_check_rx_length(cmd, rx_length)) {
            send_response(cmd, NULL, 0, true);
            continue;
        }

        switch (cmd) {
            case CMD_GET_PRIMER_ID:
                buffer[0] = 'M';
                buffer[1] = 'X';
                buffer[2] = 'O';
                buffer[3] = '2';
                tx_length = 4;
                break;

            case CMD_PROBE_FPGA:
                buffer[0] = fpga_id_get();
                tx_length = 1;
                break;

            case CMD_RESTART:
                runninng = false;
                break;

            case CMD_GET_DEVICE_ID:
                error = lcmxo2_read_device_id(buffer);
                tx_length = 4;
                break;

            case CMD_ENABLE_FLASH:
                error = lcmxo2_enable_flash();
                break;

            case CMD_ERASE_FLASH:
                error = lcmxo2_erase_flash();
                break;

            case CMD_RESET_ADDRESS:
                error = lcmxo2_reset_flash_address();
                break;

            case CMD_WRITE_PAGE:
                error = lcmxo2_write_flash_page(buffer);
                break;

            case CMD_READ_PAGE:
                error = lcmxo2_read_flash_page(buffer);
                tx_length = FLASH_PAGE_SIZE;
                break;

            case CMD_PROGRAM_DONE:
                error = lcmxo2_program_done();
                break;

            case CMD_INIT_FEATBITS:
                error = lcmxo2_init_featbits();
                break;

            case CMD_REFRESH:
                error = lcmxo2_refresh();
                hw_delay_ms(200);
                break;

            default:
                error = true;
                break;
        }

        send_response(cmd, buffer, tx_length, error);
    }
}

#include <stdbool.h>
#include "fpga.h"
#include "vendor.h"


#define VENDOR_SCR_START        (1 << 0)
#define VENDOR_SCR_BUSY         (1 << 0)
#define VENDOR_SCR_WRITE        (1 << 1)
#define VENDOR_SCR_LENGTH_BIT   (2)
#define VENDOR_SCR_DELAY        (1 << 4)
#define VENDOR_SCR_ADDRESS_BIT  (8)

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
#define ISC_NOOP                (0xFF)

#define ISC_ERASE_CFG           (1 << 18)
#define ISC_ERASE_UFM           (1 << 19)

#define LSC_STATUS_1_BUSY       (1 << 4)
#define LSC_STATUS_1_FAIL       (1 << 5)

#define FLASH_PAGE_SIZE         (16)
#define FLASH_NUM_PAGES         (11260)


typedef enum {
    CMD_NORMAL,
    CMD_DELAYED,
    CMD_TWO_OP,
} cmd_type_t;


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

static uint8_t lcmxo2_reg_get (uint8_t reg) {
    fpga_reg_set(REG_VENDOR_SCR,
        (reg << VENDOR_SCR_ADDRESS_BIT) |
        (0 << VENDOR_SCR_LENGTH_BIT) |
        VENDOR_SCR_START
    );
    while (fpga_reg_get(REG_VENDOR_SCR) & VENDOR_SCR_BUSY);
    return fpga_reg_get(REG_VENDOR_DATA) & 0xFF;
}

static void lcmxo2_reset_bus (void) {
    lcmxo2_reg_set(LCMXO2_CFGCR, CFGCR_RSTE);
    lcmxo2_reg_set(LCMXO2_CFGCR, 0);
}

static void lcmxo2_execute_cmd (uint8_t cmd, uint32_t arg, cmd_type_t type) {
    lcmxo2_reg_set(LCMXO2_CFGCR, 0);
    uint32_t data = (cmd << 24) | (arg & 0x00FFFFFF);
    lcmxo2_reg_set(LCMXO2_CFGCR, CFGCR_WBCE);
    fpga_reg_set(REG_VENDOR_DATA, data);
    fpga_reg_set(REG_VENDOR_SCR,
        (LCMXO2_CFGTXDR << VENDOR_SCR_ADDRESS_BIT) |
        (type == CMD_DELAYED ? VENDOR_SCR_DELAY : 0) |
        ((type == CMD_TWO_OP ? 2 : 3) << VENDOR_SCR_LENGTH_BIT) |
        VENDOR_SCR_WRITE |
        VENDOR_SCR_START
    );
    while (fpga_reg_get(REG_VENDOR_SCR) & VENDOR_SCR_BUSY);
}

static void lcmxo2_cleanup (void) {
    lcmxo2_reg_set(LCMXO2_CFGCR, 0);
}

static void lcmxo2_read_data (uint8_t *buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        *buffer++ = lcmxo2_reg_get(LCMXO2_CFGRXDR);
    }
}

static void lcmxo2_write_data (uint8_t *buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        lcmxo2_reg_set(LCMXO2_CFGTXDR, *buffer++);
    }
}

static bool lcmxo2_wait_busy (void) {
    uint8_t status[4];
    do {
        lcmxo2_execute_cmd(LSC_READ_STATUS, 0, CMD_NORMAL);
        lcmxo2_read_data(status, 4);
    } while(status[2] & LSC_STATUS_1_BUSY);
    return status[2] & LSC_STATUS_1_FAIL;
}

static bool lcmxo2_enable_flash (void) {
    lcmxo2_execute_cmd(ISC_ENABLE_X, 0x080000, CMD_NORMAL);
    return lcmxo2_wait_busy();
}

static void lcmxo2_disable_flash (void) {
    lcmxo2_wait_busy();
    lcmxo2_execute_cmd(ISC_DISABLE, 0, CMD_TWO_OP);
    lcmxo2_execute_cmd(ISC_NOOP, 0xFFFFFF, CMD_NORMAL);
}

static bool lcmxo2_erase_flash (void) {
    lcmxo2_execute_cmd(ISC_ERASE, ISC_ERASE_UFM | ISC_ERASE_CFG, CMD_NORMAL);
    return lcmxo2_wait_busy();
}

static void lcmxo2_reset_flash_address (void) {
    lcmxo2_execute_cmd(LSC_INIT_ADDRESS, 0, CMD_NORMAL);
}

static bool lcmxo2_write_flash_page (uint8_t *buffer) {
    lcmxo2_execute_cmd(LSC_PROG_INCR_NV, 1, CMD_NORMAL);
    lcmxo2_write_data(buffer, FLASH_PAGE_SIZE);
    return lcmxo2_wait_busy();
}

static void lcmxo2_read_flash_page (uint8_t *buffer) {
    lcmxo2_execute_cmd(LSC_READ_INCR_NV, 1, CMD_DELAYED);
    lcmxo2_read_data(buffer, FLASH_PAGE_SIZE);
}

static void lcmxo2_program_done (void) {
    lcmxo2_execute_cmd(ISC_PROGRAM_DONE, 0, CMD_NORMAL);
    lcmxo2_wait_busy();
}

static void lcmxo2_refresh (void) {
    lcmxo2_execute_cmd(LSC_REFRESH, 0, CMD_TWO_OP);
}

static vendor_error_t lcmxo2_fail (vendor_error_t error) {
    lcmxo2_disable_flash();
    lcmxo2_cleanup();
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
    lcmxo2_cleanup();

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
    lcmxo2_program_done();
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
    lcmxo2_cleanup();

    return VENDOR_OK;
}

vendor_error_t vendor_reconfigure (void) {
    lcmxo2_reset_bus();
    lcmxo2_refresh();
    lcmxo2_cleanup();

    return VENDOR_OK;
}

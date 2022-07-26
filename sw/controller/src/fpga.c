#include "fpga.h"
#include "hw.h"


uint8_t fpga_id_get (void) {
    fpga_cmd_t cmd = CMD_IDENTIFY;
    uint8_t id;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&id, 1, SPI_RX);
    hw_spi_stop();

    return id;
}

uint32_t fpga_reg_get (fpga_reg_t reg) {
    fpga_cmd_t cmd = CMD_REG_READ;
    uint32_t value;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&reg, 1, SPI_TX);
    hw_spi_trx((uint8_t *) (&value), 4, SPI_RX);
    hw_spi_stop();

    return value;
}

void fpga_reg_set (fpga_reg_t reg, uint32_t value) {
    fpga_cmd_t cmd = CMD_REG_WRITE;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&reg, 1, SPI_TX);
    hw_spi_trx((uint8_t *) (&value), 4, SPI_TX);
    hw_spi_stop();
}

void fpga_mem_read (uint32_t address, size_t length, uint8_t *buffer) {
    fpga_cmd_t cmd = CMD_MEM_READ;
    uint8_t buffer_address = 0;

    fpga_reg_set(REG_MEM_ADDRESS, address);
    fpga_reg_set(REG_MEM_SCR, (length << MEM_SCR_LENGTH_BIT) | MEM_SCR_START);
    while (fpga_reg_get(REG_MEM_SCR) & MEM_SCR_BUSY);

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&buffer_address, 1, SPI_TX);
    hw_spi_trx(buffer, length, SPI_RX);
    hw_spi_stop();
}

void fpga_mem_write (uint32_t address, size_t length, uint8_t *buffer) {
    fpga_cmd_t cmd = CMD_MEM_WRITE;
    uint8_t buffer_address = 0;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&buffer_address, 1, SPI_TX);
    hw_spi_trx(buffer, length, SPI_TX);
    hw_spi_stop();

    fpga_reg_set(REG_MEM_ADDRESS, address);
    fpga_reg_set(REG_MEM_SCR, (length << MEM_SCR_LENGTH_BIT) | MEM_SCR_DIRECTION | MEM_SCR_START);
    while (fpga_reg_get(REG_MEM_SCR) & MEM_SCR_BUSY);
}

void fpga_mem_copy (uint32_t src, uint32_t dst, size_t length) {
    fpga_reg_set(REG_MEM_ADDRESS, src);
    fpga_reg_set(REG_MEM_SCR, (length << MEM_SCR_LENGTH_BIT) | MEM_SCR_START);
    while (fpga_reg_get(REG_MEM_SCR) & MEM_SCR_BUSY);

    fpga_reg_set(REG_MEM_ADDRESS, dst);
    fpga_reg_set(REG_MEM_SCR, (length << MEM_SCR_LENGTH_BIT) | MEM_SCR_DIRECTION | MEM_SCR_START);
    while (fpga_reg_get(REG_MEM_SCR) & MEM_SCR_BUSY);
}

uint8_t fpga_usb_status_get (void) {
    fpga_cmd_t cmd = CMD_USB_STATUS;
    uint8_t status;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&status, 1, SPI_RX);
    hw_spi_stop();

    return status;
}

uint8_t fpga_usb_pop (void) {
    fpga_cmd_t cmd = CMD_USB_READ;
    uint8_t data;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&data, 1, SPI_RX);
    hw_spi_stop();

    return data;
}

void fpga_usb_push (uint8_t data) {
    fpga_cmd_t cmd = CMD_USB_WRITE;

    hw_spi_start();
    hw_spi_trx((uint8_t *) (&cmd), 1, SPI_TX);
    hw_spi_trx(&data, 1, SPI_TX);
    hw_spi_stop();
}

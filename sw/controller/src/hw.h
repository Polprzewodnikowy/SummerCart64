#ifndef HW_H__
#define HW_H__


#include <stdint.h>


#define GPIO_PORT_PIN(p, n)     ((((p) & 0x07) << 4) | ((n) & 0x0F))


typedef enum {
    GPIO_ID_N64_RESET   = GPIO_PORT_PIN(0, 0),
    GPIO_ID_N64_CIC_CLK = GPIO_PORT_PIN(0, 1),
    GPIO_ID_N64_CIC_DQ  = GPIO_PORT_PIN(0, 2),
    GPIO_ID_LED         = GPIO_PORT_PIN(0, 3),
    GPIO_ID_SPI_CS      = GPIO_PORT_PIN(0, 4),
    GPIO_ID_SPI_CLK     = GPIO_PORT_PIN(0, 5),
    GPIO_ID_SPI_MISO    = GPIO_PORT_PIN(0, 6),
    GPIO_ID_SPI_MOSI    = GPIO_PORT_PIN(0, 7),
    GPIO_ID_UART_TX     = GPIO_PORT_PIN(0, 9),
    GPIO_ID_UART_RX     = GPIO_PORT_PIN(0, 10),
    GPIO_ID_FPGA_INT    = GPIO_PORT_PIN(1, 2),
    GPIO_ID_I2C_SCL     = GPIO_PORT_PIN(1, 6),
    GPIO_ID_I2C_SDA     = GPIO_PORT_PIN(1, 7),
    GPIO_ID_RTC_MFP     = GPIO_PORT_PIN(1, 9),
} gpio_id_t;

typedef enum {
    I2C_OK,
    I2C_ERROR_BUSY,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_NACK,
} i2c_error_t;

typedef uint64_t hw_flash_t;

typedef enum {
    LOADER_FLAGS_UPDATE_MCU = (1 << 0),
    LOADER_FLAGS_UPDATE_FPGA = (1 << 1),
    LOADER_FLAGS_UPDATE_BOOTLOADER = (1 << 2),
} loader_parameters_flags_t;

typedef struct {
    uint32_t magic;
    loader_parameters_flags_t flags;
    uint32_t mcu_address;
    uint32_t fpga_address;
    uint32_t bootloader_address;
} loader_parameters_t;


void hw_set_vector_table (uint32_t offset);

void hw_enter_critical (void);
void hw_exit_critical (void);

void hw_delay_us (uint32_t delay_us);
void hw_delay_ms (uint32_t delay_ms);

void hw_systick_config (uint32_t period_ms, void (*callback) (void));

uint32_t hw_gpio_get (gpio_id_t id);
void hw_gpio_set (gpio_id_t id);
void hw_gpio_reset (gpio_id_t id);

void hw_uart_read (uint8_t *data, int length);
void hw_uart_write (uint8_t *data, int length);
void hw_uart_write_wait_busy (void);

void hw_spi_start (void);
void hw_spi_stop (void);
void hw_spi_rx (uint8_t *data, int length);
void hw_spi_tx (uint8_t *data, int length);

i2c_error_t hw_i2c_trx (uint8_t i2c_address, uint8_t *tx_data, uint8_t tx_length, uint8_t *rx_data, uint8_t rx_length);

void hw_crc32_reset (void);
uint32_t hw_crc32_calculate (uint8_t *data, uint32_t length);

uint32_t hw_flash_size (void);
hw_flash_t hw_flash_read (uint32_t offset);
void hw_flash_erase (void);
void hw_flash_program (uint32_t offset, hw_flash_t value);

void hw_reset (loader_parameters_t *parameters);

void hw_loader_get_parameters (loader_parameters_t *parameters);

void hw_adc_read_voltage_temperature (uint16_t *voltage, int16_t *temperature);

void hw_primer_init (void);
void hw_loader_init (void);
void hw_app_init (void);


#endif

#include <stdbool.h>
#include <stddef.h>
#include <stm32g0xx.h>
#include "hw.h"


#define CPU_FREQ    (64000000UL)


void hw_set_vector_table (uint32_t offset) {
    SCB->VTOR = (__IOM uint32_t) (offset);
}


void hw_enter_critical (void) {
    __disable_irq();
}

void hw_exit_critical (void) {
    __enable_irq();
}


static void hw_clock_init (void) {
    FLASH->ACR |= (FLASH_ACR_PRFTEN | (2 << FLASH_ACR_LATENCY_Pos));
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != (2 << FLASH_ACR_LATENCY_Pos));

    RCC->PLLCFGR = (
        ((2 - 1) << RCC_PLLCFGR_PLLR_Pos)
        | RCC_PLLCFGR_PLLREN
        | (16 << RCC_PLLCFGR_PLLN_Pos)
        | ((2 - 1) << RCC_PLLCFGR_PLLM_Pos)
        | RCC_PLLCFGR_PLLSRC_HSI
    );

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY_Msk) != RCC_CR_PLLRDY);

    RCC->CFGR = RCC_CFGR_SW_1;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_1);
}


#define TIMEOUT_US_PER_TICK (10)

static void hw_timeout_init (void) {
    RCC->APBENR1 |= RCC_APBENR1_DBGEN;
    DBG->APBFZ2 |= DBG_APB_FZ2_DBG_TIM17_STOP;

    RCC->APBENR2 |= RCC_APBENR2_TIM17EN;

    TIM17->CR1 = TIM_CR1_OPM;
    TIM17->PSC = (((CPU_FREQ / 1000 / 1000) * TIMEOUT_US_PER_TICK) - 1);
    TIM17->EGR = TIM_EGR_UG;
}

static void hw_timeout_start (void) {
    TIM17->CR1 &= ~(TIM_CR1_CEN);
    TIM17->CNT = 0;
    TIM17->CR1 |= TIM_CR1_CEN;
}

static bool hw_timeout_occured (uint32_t timeout_us) {
    uint16_t count = TIM17->CNT;

    uint32_t adjusted_timeout = ((timeout_us + (TIMEOUT_US_PER_TICK - 1)) / TIMEOUT_US_PER_TICK);

    if ((count >= adjusted_timeout) || (count == 0xFFFF)) {
        return true;
    }

    return false;
}


#define DELAY_MS_PER_TICK   (1)

static void hw_delay_init (void) {
    RCC->APBENR1 |= RCC_APBENR1_DBGEN;
    DBG->APBFZ2 |= DBG_APB_FZ2_DBG_TIM16_STOP;

    RCC->APBENR2 |= RCC_APBENR2_TIM16EN;

    TIM16->CR1 = TIM_CR1_OPM;
    TIM16->PSC = (((CPU_FREQ / 1000) * DELAY_MS_PER_TICK) - 1);
    TIM16->EGR = TIM_EGR_UG;
}

void hw_delay_ms (uint32_t delay_ms) {
    TIM16->CR1 &= ~(TIM_CR1_CEN);
    TIM16->CNT = 0;
    TIM16->CR1 |= TIM_CR1_CEN;

    uint32_t adjusted_delay = ((delay_ms + (DELAY_MS_PER_TICK - 1)) / DELAY_MS_PER_TICK);

    uint16_t count;
    do {
        count = TIM16->CNT;
    } while ((count < adjusted_delay) && (count != 0xFFFF));
}


static void (*systick_callback) (void) = NULL;

void hw_systick_config (uint32_t period_ms, void (*callback) (void)) {
    SysTick_Config((CPU_FREQ / 1000) * period_ms);
    systick_callback = callback;
}

void SysTick_Handler (void) {
    if (systick_callback) {
        systick_callback();
    }
}


typedef enum {
    GPIO_INPUT          = 0b00,
    GPIO_OUTPUT         = 0b01,
    GPIO_ALT            = 0b10,
    GPIO_ANALOG         = 0b11
} gpio_mode_t;

typedef enum {
    GPIO_PP             = 0b0,
    GPIO_OD             = 0b1
} gpio_ot_t;

typedef enum {
    GPIO_SPEED_VLOW     = 0b00,
    GPIO_SPEED_LOW      = 0b01,
    GPIO_SPEED_HIGH     = 0b10,
    GPIO_SPEED_VHIGH    = 0b11
} gpio_ospeed_t;

typedef enum {
    GPIO_PULL_NONE      = 0b00,
    GPIO_PULL_UP        = 0b01,
    GPIO_PULL_DOWN      = 0b10
} gpio_pupd_t;

typedef enum {
    GPIO_AF_0           = 0x00,
    GPIO_AF_1           = 0x01,
    GPIO_AF_2           = 0x02,
    GPIO_AF_3           = 0x03,
    GPIO_AF_4           = 0x04,
    GPIO_AF_5           = 0x05,
    GPIO_AF_6           = 0x06,
    GPIO_AF_7           = 0x07
} gpio_af_t;

static const GPIO_TypeDef *gpios[] = { GPIOA, GPIOB, 0, 0, 0, 0, 0, 0 };

static void hw_gpio_init (gpio_id_t id, gpio_mode_t mode, gpio_ot_t ot, gpio_ospeed_t ospeed, gpio_pupd_t pupd, gpio_af_t af, int value) {
    GPIO_TypeDef tmp;
    GPIO_TypeDef *gpio = ((GPIO_TypeDef *) (gpios[(id >> 4) & 0x07]));
    uint8_t pin = (id & 0x0F);
    uint8_t afr = ((pin < 8) ? 0 : 1);

    RCC->IOPENR |= (RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN);

    tmp.MODER = (gpio->MODER & ~(GPIO_MODER_MODE0_Msk << (pin * 2)));
    tmp.OTYPER = (gpio->OTYPER & ~(GPIO_OTYPER_OT0_Msk << pin));
    tmp.OSPEEDR = (gpio->OSPEEDR & ~(GPIO_OSPEEDR_OSPEED0_Msk << (pin * 2)));
    tmp.PUPDR = (gpio->PUPDR & ~(GPIO_PUPDR_PUPD0_Msk << (pin * 2)));
    tmp.AFR[afr] = (gpio->AFR[afr] & ~(GPIO_AFRL_AFSEL0_Msk << ((pin - (afr * 8)) * 4)));

    gpio->MODER |= (GPIO_MODER_MODE0_Msk << (pin * 2));
    gpio->OTYPER = (tmp.OTYPER | (ot << pin));
    gpio->OSPEEDR = (tmp.OSPEEDR | (ospeed << (pin * 2)));
    gpio->PUPDR = (tmp.PUPDR | (pupd << (pin * 2)));
    gpio->AFR[afr] = (tmp.AFR[afr] | (af << ((pin - (afr * 8)) * 4)));
    gpio->BSRR = ((value ? GPIO_BSRR_BS0 : GPIO_BSRR_BR0) << pin);
    gpio->MODER = (tmp.MODER | (mode << (pin * 2)));
}

uint32_t hw_gpio_get (gpio_id_t id) {
    GPIO_TypeDef *gpio = ((GPIO_TypeDef *) (gpios[(id >> 4) & 0x07]));
    uint8_t pin = (id & 0x0F);
    return gpio->IDR & (GPIO_IDR_ID0 << pin);
}

void hw_gpio_set (gpio_id_t id) {
    GPIO_TypeDef *gpio = ((GPIO_TypeDef *) (gpios[(id >> 4) & 0x07]));
    uint8_t pin = (id & 0x0F);
    gpio->BSRR = (GPIO_BSRR_BS0 << pin);
}

void hw_gpio_reset (gpio_id_t id) {
    GPIO_TypeDef *gpio = ((GPIO_TypeDef *) (gpios[(id >> 4) & 0x07]));
    uint8_t pin = (id & 0x0F);
    gpio->BSRR = (GPIO_BSRR_BR0 << pin);
}


#define UART_BAUD   (115200UL)

static void hw_uart_init (void) {
    RCC->APBENR2 |= (RCC_APBENR2_USART1EN | RCC_APBENR2_SYSCFGEN);

    SYSCFG->CFGR1 |= (SYSCFG_CFGR1_PA12_RMP | SYSCFG_CFGR1_PA11_RMP);

    hw_gpio_init(GPIO_ID_UART_TX, GPIO_ALT, GPIO_PP, GPIO_SPEED_LOW, GPIO_PULL_UP, GPIO_AF_1, 0);
    hw_gpio_init(GPIO_ID_UART_RX, GPIO_ALT, GPIO_PP, GPIO_SPEED_LOW, GPIO_PULL_UP, GPIO_AF_1, 0);

    USART1->BRR = (CPU_FREQ / UART_BAUD);
    USART1->RQR = (USART_RQR_TXFRQ | USART_RQR_RXFRQ);
    USART1->CR1 = (USART_CR1_FIFOEN | USART_CR1_M0 | USART_CR1_PCE | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void hw_uart_read (uint8_t *data, int length) {
    for (int i = 0; i < length; i++) {
        while (!(USART1->ISR & USART_ISR_RXNE_RXFNE));
        *data++ = (uint8_t) (USART1->RDR);
    }
}

void hw_uart_write (uint8_t *data, int length) {
    for (int i = 0; i < length; i++) {
        while (!(USART1->ISR & USART_ISR_TXE_TXFNF));
        USART1->TDR = *data++;
    }
}

void hw_uart_write_wait_busy (void) {
    while (!(USART1->ISR & USART_ISR_TC));
}


static void hw_spi_init (void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->APBENR2 |= RCC_APBENR2_SPI1EN;

    DMAMUX1_Channel0->CCR = (16 << DMAMUX_CxCR_DMAREQ_ID_Pos);
    DMAMUX1_Channel1->CCR = (17 << DMAMUX_CxCR_DMAREQ_ID_Pos);

    DMA1_Channel1->CPAR = (uint32_t) (&SPI1->DR);
    DMA1_Channel2->CPAR = (uint32_t) (&SPI1->DR);

    SPI1->CR2 = (
        SPI_CR2_FRXTH |
        (8 - 1) << SPI_CR2_DS_Pos |
        SPI_CR2_TXDMAEN |
        SPI_CR2_RXDMAEN
    );
    SPI1->CR1 = (
        SPI_CR1_SSM |
        SPI_CR1_SSI |
        SPI_CR1_BR_1 |
        SPI_CR1_SPE |
        SPI_CR1_MSTR |
        SPI_CR1_CPHA
    );

    hw_gpio_init(GPIO_ID_SPI_CS, GPIO_OUTPUT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 1);
    hw_gpio_init(GPIO_ID_SPI_CLK, GPIO_ALT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_SPI_MOSI, GPIO_ALT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_SPI_MISO, GPIO_ALT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_DOWN, GPIO_AF_0, 0);
}

void hw_spi_start (void) {
    hw_gpio_reset(GPIO_ID_SPI_CS);
}

void hw_spi_stop (void) {
    while (SPI1->SR & SPI_SR_BSY);
    hw_gpio_set(GPIO_ID_SPI_CS);
}

void hw_spi_rx (uint8_t *data, int length) {
    volatile uint8_t dummy = 0x00;

    DMA1_Channel1->CNDTR = length;
    DMA1_Channel2->CNDTR = length;

    DMA1_Channel1->CMAR = (uint32_t) (data);
    DMA1_Channel1->CCR = (DMA_CCR_MINC | DMA_CCR_EN);

    DMA1_Channel2->CMAR = (uint32_t) (&dummy);
    DMA1_Channel2->CCR = (DMA_CCR_DIR | DMA_CCR_EN);

    while (DMA1_Channel1->CNDTR || DMA1_Channel2->CNDTR);

    DMA1_Channel1->CCR = 0;
    DMA1_Channel2->CCR = 0;
}

void hw_spi_tx (uint8_t *data, int length) {
    volatile uint8_t dummy __attribute__((unused));

    DMA1_Channel1->CNDTR = length;
    DMA1_Channel2->CNDTR = length;

    DMA1_Channel1->CMAR = (uint32_t) (&dummy);
    DMA1_Channel1->CCR = DMA_CCR_EN;

    DMA1_Channel2->CMAR = (uint32_t) (data);
    DMA1_Channel2->CCR = (DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_EN);

    while (DMA1_Channel1->CNDTR || DMA1_Channel2->CNDTR);

    DMA1_Channel1->CCR = 0;
    DMA1_Channel2->CCR = 0;
}


#define I2C_TIMEOUT_US_BUSY     (10000)
#define I2C_TIMEOUT_US_PER_BYTE (1000)

static void hw_i2c_init (void) {
    RCC->APBENR1 |= RCC_APBENR1_I2C1EN;

    I2C1->CR1 = 0;

    hw_gpio_init(GPIO_ID_I2C_SCL, GPIO_ALT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_NONE, GPIO_AF_6, 0);
    hw_gpio_init(GPIO_ID_I2C_SDA, GPIO_ALT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_NONE, GPIO_AF_6, 0);

    while (!(hw_gpio_get(GPIO_ID_I2C_SCL) && hw_gpio_get(GPIO_ID_I2C_SDA)));

    I2C1->TIMINGR = 0x10901032UL;
    I2C1->CR1 |= I2C_CR1_PE;
}

i2c_err_t hw_i2c_trx (uint8_t address, uint8_t *tx_data, uint8_t tx_length, uint8_t *rx_data, uint8_t rx_length) {
    hw_timeout_start();

    while (I2C1->ISR & I2C_ISR_BUSY) {
        if (hw_timeout_occured(I2C_TIMEOUT_US_BUSY)) {
            return I2C_ERR_BUSY;
        }
    }

    if (tx_length > 0) {
        uint32_t tx_timeout = ((tx_length + 1) * I2C_TIMEOUT_US_PER_BYTE);

        hw_timeout_start();

        I2C1->ICR = I2C_ICR_NACKCF;
        I2C1->CR2 = (
            ((rx_length > 0) ? 0 : I2C_CR2_AUTOEND) |
            (tx_length << I2C_CR2_NBYTES_Pos) |
            I2C_CR2_START |
            (address << I2C_CR2_SADD_Pos)
        );

        uint8_t left = tx_length;

        while (left > 0) {
            uint32_t isr = I2C1->ISR;

            if (isr & I2C_ISR_TXIS) {
                I2C1->TXDR = *tx_data++;
                left -= 1;
            }

            if (isr & I2C_ISR_NACKF) {
                return I2C_ERR_NACK;
            }

            if (hw_timeout_occured(tx_timeout)) {
                return I2C_ERR_TIMEOUT;
            }
        }

        if (rx_length == 0) {
            return I2C_OK;
        }

        while (!(I2C1->ISR & I2C_ISR_TC)) {
            if (hw_timeout_occured(tx_timeout)) {
                return I2C_ERR_TIMEOUT;
            }
        }
    }

    if (rx_length > 0) {
        uint32_t rx_timeout = ((rx_length + 1) * I2C_TIMEOUT_US_PER_BYTE);

        hw_timeout_start();

        I2C1->CR2 = (
            I2C_CR2_AUTOEND |
            (rx_length << I2C_CR2_NBYTES_Pos) |
            I2C_CR2_START |
            I2C_CR2_RD_WRN |
            (address << I2C_CR2_SADD_Pos)
        );

        uint8_t left = rx_length;

        while (left > 0) {
            uint32_t isr = I2C1->ISR;

            if (isr & I2C_ISR_RXNE) {
                *rx_data++ = I2C1->RXDR;
                left -= 1;
            }

            if (hw_timeout_occured(rx_timeout)) {
                return I2C_ERR_TIMEOUT;
            }
        }
    }

    return I2C_OK;
}


static void hw_crc32_init (void) {
    RCC->AHBENR |= RCC_AHBENR_CRCEN;

    CRC->CR = (CRC_CR_REV_OUT | CRC_CR_REV_IN_0);
}

void hw_crc32_reset (void) {
    CRC->CR |= CRC_CR_RESET;
}

uint32_t hw_crc32_calculate (uint8_t *data, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        *(__IO uint8_t *) (&CRC->DR) = data[i];
    }
    return (CRC->DR ^ 0xFFFFFFFF);
}


uint32_t hw_flash_size (void) {
    return FLASH_SIZE;
}

hw_flash_t hw_flash_read (uint32_t offset) {
    return *(uint64_t *) (FLASH_BASE + offset);
}

static void hw_flash_unlock (void) {
    while (FLASH->SR & FLASH_SR_BSY1);
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123;
        __ISB();
        FLASH->KEYR = 0xCDEF89AB;
    }
}

void hw_flash_erase (void) {
    hw_flash_unlock();
    FLASH->CR |= (FLASH_CR_STRT | FLASH_CR_MER1);
    while (FLASH->SR & FLASH_SR_BSY1);
}

void hw_flash_program (uint32_t offset, hw_flash_t value) {
    hw_flash_unlock();
    FLASH->CR |= FLASH_CR_PG;
    *(__IO uint32_t *) (FLASH_BASE + offset) = ((value) & 0xFFFFFFFF);
    __ISB();
    *(__IO uint32_t *) (FLASH_BASE + offset + 4) = ((value >> 32) & 0xFFFFFFFF);
    while (FLASH->SR & FLASH_SR_BSY1);
    FLASH->CR &= ~(FLASH_CR_PG);
}


void hw_reset (loader_parameters_t *parameters) {
    if (parameters != NULL) {
        RCC->APBENR1 |= (RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN);
        PWR->CR1 |= PWR_CR1_DBP;
        TAMP->BKP0R = parameters->magic;
        TAMP->BKP1R = parameters->flags;
        TAMP->BKP2R = parameters->mcu_address;
        TAMP->BKP3R = parameters->fpga_address;
        TAMP->BKP4R = parameters->bootloader_address;
        PWR->CR1 &= ~(PWR_CR1_DBP);
        RCC->APBENR1 &= ~(RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN);
    }
    NVIC_SystemReset();
}


void hw_loader_get_parameters (loader_parameters_t *parameters) {
    RCC->APBENR1 |= (RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN);
    parameters->magic = TAMP->BKP0R;
    parameters->flags = TAMP->BKP1R;
    parameters->mcu_address = TAMP->BKP2R;
    parameters->fpga_address = TAMP->BKP3R;
    parameters->bootloader_address = TAMP->BKP4R;
    PWR->CR1 |= PWR_CR1_DBP;
    TAMP->BKP0R = 0;
    TAMP->BKP1R = 0;
    TAMP->BKP2R = 0;
    TAMP->BKP3R = 0;
    TAMP->BKP4R = 0;
    PWR->CR1 &= ~(PWR_CR1_DBP);
    RCC->APBENR1 &= ~(RCC_APBENR1_PWREN | RCC_APBENR1_RTCAPBEN);
}


static void hw_led_init (void) {
    hw_gpio_init(GPIO_ID_LED, GPIO_OUTPUT, GPIO_PP, GPIO_SPEED_VLOW, GPIO_PULL_NONE, GPIO_AF_0, 0);
}


static void hw_misc_init (void) {
    hw_gpio_init(GPIO_ID_N64_RESET, GPIO_INPUT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_DOWN, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_N64_CIC_CLK, GPIO_INPUT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_N64_CIC_DQ, GPIO_INPUT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 1);
    hw_gpio_init(GPIO_ID_FPGA_INT, GPIO_INPUT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_RTC_MFP, GPIO_INPUT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 0);
}


void hw_primer_init (void) {
    hw_clock_init();
    hw_timeout_init();
    hw_delay_init();
    hw_led_init();
    hw_uart_init();
    hw_spi_init();
    hw_i2c_init();
    hw_crc32_init();
}

void hw_loader_init (void) {
    hw_clock_init();
    hw_timeout_init();
    hw_delay_init();
    hw_led_init();
    hw_spi_init();
}

void hw_app_init (void) {
    hw_clock_init();
    hw_timeout_init();
    hw_delay_init();
    hw_led_init();
    hw_misc_init();
    hw_uart_init();
    hw_spi_init();
    hw_i2c_init();
    hw_crc32_init();
}

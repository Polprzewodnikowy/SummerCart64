#include <stm32g0xx.h>
#include "hw.h"


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

typedef struct {
    void (*volatile falling)(void);
    void (*volatile rising)(void);
} gpio_irq_callback_t;


static const GPIO_TypeDef *gpios[] = { GPIOA, GPIOB };
static gpio_irq_callback_t gpio_irq_callbacks[16];

static volatile uint8_t *i2c_data_txptr;
static volatile uint8_t *i2c_data_rxptr;
static volatile uint32_t i2c_next_cr2;
static void (*volatile i2c_callback)(void);

static const TIM_TypeDef *tims[] = { TIM14, TIM16, TIM17 };
static void (*volatile tim_callbacks[3])(void);


void hw_gpio_init (gpio_id_t id, gpio_mode_t mode, gpio_ot_t ot, gpio_ospeed_t ospeed, gpio_pupd_t pupd, gpio_af_t af, int value) {
    GPIO_TypeDef tmp;
    GPIO_TypeDef *gpio = ((GPIO_TypeDef *) (gpios[(id >> 4) & 0x07]));
    uint8_t pin = (id & 0x0F);
    uint8_t afr = ((pin < 8) ? 0 : 1);

    if (!gpio) {
        return;
    }

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

void hw_gpio_irq_setup (gpio_id_t id, gpio_irq_t irq, void (*callback)(void)) {
    uint8_t port = ((id >> 4) & 0x0F);
    uint8_t pin = (id & 0x0F);
    if (irq == GPIO_IRQ_FALLING) {
        EXTI->FTSR1 |= (EXTI_FTSR1_FT0 << pin);
        gpio_irq_callbacks[pin].falling = callback;
    } else {
        EXTI->RTSR1 |= (EXTI_RTSR1_RT0 << pin);
        gpio_irq_callbacks[pin].rising = callback;
    }
    EXTI->EXTICR[pin / 4] |= (port << (8 * (pin % 4)));
    EXTI->IMR1 |= (EXTI_IMR1_IM0 << pin);
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

void hw_spi_start (void) {
    hw_gpio_reset(GPIO_ID_SPI_CS);
}

void hw_spi_stop (void) {
    while (SPI1->SR & SPI_SR_BSY);
    hw_gpio_set(GPIO_ID_SPI_CS);
}

void hw_spi_trx (uint8_t *data, int length, spi_direction_t direction) {
    volatile uint8_t dummy __attribute__((unused));

    DMA1_Channel1->CNDTR = length;
    DMA1_Channel2->CNDTR = length;

    if (direction == SPI_TX) {
        DMA1_Channel1->CMAR = (uint32_t) (&dummy);
        DMA1_Channel1->CCR = DMA_CCR_EN;

        DMA1_Channel2->CMAR = (uint32_t) (data);
        DMA1_Channel2->CCR = (DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_EN);
    } else {
        DMA1_Channel1->CMAR = (uint32_t) (data);
        DMA1_Channel1->CCR = (DMA_CCR_MINC | DMA_CCR_EN);

        DMA1_Channel2->CMAR = (uint32_t) (&dummy);
        DMA1_Channel2->CCR = (DMA_CCR_DIR | DMA_CCR_EN);
    }

    while (DMA1_Channel1->CNDTR || DMA1_Channel2->CNDTR);

    DMA1_Channel1->CCR = 0;    
    DMA1_Channel2->CCR = 0;
}

void hw_i2c_read (uint8_t i2c_address, uint8_t address, uint8_t *data, uint8_t length, void (*callback)(void)) {
    i2c_data_rxptr = data;
    i2c_callback = callback;
    I2C1->TXDR = address;
    i2c_next_cr2 = (
        I2C_CR2_AUTOEND |
        (length << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_START |
        I2C_CR2_RD_WRN |
        (i2c_address << I2C_CR2_SADD_Pos)
    );
    I2C1->CR2 = (
        (1 << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_START |
        (i2c_address << I2C_CR2_SADD_Pos)
    );
}

void hw_i2c_write (uint8_t i2c_address, uint8_t address, uint8_t *data, uint8_t length, void (*callback)(void)) {
    i2c_data_txptr = data;
    i2c_callback = callback;
    I2C1->TXDR = address;
    I2C1->CR2 = (
        I2C_CR2_AUTOEND |
        ((length + 1) << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_START |
        (i2c_address << I2C_CR2_SADD_Pos)
    );
}

uint32_t hw_i2c_get_error (void) {
    return I2C1->ISR & I2C_ISR_NACKF;
}

void hw_i2c_disable_irq (void) {
    NVIC_DisableIRQ(I2C1_IRQn);
}

void hw_i2c_enable_irq (void) {
    NVIC_EnableIRQ(I2C1_IRQn);
}

void hw_tim_setup (tim_id_t id, uint16_t delay, void (*callback)(void)) {
    TIM_TypeDef *tim = ((TIM_TypeDef *) (tims[id]));
    tim->CR1 = (TIM_CR1_OPM | TIM_CR1_URS);
    tim->PSC = (64000 - 1);
    tim->ARR = (delay - 1);
    tim->DIER = TIM_DIER_UIE;
    tim->EGR = TIM_EGR_UG;
    tim->SR = 0;
    tim->CR1 |= TIM_CR1_CEN;
    tim_callbacks[id] = callback;
}

void hw_tim_disable_irq (tim_id_t id) {
    switch (id) {
        case TIM_ID_CIC:
            NVIC_DisableIRQ(TIM14_IRQn);
            break;
        case TIM_ID_RTC:
            NVIC_DisableIRQ(TIM16_IRQn);
            break;
        case TIM_ID_GVR:
            NVIC_DisableIRQ(TIM17_IRQn);
            break;
        default:
            break;
    }
}

void hw_tim_enable_irq (tim_id_t id) {
    switch (id) {
        case TIM_ID_CIC:
            NVIC_EnableIRQ(TIM14_IRQn);
            break;
        case TIM_ID_RTC:
            NVIC_EnableIRQ(TIM16_IRQn);
            break;
        case TIM_ID_GVR:
            NVIC_EnableIRQ(TIM17_IRQn);
            break;
        default:
            break;
    }
}

void hw_tim_stop (tim_id_t id) {
    TIM_TypeDef *tim = ((TIM_TypeDef *) (tims[id]));
    tim->CR1 &= ~(TIM_CR1_CEN);
    tim_callbacks[id] = 0;
}

void hw_init (void) {
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

    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->APBENR1 |= (
        RCC_APBENR1_DBGEN |
        RCC_APBENR1_I2C1EN
    );
    RCC->APBENR2 |= (
        RCC_APBENR2_TIM17EN |
        RCC_APBENR2_TIM16EN |
        RCC_APBENR2_TIM14EN |
        RCC_APBENR2_USART1EN |
        RCC_APBENR2_SPI1EN |
        RCC_APBENR2_SYSCFGEN
    );

    DBG->APBFZ2 = (
        DBG_APB_FZ2_DBG_TIM17_STOP |
        DBG_APB_FZ2_DBG_TIM16_STOP |
        DBG_APB_FZ2_DBG_TIM14_STOP
    );

    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN;

    DMAMUX1_Channel0->CCR = (16 << DMAMUX_CxCR_DMAREQ_ID_Pos);
    DMAMUX1_Channel1->CCR = (17 << DMAMUX_CxCR_DMAREQ_ID_Pos);

    DMA1_Channel1->CPAR = (uint32_t) (&SPI1->DR);
    DMA1_Channel2->CPAR = (uint32_t) (&SPI1->DR);

    SYSCFG->CFGR1 |= (SYSCFG_CFGR1_PA12_RMP | SYSCFG_CFGR1_PA11_RMP);

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

    USART1->BRR = (64000000UL) / 1000000;
    USART1->CR1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_UE;

    I2C1->TIMINGR = 0x10B17DB5UL;
    I2C1->CR1 |= (I2C_CR1_TCIE | I2C_CR1_STOPIE | I2C_CR1_RXIE | I2C_CR1_TXIE | I2C_CR1_PE);

    hw_gpio_init(GPIO_ID_N64_RESET, GPIO_INPUT, GPIO_PP, GPIO_SPEED_VLOW, GPIO_PULL_DOWN, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_N64_CIC_CLK, GPIO_INPUT, GPIO_PP, GPIO_SPEED_VLOW, GPIO_PULL_DOWN, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_N64_CIC_DQ, GPIO_OUTPUT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 1);

    hw_gpio_init(GPIO_ID_LED, GPIO_OUTPUT, GPIO_PP, GPIO_SPEED_VLOW, GPIO_PULL_NONE, GPIO_AF_0, 0);

    hw_gpio_init(GPIO_ID_SPI_CS, GPIO_OUTPUT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 1);
    hw_gpio_init(GPIO_ID_SPI_CLK, GPIO_ALT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_SPI_MISO, GPIO_ALT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 0);
    hw_gpio_init(GPIO_ID_SPI_MOSI, GPIO_ALT, GPIO_PP, GPIO_SPEED_HIGH, GPIO_PULL_NONE, GPIO_AF_0, 0);

    hw_gpio_init(GPIO_ID_FPGA_INT, GPIO_INPUT, GPIO_PP, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 0);

    hw_gpio_init(GPIO_ID_UART_TX, GPIO_ALT, GPIO_PP, GPIO_SPEED_LOW, GPIO_PULL_NONE, GPIO_AF_1, 0);
    hw_gpio_init(GPIO_ID_UART_RX, GPIO_ALT, GPIO_PP, GPIO_SPEED_LOW, GPIO_PULL_NONE, GPIO_AF_1, 0);

    hw_gpio_init(GPIO_ID_I2C_SCL, GPIO_ALT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_NONE, GPIO_AF_6, 0);
    hw_gpio_init(GPIO_ID_I2C_SDA, GPIO_ALT, GPIO_OD, GPIO_SPEED_VLOW, GPIO_PULL_NONE, GPIO_AF_6, 0);

    hw_gpio_init(GPIO_ID_RTC_MFP, GPIO_INPUT, GPIO_PP, GPIO_SPEED_VLOW, GPIO_PULL_UP, GPIO_AF_0, 0);

    NVIC_SetPriority(EXTI0_1_IRQn, 0);
    NVIC_SetPriority(EXTI2_3_IRQn, 1);
    NVIC_SetPriority(EXTI4_15_IRQn, 2);
    NVIC_SetPriority(I2C1_IRQn, 1);
    NVIC_SetPriority(TIM14_IRQn, 0);
    NVIC_SetPriority(TIM16_IRQn, 1);
    NVIC_SetPriority(TIM17_IRQn, 2);

    NVIC_EnableIRQ(EXTI0_1_IRQn);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
    NVIC_EnableIRQ(I2C1_IRQn);
    NVIC_EnableIRQ(TIM14_IRQn);
    NVIC_EnableIRQ(TIM16_IRQn);
    NVIC_EnableIRQ(TIM17_IRQn);
}

void EXTI0_1_IRQHandler (void) {
    for (int i = 0; i <= 1; i++) {
        if (EXTI->FPR1 & (EXTI_FPR1_FPIF0 << i)) {
            EXTI->FPR1 = (EXTI_FPR1_FPIF0 << i);
            if (gpio_irq_callbacks[i].falling) {
                gpio_irq_callbacks[i].falling();
            }
        }
        if (EXTI->RPR1 & (EXTI_RPR1_RPIF0 << i)) {
            EXTI->RPR1 = (EXTI_RPR1_RPIF0 << i);
            if (gpio_irq_callbacks[i].rising) {
                gpio_irq_callbacks[i].rising();
            }
        }
    }
}

void EXTI2_3_IRQHandler (void) {
    for (int i = 2; i <= 3; i++) {
        if (EXTI->FPR1 & (EXTI_FPR1_FPIF0 << i)) {
            EXTI->FPR1 = (EXTI_FPR1_FPIF0 << i);
            if (gpio_irq_callbacks[i].falling) {
                gpio_irq_callbacks[i].falling();
            }
        }
        if (EXTI->RPR1 & (EXTI_RPR1_RPIF0 << i)) {
            EXTI->RPR1 = (EXTI_RPR1_RPIF0 << i);
            if (gpio_irq_callbacks[i].rising) {
                gpio_irq_callbacks[i].rising();
            }
        }
    }
}

void EXTI4_15_IRQHandler (void) {
    for (int i = 4; i <= 15; i++) {
        if (EXTI->FPR1 & (EXTI_FPR1_FPIF0 << i)) {
            EXTI->FPR1 = (EXTI_FPR1_FPIF0 << i);
            if (gpio_irq_callbacks[i].falling) {
                gpio_irq_callbacks[i].falling();
            }
        }
        if (EXTI->RPR1 & (EXTI_RPR1_RPIF0 << i)) {
            EXTI->RPR1 = (EXTI_RPR1_RPIF0 << i);
            if (gpio_irq_callbacks[i].rising) {
                gpio_irq_callbacks[i].rising();
            }
        }
    }
}

void I2C1_IRQHandler (void) {
    if (I2C1->ISR & I2C_ISR_TXIS) {
        I2C1->TXDR = *i2c_data_txptr++;
    }

    if (I2C1->ISR & I2C_ISR_RXNE) {
        *i2c_data_rxptr++ = I2C1->RXDR;
    }

    if (I2C1->ISR & I2C_ISR_TC) {
        I2C1->CR2 = i2c_next_cr2;
    }

    if (I2C1->ISR & I2C_ISR_STOPF) {
        I2C1->ICR = I2C_ICR_STOPCF;
        if (i2c_callback) {
            i2c_callback();
            i2c_callback = 0;
        }
    }
}

void TIM14_IRQHandler (void) {
    TIM14->SR &= ~(TIM_SR_UIF);
    if (tim_callbacks[0]) {
        tim_callbacks[0]();
        tim_callbacks[0] = 0;
    }
}

void TIM16_IRQHandler (void) {
    TIM16->SR &= ~(TIM_SR_UIF);
    if (tim_callbacks[1]) {
        tim_callbacks[1]();
        tim_callbacks[1] = 0;
    }
}

void TIM17_IRQHandler (void) {
    TIM17->SR &= ~(TIM_SR_UIF);
    if (tim_callbacks[2]) {
        tim_callbacks[2]();
        tim_callbacks[2] = 0;
    }
}

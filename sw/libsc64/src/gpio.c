#include "gpio.h"
#include "io_dma.h"
#include "registers.h"


static uint32_t gpio_state;


void sc64_gpio_init(void) {
    gpio_state = 0;

    sc64_io_write(&SC64_CART->GPIO, gpio_state);    
}

void sc64_gpio_mode_set(uint8_t num, sc64_gpio_mode_t mode) {
    if (num >= 8) {
        return;
    }

    gpio_state &= ~(
        ((1 << num) << SC64_CART_GPIO_OFFSET_OPEN_DRAIN) |
        ((1 << num) << SC64_CART_GPIO_OFFSET_DIR)
    );

    switch (mode) {
        case MODE_OUTPUT:
            gpio_state |= ((1 << num) << SC64_CART_GPIO_OFFSET_DIR);
            break;
        case MODE_OPEN_DRAIN:
            gpio_state |= (
                ((1 << num) << SC64_CART_GPIO_OFFSET_OPEN_DRAIN) |
                ((1 << num) << SC64_CART_GPIO_OFFSET_DIR)
            );
            break;
        default:
            break;
    }

    sc64_io_write(&SC64_CART->GPIO, gpio_state);
}

void sc64_gpio_output_write(uint8_t value) {
    gpio_state &= ~(0xFF << SC64_CART_GPIO_OFFSET_OUTPUT);
    gpio_state |= (value << SC64_CART_GPIO_OFFSET_OUTPUT);

    sc64_io_write(&SC64_CART->GPIO, gpio_state);
}

void sc64_gpio_output_set(uint8_t mask) {
    gpio_state |= (mask << SC64_CART_GPIO_OFFSET_OUTPUT);

    sc64_io_write(&SC64_CART->GPIO, gpio_state);
}

void sc64_gpio_output_clear(uint8_t mask) {
    gpio_state &= ~(mask << SC64_CART_GPIO_OFFSET_OUTPUT);

    sc64_io_write(&SC64_CART->GPIO, gpio_state);
}

uint8_t sc64_gpio_input_get(void) {
    return (uint8_t) ((sc64_io_read(&SC64_CART->GPIO) >> SC64_CART_GPIO_OFFSET_INPUT) & 0xFF);
}

#ifndef SC64_GPIO_H__
#define SC64_GPIO_H__


#include <stdint.h>


typedef enum sc64_gpio_mode_e {
    MODE_INPUT,
    MODE_OUTPUT,
    MODE_OPEN_DRAIN,
} sc64_gpio_mode_t;


void sc64_gpio_init(void);

void sc64_gpio_mode_set(uint8_t num, sc64_gpio_mode_t mode);

void sc64_gpio_output_write(uint8_t value);
void sc64_gpio_output_set(uint8_t mask);
void sc64_gpio_output_clear(uint8_t mask);

uint8_t sc64_gpio_input_get(void);


#endif

#include "gpio.h"
#include "io_dma.h"


void sc64_init(void) {
    sc64_io_dma_init();
    sc64_gpio_init();
}

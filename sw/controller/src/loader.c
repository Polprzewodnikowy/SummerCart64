#include <stdint.h>
#include <stm32g030xx.h>
#include "hw.h"
#include "update.h"


void no_valid_image (void) {
    hw_loader_init();
    hw_gpio_set(GPIO_ID_LED);
}

void loader (void) {
    if (update_check()) {
        hw_loader_init();
        update_perform();
    }
}

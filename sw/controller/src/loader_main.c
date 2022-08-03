#include <stdint.h>
#include "hw.h"
#include "update.h"


void loader_main (void) {
    uint32_t parameters[5];

    hw_loader_get_parameters(parameters);

    if (parameters[0] == HW_UPDATE_START_MAGIC) {
        hw_loader_clear_parameters();
        hw_loader_init();

        hw_gpio_set(GPIO_ID_LED);

        update_perform(parameters);

        hw_gpio_reset(GPIO_ID_LED);

        parameters[0] = HW_UPDATE_DONE_MAGIC;
        hw_loader_reset(parameters);
    }
}

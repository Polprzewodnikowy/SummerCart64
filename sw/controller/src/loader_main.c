#include <stdint.h>
#include "fpga.h"
#include "hw.h"
#include "vendor.h"


void loader_main (void) {
    uint32_t parameters[5];
    uint64_t buffer;

    hw_loader_get_parameters(parameters);

    if (parameters[0] == HW_UPDATE_START_MAGIC) {
        hw_loader_clear_parameters();
        hw_loader_init();

        hw_gpio_set(GPIO_ID_LED);

        if (parameters[2] != 0) {
            hw_flash_erase();
            for (int i = 0; i < parameters[2]; i += sizeof(buffer)) {
                fpga_mem_read(parameters[1] + i, sizeof(buffer), (uint8_t *) (&buffer));
                hw_flash_program(HW_FLASH_ADDRESS + i, buffer);
            }
        }

        if (parameters[4] != 0) {
            vendor_update(parameters[3], parameters[4]);
            vendor_reconfigure();
        }

        hw_gpio_reset(GPIO_ID_LED);

        parameters[0] = HW_UPDATE_DONE_MAGIC;
        hw_loader_reset(parameters);
    }
}

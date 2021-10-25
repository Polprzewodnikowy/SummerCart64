#include <stdint.h>
#include "sys.h"


__attribute__ ((naked, section(".bootloader"))) void reset_handler (void) {
    io32_t *ram = (io32_t *) &RAM;
    io32_t *flash = (io32_t *) (FLASH_BASE + FLASH_IMAGE_OFFSET);

    for (int i = 0; i < RAM_SIZE; i += 4) {
        *ram++ = *flash++;
    }

    __asm__ volatile (
        "la t0, app_handler \n"
        "jalr zero, t0 \n"
    );
}


__attribute__ ((naked)) void app_handler (void) {
    __asm__ volatile (
        "la sp, __stack_pointer \n"
        "la gp, __global_pointer \n"
        "jal zero, main \n"
    );
}

#include <stdint.h>
#include "sys.h"


#define BOOT_UART


__attribute__ ((naked, section(".bootloader"))) int reset_handler (void) {
    register uint32_t length = 0;

#if defined(BOOT_UART)
    volatile uint8_t *pointer = (volatile uint8_t *) &RAM;
    for (int i = 0; i < 4; i++) {
        while (!(UART->SCR & UART_SCR_RXNE));
        length |= (UART->DR << (i * 8));
    }
#elif defined(BOOT_N64)
    volatile uint32_t *pointer = (volatile uint32_t *) &RAM;
    while (!(CFG->SCR & CFG_SCR_CPU_BUSY));
    length = CFG->DATA[0];
    CFG->SCR &= ~(CFG_SCR_CPU_READY);
#endif

    while (1) {
#if defined(BOOT_UART)
        while (!(UART->SCR & UART_SCR_RXNE));
        *pointer++ = UART->DR;
        length = length - 1;
#elif defined(BOOT_N64)
        while (!(CFG->SCR & CFG_SCR_CPU_BUSY));
        *pointer++ = CFG->DATA[0];
        CFG->SCR &= ~(CFG_SCR_CPU_READY);
        length = length - 4;
#endif
        if (length == 0) {
            __asm__ volatile (
                "la t0, app_handler\n"
                "jalr zero, t0\n"
            );
        }
    }
}


__attribute__ ((naked)) void app_handler (void) {
    __asm__ volatile (
        "la sp, __stack_pointer\n"
        "la gp, __global_pointer\n"
        "jal zero, main\n"
    );
}

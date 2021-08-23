#include "sys.h"

int reset_handler (void) {
#ifdef BOOT_UART
    io8_t pointer = &RAM;
#else
#ifdef BOOT_N64
    io32_t pointer = &RAM;
#endif
#endif
    uint32_t length = 0;

#ifdef BOOT_UART
    for (int i = 0; i < 4; i++) {
        while (!(UART_SR & UART_SR_RXNE));
        length |= (UART_DR << (i * 8));
    }
#else
#ifdef BOOT_N64
    while (!(CFG_SCR & CFG_SCR_BOOTSTRAP_PENDING));
    length = CFG_BOOTSTRAP;
#endif
#endif

    while (1) {
#ifdef BOOT_UART
        while (!(UART_SR & UART_SR_RXNE));
        *pointer++ = UART_DR;
#else
#ifdef BOOT_N64
        while (!(CFG_SCR & CFG_SCR_BOOTSTRAP_PENDING));
        *pointer++ = CFG_BOOTSTRAP;
#endif
#endif
        if (((uint32_t) pointer) == length) {
            CFG_SCR |= CFG_SCR_CPU_BOOTSTRAPPED;
            __asm__("call 0");
        }
    }
}

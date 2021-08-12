#include "btldr.h"

int reset_handler (void) {
    io8_t pointer = &RAM;

    while (!(UART_SR & UART_SR_TXE));
    UART_TX = '>';

    while (1) {
        if (UART_SR & UART_SR_RXNE) {
            *pointer++ = UART_RX;
        }
        if ((uint32_t)pointer == (24 * 1024)) {
            __asm__("call 0");
        }
    }
}

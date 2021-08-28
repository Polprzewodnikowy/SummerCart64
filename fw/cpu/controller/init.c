#include "sys.h"


void init (void) {
    DMA->SCR = DMA_SCR_STOP;
    USB->SCR = USB_SCR_FLUSH_TX | USB_SCR_FLUSH_TX;

    GPIO->ODR = 0;
    GPIO->OER = (1 << 0);

    CFG->SCR = CFG_SCR_SDRAM_SWITCH;
    CFG->DD_OFFSET = DEFAULT_DD_OFFSET;
    CFG->SAVE_OFFSET = DEFAULT_SAVE_OFFSET;

    while (!(UART->SCR & UART_SCR_TXE));
    UART->DR = '$';
}

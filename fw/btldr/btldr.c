#include "btldr.h"

int reset_handler (void) {
    io8_t pointer = &RAM;
    uint32_t length = 0;

    while (!(USB_SR & USB_SR_TXE));
    USB_DR = '>';

    for (int i = 0; i < 4; i++) {
        while (!(USB_SR & USB_SR_RXNE));
        length |= (USB_DR << (i * 8));
    }

    while (1) {
        while (!(USB_SR & USB_SR_RXNE));
        *pointer++ = USB_DR;
        if ((uint32_t)pointer == length) {
            __asm__("call 0");
        }
    }
}

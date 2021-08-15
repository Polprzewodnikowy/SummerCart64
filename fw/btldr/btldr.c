#include "btldr.h"

int reset_handler (void) {
    io8_t pointer = &RAM;

    while (!(USB_SR & USB_SR_TXE));
    USB_DR = '>';

    while (1) {
        if (USB_SR & USB_SR_RXNE) {
            *pointer++ = USB_DR;
        }
        if ((uint32_t)pointer == (24 * 1024)) {
            __asm__("call 0");
        }
    }
}

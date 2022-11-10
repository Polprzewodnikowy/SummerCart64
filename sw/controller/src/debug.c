#include <string.h>
#include "debug.h"
#include "hw.h"


static char hex_chars[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
};


void debug_print_text (char *text) {
    hw_uart_write((uint8_t *) (text), strlen(text));
}

void debug_print_8bit (uint8_t value) {
    char buffer[3] = {
        hex_chars[(value >> 4) & 0x0F],
        hex_chars[value & 0x0F],
        '\0'
    };
    debug_print_text(buffer);
}

void debug_print_16bit (uint16_t value) {
    debug_print_8bit((value >> 8) & 0xFF);
    debug_print_8bit(value & 0xFF);
}

void debug_print_32bit (uint32_t value) {
    debug_print_8bit((value >> 24) & 0xFF);
    debug_print_8bit((value >> 16) & 0xFF);
    debug_print_8bit((value >> 8) & 0xFF);
    debug_print_8bit(value & 0xFF);
}

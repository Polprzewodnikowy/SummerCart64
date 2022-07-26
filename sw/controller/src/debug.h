#ifndef DEBUG_H__
#define DEBUG_H__


#include <stdint.h>


void debug_print_text (char *text);
void debug_print_8bit (uint8_t value);
void debug_print_16bit (uint16_t value);
void debug_print_32bit (uint32_t value);


#endif

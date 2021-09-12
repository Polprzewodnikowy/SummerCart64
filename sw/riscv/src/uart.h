#ifndef UART_H__
#define UART_H__


#include "sys.h"


void uart_print (const char *text);
void uart_print_02hex (uint8_t number);
void uart_print_08hex (uint32_t number);
void uart_init (void);
void process_uart (void);


#endif

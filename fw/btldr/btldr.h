#ifndef BTLDR_H__
#define BTLDR_H__


#include <stdint.h>


typedef volatile uint8_t *      io8_t;
typedef volatile uint32_t *     io32_t;

#define RAM                     (*((io8_t)  0x00000000))
#define UART_SR                 (*((io8_t)  0xB0000000))
#define UART_RX                 (*((io8_t)  0xB0000004))
#define UART_TX                 (*((io8_t)  0xB0000008))
#define GPIO                    (*((io32_t) 0xE0000000))
#define GPIO_O                  (*((io8_t)  0xE0000000))
#define GPIO_I                  (*((io8_t)  0xE0000001))
#define GPIO_OE                 (*((io8_t)  0xE0000002))

#define UART_SR_RXNE            (1 << 0)
#define UART_SR_TXE             (1 << 1)


#endif

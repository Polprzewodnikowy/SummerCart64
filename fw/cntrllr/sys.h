#ifndef SYS_H__
#define SYS_H__


#include <stddef.h>
#include <stdint.h>


#define __NAKED__               __attribute__((naked))

typedef volatile uint8_t *      io8_t;
typedef volatile uint32_t *     io32_t;

#define I2C_SR                  (*((io8_t)  0xC0000000))
#define I2C_DR                  (*((io8_t)  0xC0000004))
#define UART_SR                 (*((io8_t)  0xD0000000))
#define UART_RX                 (*((io8_t)  0xD0000004))
#define UART_TX                 (*((io8_t)  0xD0000008))
#define GPIO                    (*((io32_t) 0xE0000000))
#define GPIO_O                  (*((io8_t)  0xE0000000))
#define GPIO_I                  (*((io8_t)  0xE0000001))
#define GPIO_OE                 (*((io8_t)  0xE0000002))

#define I2C_SR_START            (1 << 0)
#define I2C_SR_STOP             (1 << 1)
#define I2C_SR_MACK             (1 << 2)
#define I2C_SR_ACK              (1 << 3)
#define I2C_SR_BUSY             (1 << 4)
#define I2C_ADDR_READ           (1 << 0)

#define UART_SR_RXNE            (1 << 0)
#define UART_SR_TXE             (1 << 1)


#endif

#ifndef BTLDR_H__
#define BTLDR_H__


#include <stdint.h>


typedef volatile uint8_t *      io8_t;
typedef volatile uint32_t *     io32_t;

#define RAM                     (*((io8_t)  0x00000000))
#define USB_SR                  (*((io8_t)  0x40000000))
#define USB_DR                  (*((io8_t)  0x40000004))

#define USB_SR_RXNE             (1 << 0)
#define USB_SR_TXE              (1 << 1)


#endif

#ifndef BTLDR_H__
#define BTLDR_H__


#include <stdint.h>


#define BOOT_UART
// #define BOOT_N64

typedef volatile uint8_t *          io8_t;
typedef volatile uint32_t *         io32_t;

#ifdef BOOT_UART
#define RAM                         (*((io8_t)  0x00000000))
#else
#ifdef BOOT_N64
#define RAM                         (*((io32_t) 0x00000000))
#endif
#endif

#define UART_SR                     (*((io8_t)  0x50000000))
#define UART_DR                     (*((io8_t)  0x50000004))

#define UART_SR_RXNE                (1 << 0)
#define UART_SR_TXE                 (1 << 1)

#define CFG_SCR                     (*((io32_t) 0x70000000))
#define CFG_BOOTSTRAP               (*((io32_t) 0x7000001C))

#define CFG_SCR_CPU_BOOTSTRAPPED    (1 << 31)
#define CFG_SCR_BOOTSTRAP_PENDING   (1 << 29)


#endif

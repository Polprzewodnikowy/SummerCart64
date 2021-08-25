#ifndef SYS_H__
#define SYS_H__


#include <stddef.h>
#include <stdint.h>


#define __NAKED__                   __attribute__((naked))

typedef volatile uint8_t *          io8_t;
typedef volatile uint32_t *         io32_t;

#define RAM                         (*((io32_t) 0x00000000))


#define BOOTLOADER                  (*((io32_t) 0x10000000))


#define GPIO                        (*((io32_t) 0x20000000))
#define GPIO_O                      (*((io8_t)  0x20000000))
#define GPIO_I                      (*((io8_t)  0x20000001))
#define GPIO_OE                     (*((io8_t)  0x20000002))


#define I2C_SCR                     (*((io8_t)  0x30000000))
#define I2C_DR                      (*((io8_t)  0x30000004))

#define I2C_SCR_START               (1 << 0)
#define I2C_SCR_STOP                (1 << 1)
#define I2C_SCR_MACK                (1 << 2)
#define I2C_SCR_ACK                 (1 << 3)
#define I2C_SCR_BUSY                (1 << 4)
#define I2C_ADDR_READ               (1 << 0)


#define USB_SCR                     (*((io8_t)  0x40000000))
#define USB_DR                      (*((io8_t)  0x40000004))

#define USB_SCR_RXNE                (1 << 0)
#define USB_SCR_TXE                 (1 << 1)
#define USB_SCR_FLUSH_RX            (1 << 2)
#define USB_SCR_FLUSH_TX            (1 << 3)


#define UART_SCR                    (*((io8_t)  0x50000000))
#define UART_DR                     (*((io8_t)  0x50000004))

#define UART_SCR_RXNE               (1 << 0)
#define UART_SCR_TXE                (1 << 1)


#define DMA_SCR                     (*((io8_t)  0x60000000))
#define DMA_MADDR                   (*((io32_t) 0x60000004))
#define DMA_ID_LEN                  (*((io32_t) 0x60000008))

#define DMA_SCR_START               (1 << 0)
#define DMA_SCR_STOP                (1 << 1)
#define DMA_SCR_DIR                 (1 << 2)
#define DMA_SCR_BUSY                (1 << 3)


#define SDRAM                       (*((io32_t) 0x68000000))


#define CFG_SCR                     (*((io32_t) 0x70000000))
#define CFG_DD_OFFSET               (*((io32_t) 0x70000004))
#define CFG_SAVE_OFFSET             (*((io32_t) 0x70000008))
#define CFG_COMMAND                 (*((io8_t)  0x7000000C))
#define CFG_ARG_1                   (*((io32_t) 0x70000010))
#define CFG_ARG_2                   (*((io32_t) 0x70000014))
#define CFG_RESPONSE                (*((io32_t) 0x70000018))

#define CFG_SCR_SDRAM_SWITCH        (1 << 0)
#define CFG_SCR_SDRAM_WRITABLE      (1 << 1)
#define CFG_SCR_DD_EN               (1 << 2)
#define CFG_SCR_SRAM_EN             (1 << 3)
#define CFG_SCR_FLASHRAM_EN         (1 << 4)
#define CFG_SCR_CPU_BUSY            (1 << 30)
#define CFG_SCR_CPU_RUNNING         (1 << 31)


#endif
#ifndef SC64_REGISTERS_H__
#define SC64_REGISTERS_H__


#include <stdint.h>


// Bank definitions

#define SC64_BANK_SDRAM                     (1)
#define SC64_BANK_EEPROM                    (3)


// Cart Interface Registers

typedef struct sc64_cart_registers {
    volatile uint32_t SCR;                  // Cart status and control
    volatile uint32_t BOOT;                 // Boot behavior control
    volatile uint32_t VERSION;              // Cart firmware version
    volatile uint32_t GPIO;                 // GPIO control
    volatile uint32_t USB_SCR;              // USB interface status and control
    volatile uint32_t USB_DMA_ADDR;         // USB address for DMA to PC
    volatile uint32_t USB_DMA_LEN;          // USB transfer length for DMA to PC
    volatile uint32_t DDIPL_ADDR;           // 64 Disk Drive IPL location in SDRAM
    volatile uint32_t SAVE_ADDR;            // SRAM/FlashRAM save emulation location in SDRAM
    volatile uint32_t _unused[1015];
    volatile uint32_t USB_FIFO[1024];       // USB data from PC read FIFO memory
} sc64_cart_registers_t;


#define SC64_CART_BASE                      (0x1E000000)

#define SC64_CART                           ((volatile sc64_cart_registers_t *) SC64_CART_BASE)


// SCR
#define SC64_CART_SCR_SKIP_BOOTLOADER       (1 << 10)
#define SC64_CART_SCR_SD_ENABLE             (1 << 9)
#define SC64_CART_SCR_FLASHRAM_ENABLE       (1 << 8)
#define SC64_CART_SCR_SRAM_768K_MODE        (1 << 7)
#define SC64_CART_SCR_SRAM_ENABLE           (1 << 6)
#define SC64_CART_SCR_EEPROM_PI_ENABLE      (1 << 5)
#define SC64_CART_SCR_EEPROM_16K_MODE       (1 << 4)
#define SC64_CART_SCR_EEPROM_ENABLE         (1 << 3)
#define SC64_CART_SCR_DDIPL_ENABLE          (1 << 2)
#define SC64_CART_SCR_SDRAM_WRITABLE        (1 << 1)
#define SC64_CART_SCR_ROM_SWITCH            (1 << 0)


// GPIO
#define SC64_CART_GPIO_OFFSET_OUTPUT        (0)
#define SC64_CART_GPIO_OFFSET_INPUT         (8)
#define SC64_CART_GPIO_OFFSET_DIR           (16)
#define SC64_CART_GPIO_OFFSET_OPEN_DRAIN    (24)


// USB_SCR
#define SC64_CART_USB_SCR_FIFO_ITEMS(s)     (((s) >> 3) & 0x7FF)
#define SC64_CART_USB_SCR_READY             (1 << 1)
#define SC64_CART_USB_SCR_DMA_BUSY          (1 << 0)

#define SC64_CART_USB_SCR_FIFO_FLUSH        (1 << 2)
#define SC64_CART_USB_SCR_DMA_START         (1 << 0)


// USB_DMA_ADDR
#define SC64_CART_USB_DMA_ADDR(a)           ((SC64_BANK_SDRAM << 28) | ((a) & 0x3FFFFFC))


// USB_DMA_LEN
#define SC64_CART_USB_DMA_LEN(l)            (((l) - 1) & 0xFFFFF)


// SD Card Interface Registers

typedef struct sc64_sd_registers_s {
    volatile uint32_t SCR;                  // Clock control and bus width selection
    volatile uint32_t ARG;                  // SD command argument
    volatile uint32_t CMD;                  // SD command index and flags
    volatile uint32_t RSP;                  // SD command response
    volatile uint32_t DAT;                  // SD data path control
    volatile uint32_t DMA_SCR;              // DMA status and configuration
    volatile uint32_t DMA_ADDR;             // DMA current address
    volatile uint32_t DMA_LEN;              // DMA remaining length
    volatile uint32_t _unused[120];
    volatile uint32_t FIFO[128];            // SD data path FIFO buffer
} sc64_sd_registers_t;


#define SC64_SD_BASE                        (0x1E010000)

#define SC64_SD                             ((volatile sc64_sd_registers_t *) SC64_SD_BASE)


// SCR
#define SC64_SD_SCR_DAT_WIDTH               (1 << 2)
#define SC64_SD_SCR_CLK_MASK                (0x3 << 0)
#define SC64_SD_SCR_CLK_STOP                (0 << 0)
#define SC64_SD_SCR_CLK_400_KHZ             (1 << 0)
#define SC64_SD_SCR_CLK_25_MHZ              (2 << 0)
#define SC64_SD_SCR_CLK_50_MHZ              (3 << 0)


// CMD
#define SC64_SD_CMD_RESPONSE_CRC_ERROR      (1 << 8)
#define SC64_SD_CMD_TIMEOUT                 (1 << 7)
#define SC64_SD_CMD_BUSY                    (1 << 6)
#define SC64_SD_CMD_INDEX_GET(cmd)          ((cmd) & 0x3F)

#define SC64_SD_CMD_SKIP_RESPONSE           (1 << 8)
#define SC64_SD_CMD_LONG_RESPONSE           (1 << 7)
#define SC64_SD_CMD_START                   (1 << 6)
#define SC64_SD_CMD_INDEX(i)                ((i) & 0x3F)


// DAT
#define SC64_SD_DAT_WRITE_OK                (1 << 28)
#define SC64_SD_DAT_WRITE_ERROR             (1 << 27)
#define SC64_SD_DAT_WRITE_BUSY              (1 << 26)
#define SC64_SD_DAT_TX_FIFO_ITEMS_GET(dat)  (((dat) >> 17) & 0x1FF)
#define SC64_SD_DAT_TX_FIFO_BYTES_GET(dat)  (SC64_SD_DAT_TX_FIFO_ITEMS_GET(dat) * 4)
#define SC64_SD_DAT_TX_FIFO_FULL            (1 << 16)
#define SC64_SD_DAT_TX_FIFO_EMPTY           (1 << 15)
#define SC64_SD_DAT_TX_FIFO_UNDERRUN        (1 << 14)
#define SC64_SD_DAT_RX_FIFO_ITEMS_GET(dat)  (((dat) >> 5) & 0x1FF)
#define SC64_SD_DAT_RX_FIFO_BYTES_GET(dat)  (SC64_SD_DAT_RX_FIFO_ITEMS_GET(dat) * 4)
#define SC64_SD_DAT_RX_FIFO_FULL            (1 << 4)
#define SC64_SD_DAT_RX_FIFO_EMPTY           (1 << 3)
#define SC64_SD_DAT_RX_FIFO_OVERRUN         (1 << 2)
#define SC64_SD_DAT_CRC_ERROR               (1 << 1)
#define SC64_SD_DAT_BUSY                    (1 << 0)

#define SC64_SD_DAT_TX_FIFO_FLUSH           (1 << 19)
#define SC64_SD_DAT_RX_FIFO_FLUSH           (1 << 18)
#define SC64_SD_DAT_NUM_BLOCKS(nb)          ((((nb) - 1) & 0xFF) << 10)
#define SC64_SD_DAT_BLOCK_SIZE(bs)          (((((bs) / 4) - 1) & 0x7F) << 3)
#define SC64_SD_DAT_DIRECTION               (1 << 2)
#define SC64_SD_DAT_STOP                    (1 << 1)
#define SC64_SD_DAT_START                   (1 << 0)

#define SC64_SD_DAT_FIFO_SIZE_BYTES         (1024)
#define SC64_SD_DAT_NUM_BLOCKS_MAX          (256)
#define SC64_SD_DAT_BLOCK_SIZE_MAX          (512)


// DMA_SCR
#define SC64_SD_DMA_SCR_BUSY                (1 << 0)

#define SC64_SD_DMA_SCR_DIRECTION           (1 << 2)
#define SC64_SD_DMA_SCR_STOP                (1 << 1)
#define SC64_SD_DMA_SCR_START               (1 << 0)


// DMA_ADDR
#define SC64_SD_DMA_ADDR_GET(addr)          ((addr) & 0x3FFFFFC)
#define SC64_SD_DMA_BANK_GET(addr)          (((addr) >> 28) & 0xF)

#define SC64_SD_DMA_BANK_ADDR(b, a)         ((((b) & 0xF) << 28) | ((a) & 0x3FFFFFC))


// DMA_LEN
#define SC64_SD_DMA_LEN_GET(len)            (((len) & 0x7FFF) * 4)

#define SC64_SD_DMA_LEN(l)                  ((((l) / 4) - 1) & 0x7FFF)

#define SC64_SD_DMA_LEN_MAX                 (0x20000)


// EEPROM Block RAM

#define SC64_EEPROM_BASE                    (0x1E030000)

#define SC64_EEPROM                         ((volatile uint8_t *) SC64_EEPROM_BASE)

#define SC64_EEPROM_SIZE                    ((16 * 1024) / 8)


#endif

#ifndef SC64_REGS_H__
#define SC64_REGS_H__


#include "platform.h"


// Bank definitions

#define SC64_BANK_INVALID                   (0)
#define SC64_BANK_SDRAM                     (1)
#define SC64_BANK_CART                      (2)
#define SC64_BANK_EEPROM                    (3)
#define SC64_BANK_SD                        (4)


// Cart Interface Registers

typedef struct sc64_cart_registers {
    __IO reg_t SCR;                         // Cart status and config
    __IO reg_t BOOT;                        // Boot behavior override
    __IO reg_t VERSION;                     // Cart firmware version
    __IO reg_t GPIO;                        // Fixed I/O control
    __IO reg_t USB_SCR;                     // USB interface status and control
    __IO reg_t USB_DMA_ADDR;                // USB bank and address for DMA to PC
    __IO reg_t USB_DMA_LEN;                 // USB transfer length for DMA to PC
    __IO reg_t DDIPL_ADDR;                  // 64 Disk Drive IPL location in SDRAM
    __IO reg_t SRAM_ADDR;                   // SRAM save emulation location in SDRAM
    __IO reg_t ___unused[1015];
    __IO reg_t USB_FIFO[1024];              // USB data from PC read FIFO memory end
} sc64_cart_registers_t;

#define SC64_CART_BASE                      (0x1E000000)

#define SC64_CART                           ((__IO sc64_cart_registers_t *) SC64_CART_BASE)


#define SC64_CART_SCR_SKIP_BOOTLOADER       (1 << 10)
#define SC64_CART_SCR_FLASHRAM_ENABLE       (1 << 9)
#define SC64_CART_SCR_SRAM_768K_MODE        (1 << 8)
#define SC64_CART_SCR_SRAM_ENABLE           (1 << 7)
#define SC64_CART_SCR_SD_ENABLE             (1 << 6)
#define SC64_CART_SCR_EEPROM_PI_ENABLE      (1 << 5)
#define SC64_CART_SCR_EEPROM_16K_MODE       (1 << 4)
#define SC64_CART_SCR_EEPROM_ENABLE         (1 << 3)
#define SC64_CART_SCR_DDIPL_ENABLE          (1 << 2)
#define SC64_CART_SCR_ROM_SWITCH            (1 << 1)
#define SC64_CART_SCR_SDRAM_WRITABLE        (1 << 0)

#define SC64_CART_BOOT_SKIP_MENU            (1 << 15)
#define SC64_CART_BOOT_CIC_SEED_OVERRIDE    (1 << 14)
#define SC64_CART_BOOT_TV_TYPE_OVERRIDE     (1 << 13)
#define SC64_CART_BOOT_DDIPL_OVERRIDE       (1 << 12)
#define SC64_CART_BOOT_TV_TYPE_BIT          (10)
#define SC64_CART_BOOT_TV_TYPE_MASK         (0x3 << SC64_CART_BOOT_TV_TYPE_BIT)
#define SC64_CART_BOOT_ROM_LOADED           (1 << 9)
#define SC64_CART_BOOT_CIC_SEED_BIT         (0)
#define SC64_CART_BOOT_CIC_SEED_MASK        (0x1FF << SC64_CART_BOOT_CIC_SEED_BIT)

#define SC64_CART_VERSION_A                 (0x53363461)

#define SC64_CART_GPIO_RESET_BUTTON         (1 << 0)

#define SC64_CART_USB_SCR_FIFO_ITEMS_BIT    (3)
#define SC64_CART_USB_SCR_FIFO_ITEMS_MASK   (0x7FF << SC64_CART_USB_SCR_FIFO_ITEMS_BIT)
#define SC64_CART_USB_SCR_READY             (1 << 1)
#define SC64_CART_USB_SCR_DMA_BUSY          (1 << 0)
#define SC64_CART_USB_SCR_FIFO_FLUSH        (1 << 2)
#define SC64_CART_USB_SCR_DMA_START         (1 << 0)

#define SC64_CART_USB_SCR_FIFO_ITEMS(i)     (((i) & SC64_CART_USB_SCR_FIFO_ITEMS_MASK) >> SC64_CART_USB_SCR_FIFO_ITEMS_BIT)

#define SC64_CART_USB_DMA_ADDR_BANK_BIT     (28)
#define SC64_CART_USB_DMA_ADDR_BANK_MASK    (0xF << SC64_CART_USB_DMA_ADDR_BANK_BIT)
#define SC64_CART_USB_DMA_ADDR_ADDRESS_BIT  (0)
#define SC64_CART_USB_DMA_ADDR_ADDRESS_MASK (0x3FFFFFC << SC64_CART_USB_DMA_ADDR_ADDRESS_BIT)

#define SC64_CART_USB_DMA_ADDR(b, a)        ((((b) << SC64_CART_USB_DMA_ADDR_BANK_BIT) & SC64_CART_USB_DMA_ADDR_BANK_MASK) | (((a) << SC64_CART_USB_DMA_ADDR_ADDRESS_BIT) & SC64_CART_USB_DMA_ADDR_ADDRESS_MASK))

#define SC64_CART_USB_DMA_LEN_LENGTH_BIT    (0)
#define SC64_CART_USB_DMA_LEN_LENGTH_MASK   (0xFFFFF << SC64_CART_USB_DMA_LEN_LENGTH_BIT)

#define SC64_CART_USB_DMA_LEN(l)            ((((l) - 1) << SC64_CART_USB_DMA_LEN_LENGTH_BIT) & SC64_CART_USB_DMA_LEN_LENGTH_MASK)

#define SC64_CART_DDIPL_ADDR_ADDRESS_BIT    (0)
#define SC64_CART_DDIPL_ADDR_ADDRESS_MASK   (0x3FFFFFC << SC64_CART_DDIPL_ADDR_ADDRESS_BIT)
#define SC64_CART_DDIPL_ADDR_DEFAULT        (0x3C00000)

#define SC64_CART_DDIPL_ADDR(a)             (((a) << SC64_CART_DDIPL_ADDR_ADDRESS_BIT) & SC64_CART_DDIPL_ADDR_ADDRESS_MASK)

#define SC64_CART_SRAM_ADDR_ADDRESS_BIT     (0)
#define SC64_CART_SRAM_ADDR_ADDRESS_MASK    (0x3FFFFFC << SC64_CART_SRAM_ADDR_ADDRESS_BIT)
#define SC64_CART_SRAM_ADDR_DEFAULT         (0x3FF8000)

#define SC64_CART_SRAM_ADDR(a)              (((a) << SC64_CART_SRAM_ADDR_ADDRESS_BIT) & SC64_CART_SRAM_ADDR_ADDRESS_MASK)


// EEPROM Registers

typedef struct sc64_eeprom_registers {
    __IO reg_t MEM[512];                    // EEPROM memory
} sc64_eeprom_registers_t;

#define SC64_EEPROM_BASE                    (0x1E010000)

#define SC64_EEPROM                         ((__IO sc64_eeprom_registers_t *) SC64_EEPROM_BASE)


// SD Card Interface Registers

typedef struct sc64_sd_registers_s {
    __IO reg_t SCR;                         // Clock control and bus width selection
    __IO reg_t ARG;                         // SD command argument
    __IO reg_t CMD;                         // SD command index and flags
    __IO reg_t RSP;                         // SD command response
    __IO reg_t DAT;                         // SD data path control
    __IO reg_t DMA_SCR;                     // DMA status and configuration
    __IO reg_t DMA_ADDR;                    // DMA current address
    __IO reg_t DMA_LEN;                     // DMA remaining length
    __IO reg_t ___unused[120];
    __IO reg_t FIFO[128];                   // SD data path FIFO buffer
} sc64_sd_registers_t;

#define SC64_SD_BASE                        (0x1E020000)

#define SC64_SD                             ((__IO sc64_sd_registers_t *) SC64_SD_BASE)


#define SC64_SD_SCR_DAT_WIDTH               (1 << 2)
#define SC64_SD_SCR_CLK_MASK                (0x3 << 0)
#define SC64_SD_SCR_CLK_STOP                (0 << 0)
#define SC64_SD_SCR_CLK_400_KHZ             (1 << 0)
#define SC64_SD_SCR_CLK_25_MHZ              (2 << 0)
#define SC64_SD_SCR_CLK_50_MHZ              (3 << 0)


#define SC64_SD_CMD_RESPONSE_CRC_ERROR      (1 << 8)
#define SC64_SD_CMD_TIMEOUT                 (1 << 7)
#define SC64_SD_CMD_BUSY                    (1 << 6)
#define SC64_SD_CMD_INDEX_GET(cmd)          ((cmd) & 0x3F)

#define SC64_SD_CMD_SKIP_RESPONSE           (1 << 8)
#define SC64_SD_CMD_LONG_RESPONSE           (1 << 7)
#define SC64_SD_CMD_START                   (1 << 6)
#define SC64_SD_CMD_INDEX(i)                ((i) & 0x3F)


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


#define SC64_SD_DMA_SCR_BUSY                (1 << 0)

#define SC64_SD_DMA_SCR_DIRECTION           (1 << 2)
#define SC64_SD_DMA_SCR_STOP                (1 << 1)
#define SC64_SD_DMA_SCR_START               (1 << 0)


#define SC64_SD_DMA_ADDR_GET(addr)          ((addr) & 0x3FFFFFC)
#define SC64_SD_DMA_BANK_GET(addr)          (((addr) >> 28) & 0xF)

#define SC64_SD_DMA_BANK_ADDR(b, a)         ((((b) & 0xF) << 28) | ((a) & 0x3FFFFFC))


#define SC64_SD_DMA_LEN_GET(len)            (((len) & 0x7FFF) * 4)

#define SC64_SD_DMA_LEN(l)                  ((((l) / 4) - 1) & 0x7FFF)

#define SC64_SD_DMA_LEN_MAX                 (0x20000)


#endif

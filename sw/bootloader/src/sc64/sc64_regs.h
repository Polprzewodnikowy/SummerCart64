#ifndef SC64_REGS_H__
#define SC64_REGS_H__


#include "platform.h"


// Bank definitions

#define SC64_BANK_INVALID                   (0)
#define SC64_BANK_SDRAM                     (1)
#define SC64_BANK_CART                      (2)
#define SC64_BANK_EEPROM                    (3)
#define SC64_BANK_FLASHRAM                  (4)
#define SC64_BANK_SD                        (5)


// Cart Interface Registers

typedef struct sc64_cart_registers {
    __IO reg_t scr;                         // Cart status and config
    __IO reg_t boot;                        // Boot behavior override
    __IO reg_t version;                     // Cart firmware version
    __IO reg_t gpio;                        // Fixed I/O control
    __IO reg_t usb_scr;                     // USB interface status and control
    __IO reg_t usb_dma_addr;                // USB bank and address for DMA to PC
    __IO reg_t usb_dma_len;                 // USB transfer length for DMA to PC
    __IO reg_t ddipl_addr;                  // 64 Disk Drive IPL location in SDRAM
    __IO reg_t sram_addr;                   // SRAM save emulation location in SDRAM
    __IO reg_t __reserved[1015];
    __IO reg_t usb_fifo[1024];              // USB data from PC read FIFO memory end
} sc64_cart_registers_t;

#define SC64_CART_BASE                      (0x1E000000)

#define SC64_CART                           ((__IO sc64_cart_registers_t *) SC64_CART_BASE)

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
    __IO reg_t mem[512];                    // EEPROM memory
} sc64_eeprom_registers_t;

#define SC64_EEPROM_BASE                    (0x1E004000)

#define SC64_EEPROM                         ((__IO sc64_sd_registers_t *) SC64_EEPROM_BASE)


// SD Card Interface Registers

typedef struct sc64_sd_registers {
    __IO reg_t scr;                         // Clock prescaler and busy flag
    __IO reg_t cs;                          // Chip select pin control
    __IO reg_t dr;                          // Data to be sent and return value
    __IO reg_t multi;                       // Multi byte transmission
    __IO reg_t dma_scr;                     // DMA configuration
    __IO reg_t dma_addr;                    // DMA starting address
    __IO reg_t __reserved[122];
    __IO reg_t buffer[128];                 // Multi byte transmission buffer
} sc64_sd_registers_t;

#define SC64_SD_BASE                        (0x1E008000)

#define SC64_SD                             ((__IO sc64_sd_registers_t *) SC64_SD_BASE)

#define SC64_SC_SCR_PRESCALER_BIT           (1)
#define SC64_SD_SCR_PRESCALER_MASK          (0x7 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_2             (0 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_4             (1 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_8             (2 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_16            (3 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_32            (4 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_64            (5 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_128           (6 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_PRESCALER_256           (7 << SC64_SC_SCR_PRESCALER_BIT)
#define SC64_SD_SCR_BUSY                    (1 << 0)

#define SC64_SD_CS_PIN                      (1 << 0)

#define SC64_SD_DR_BUSY                     (1 << 8)
#define SC64_SD_DR_DATA_BIT                 (0)
#define SC64_SD_DR_DATA_MASK                (0xFF << SC64_SD_DR_DATA_BIT)

#define SC64_SD_DR_DATA(d)                  (((d) & SC64_SD_DR_DATA_MASK) >> SC64_SD_DR_DATA_BIT)

#define SC64_SD_MULTI_DMA                   (1 << 10)
#define SC64_SD_MULTI_RX_ONLY               (1 << 9)
#define SC64_SD_MULTI_LENGTH_BIT            (0)
#define SC64_SD_MULTI_LENGTH_MASK           (0x1FF << SC64_SD_MULTI_LENGTH_BIT)

#define SC64_SD_MULTI(d, r, l)              ((d ? (SC64_SD_MULTI_DMA) : 0) | ((r) ? (SC64_SD_MULTI_RX_ONLY) : 0) | ((((l) - 1) & SC64_SD_MULTI_LENGTH_MASK) << SC64_SD_MULTI_LENGTH_BIT))

#define SC64_SD_DMA_SCR_FLUSH               (1 << 0)
#define SC64_SD_DMA_SCR_FIFO_FULL           (1 << 1)
#define SC64_SD_DMA_SCR_FIFO_EMPTY          (1 << 0)

#define SC64_SD_DMA_ADDR_BANK_BIT           (28)
#define SC64_SD_DMA_ADDR_BANK_MASK          (0xF << SC64_SD_DMA_ADDR_BANK_BIT)
#define SC64_SD_DMA_ADDR_ADDRESS_BIT        (0)
#define SC64_SD_DMA_ADDR_ADDRESS_MASK       (0x3FFFFFC << SC64_SD_DMA_ADDR_ADDRESS_BIT)

#define SC64_SD_DMA_ADDR(b, a)              ((((b) << SC64_SD_DMA_ADDR_BANK_BIT) & SC64_SD_DMA_ADDR_BANK_MASK) | (((a) << SC64_SD_DMA_ADDR_ADDRESS_BIT) & SC64_SD_DMA_ADDR_ADDRESS_MASK))


#endif

#ifndef FPGA_H__
#define FPGA_H__


#include <stddef.h>
#include <stdint.h>


typedef enum {
    CMD_IDENTIFY,
    CMD_REG_READ,
    CMD_REG_WRITE,
    CMD_MEM_READ,
    CMD_MEM_WRITE,
    CMD_USB_STATUS,
    CMD_USB_READ,
    CMD_USB_WRITE,
    CMD_FLASHRAM_READ,
    CMD_EEPROM_READ,
    CMD_EEPROM_WRITE
} fpga_cmd_t;

typedef enum {
    REG_STATUS,
    REG_MEM_ADDRESS,
    REG_MEM_SCR,
    REG_USB_SCR,
    REG_USB_DMA_ADDRESS,
    REG_USB_DMA_LENGTH,
    REG_USB_DMA_SCR,
    REG_CFG_SCR,
    REG_CFG_DATA_0,
    REG_CFG_DATA_1,
    REG_CFG_CMD,
    REG_CFG_VERSION,
    REG_FLASHRAM_SCR,
    REG_FLASH_SCR,
    REG_RTC_SCR,
    REG_RTC_TIME_0,
    REG_RTC_TIME_1,
    REG_SD_SCR,
    REG_SD_ARG,
    REG_SD_CMD,
    REG_SD_RSP_0,
    REG_SD_RSP_1,
    REG_SD_RSP_2,
    REG_SD_RSP_3,
    REG_SD_DAT,
    REG_SD_DMA_ADDRESS,
    REG_SD_DMA_LENGTH,
    REG_SD_DMA_SCR,
} fpga_reg_t;


#define FPGA_ID                     (0x64)

#define FPGA_MAX_MEM_TRANSFER       (1024)

#define MEM_SCR_START               (1 << 0)
#define MEM_SCR_STOP                (1 << 1)
#define MEM_SCR_DIRECTION           (1 << 2)
#define MEM_SCR_BUSY                (1 << 3)
#define MEM_SCR_LENGTH_BIT          (4)

#define USB_STATUS_RXNE             (1 << 0)
#define USB_STATUS_TXE              (1 << 1)

#define STATUS_BUTTON               (1 << 0)
#define STATUS_USB_RESET_PENDING    (1 << 1)
#define STATUS_DMA_BUSY             (1 << 2)
#define STATUS_CFG_PENDING          (1 << 3)
#define STATUS_FLASHRAM_PENDING     (1 << 4)
#define STATUS_USB_RXNE             (1 << 5)
#define STATUS_USB_TXE              (1 << 6)

#define USB_SCR_FIFO_FLUSH          (1 << 0)
#define USB_SCR_RXNE                (1 << 1)
#define USB_SCR_TXE                 (1 << 2)
#define USB_SCR_RESET_PENDING       (1 << 3)
#define USB_SCR_RESET_ACK           (1 << 4)
#define USB_SCR_WRITE_FLUSH         (1 << 5)

#define DMA_SCR_START               (1 << 0)
#define DMA_SCR_STOP                (1 << 1)
#define DMA_SCR_DIRECTION           (1 << 2)
#define DMA_SCR_BUSY                (1 << 3)

#define CFG_SCR_BOOTLOADER_ENABLED  (1 << 0)
#define CFG_SCR_BOOTLOADER_SKIP     (1 << 1)
#define CFG_SCR_ROM_WRITE_ENABLED   (1 << 2)
#define CFG_SCR_ROM_SHADOW_ENABLED  (1 << 3)
#define CFG_SCR_SRAM_ENABLED        (1 << 4)
#define CFG_SCR_SRAM_BANKED         (1 << 5)
#define CFG_SCR_FLASHRAM_ENABLED    (1 << 6)
#define CFG_SCR_DD_ENABLED          (1 << 7)
#define CFG_SCR_EEPROM_ENABLED      (1 << 8)
#define CFG_SCR_EEPROM_16K          (1 << 9)

#define CFG_CMD_DONE                (1 << 0)
#define CFG_CMD_ERROR               (1 << 1)
#define CFG_CMD_IRQ                 (1 << 2)

#define FLASHRAM_SCR_DONE           (1 << 0)
#define FLASHRAM_SCR_PENDING        (1 << 1)
#define FLASHRAM_SCR_PAGE_BIT       (2)
#define FLASHRAM_SCR_PAGE_MASK      (0x3FF << FLASHRAM_SCR_PAGE_BIT)
#define FLASHRAM_SCR_SECTOR_OR_ALL  (1 << 12)
#define FLASHRAM_SCR_WRITE_OR_ERASE (1 << 13)

#define FLASH_SCR_BUSY              (1 << 0)

#define RTC_SCR_PENDING             (1 << 0)
#define RTC_SCR_DONE                (1 << 1)

#define SD_SCR_CLOCK_MODE_OFF       (0 << 0)
#define SD_SCR_CLOCK_MODE_400KHZ    (1 << 0)
#define SD_SCR_CLOCK_MODE_25MHZ     (2 << 0)
#define SD_SCR_CLOCK_MODE_50MHZ     (3 << 0)


uint8_t fpga_id_get (void);
uint32_t fpga_reg_get (fpga_reg_t reg);
void fpga_reg_set (fpga_reg_t reg, uint32_t value);
void fpga_mem_read (uint32_t address, size_t length, uint8_t *buffer);
void fpga_mem_write (uint32_t address, size_t length, uint8_t *buffer);
uint8_t fpga_usb_status_get (void);
uint8_t fpga_usb_pop (void);
void fpga_usb_push (uint8_t data);
void fpga_flashram_buffer_read (uint8_t *buffer);
void fpga_eeprom_read (uint16_t address, size_t length, uint8_t *buffer);
void fpga_eeprom_write (uint16_t address, size_t length, uint8_t *buffer);


#endif

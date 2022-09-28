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
    CMD_USB_WRITE
} fpga_cmd_t;

typedef enum {
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
    REG_DD_SCR,
    REG_DD_CMD_DATA,
    REG_DD_HEAD_TRACK,
    REG_DD_SECTOR_INFO,
    REG_DD_DRIVE_ID,
    REG_VENDOR_SCR,
    REG_VENDOR_DATA,
    REG_DEBUG
} fpga_reg_t;


#define	ALIGN(value, align)             (((value) + ((typeof(value))(align) - 1)) & ~((typeof(value))(align) - 1))
#define SWAP16(x)                       ((((x) & 0xFF) << 8) | (((x) & 0xFF00) >> 8))
#define SWAP32(x)                       (((x) & 0xFF) << 24 | ((x) & 0xFF00) << 8 | ((x) & 0xFF0000) >> 8 | ((x) & 0xFF000000) >> 24)

#define FPGA_ID                         (0x64)

#define FPGA_MAX_MEM_TRANSFER           (1024)

#define USB_STATUS_RXNE                 (1 << 0)
#define USB_STATUS_TXE                  (1 << 1)

#define MEM_SCR_START                   (1 << 0)
#define MEM_SCR_STOP                    (1 << 1)
#define MEM_SCR_DIRECTION               (1 << 2)
#define MEM_SCR_BUSY                    (1 << 3)
#define MEM_SCR_LENGTH_BIT              (4)

#define USB_SCR_FIFO_FLUSH              (1 << 0)
#define USB_SCR_RXNE                    (1 << 1)
#define USB_SCR_TXE                     (1 << 2)
#define USB_SCR_RESET_PENDING           (1 << 3)
#define USB_SCR_RESET_ACK               (1 << 4)
#define USB_SCR_WRITE_FLUSH             (1 << 5)
#define USB_SCR_RX_COUNT_BIT            (6)
#define USB_SCR_RX_COUNT_MASK           (0x7FF << USB_SCR_RX_COUNT_BIT)
#define USB_SCR_TX_COUNT_BIT            (17)
#define USB_SCR_TX_COUNT_MASK           (0x7FF << USB_SCR_TX_COUNT_BIT)

#define DMA_SCR_START                   (1 << 0)
#define DMA_SCR_STOP                    (1 << 1)
#define DMA_SCR_DIRECTION               (1 << 2)
#define DMA_SCR_BUSY                    (1 << 3)

#define CFG_SCR_BOOTLOADER_ENABLED      (1 << 0)
#define CFG_SCR_BOOTLOADER_SKIP         (1 << 1)
#define CFG_SCR_ROM_WRITE_ENABLED       (1 << 2)
#define CFG_SCR_ROM_SHADOW_ENABLED      (1 << 3)
#define CFG_SCR_SRAM_ENABLED            (1 << 4)
#define CFG_SCR_SRAM_BANKED             (1 << 5)
#define CFG_SCR_FLASHRAM_ENABLED        (1 << 6)
#define CFG_SCR_DD_ENABLED              (1 << 7)
#define CFG_SCR_DDIPL_ENABLED           (1 << 8)
#define CFG_SCR_EEPROM_ENABLED          (1 << 9)
#define CFG_SCR_EEPROM_16K              (1 << 10)
#define CFG_SCR_ROM_EXTENDED_ENABLED    (1 << 11)
#define CFG_SCR_BUTTON_STATE            (1 << 31)

#define CFG_CMD_BIT                     (0)
#define CFG_CMD_MASK                    (0xFF << CFG_CMD_BIT)
#define CFG_CMD_PENDING                 (1 << 8)
#define CFG_CMD_DONE                    (1 << 9)
#define CFG_CMD_ERROR                   (1 << 10)
#define CFG_CMD_IRQ                     (1 << 11)

#define FLASHRAM_SCR_DONE               (1 << 0)
#define FLASHRAM_SCR_PENDING            (1 << 1)
#define FLASHRAM_SCR_PAGE_BIT           (2)
#define FLASHRAM_SCR_PAGE_MASK          (0x3FF << FLASHRAM_SCR_PAGE_BIT)
#define FLASHRAM_SCR_SECTOR_OR_ALL      (1 << 12)
#define FLASHRAM_SCR_WRITE_OR_ERASE     (1 << 13)

#define FLASH_SCR_BUSY                  (1 << 0)

#define RTC_SCR_PENDING                 (1 << 0)
#define RTC_SCR_DONE                    (1 << 1)
#define RTC_SCR_MAGIC                   (0x52544300)
#define RTC_SCR_MAGIC_MASK              (0xFFFFFF00)

#define SD_SCR_CLOCK_MODE_OFF           (0 << 0)
#define SD_SCR_CLOCK_MODE_400KHZ        (1 << 0)
#define SD_SCR_CLOCK_MODE_25MHZ         (2 << 0)
#define SD_SCR_CLOCK_MODE_50MHZ         (3 << 0)
#define SD_SCR_CMD_BUSY                 (1 << 2)
#define SD_SCR_CMD_ERROR                (1 << 3)
#define SD_SCR_CARD_BUSY                (1 << 4)
#define SD_SCR_CARD_INSERTED            (1 << 5)
#define SD_SCR_RX_COUNT_BIT             (6)
#define SD_SCR_RX_COUNT_MASK            (0x7FF << SD_SCR_RX_COUNT_BIT)
#define SD_SCR_TX_COUNT_BIT             (17)
#define SD_SCR_TX_COUNT_MASK            (0x7FF << SD_SCR_TX_COUNT_BIT)

#define SD_CMD_INDEX_BIT                (0)
#define SD_CMD_INDEX_MASK               (0x3F)
#define SD_CMD_SKIP_RESPONSE            (1 << 6)
#define SD_CMD_RESERVED_RESPONSE        (1 << 7)
#define SD_CMD_LONG_RESPONSE            (1 << 8)
#define SD_CMD_IGNORE_CRC               (1 << 9)

#define SD_DAT_FIFO_FLUSH               (1 << 0)
#define SD_DAT_START_WRITE              (1 << 1)
#define SD_DAT_START_READ               (1 << 2)
#define SD_DAT_STOP                     (1 << 3)
#define SD_DAT_BLOCKS_BIT               (4)
#define SD_DAT_BLOCKS_MASK              (0xFF << SD_DAT_BLOCKS_BIT)
#define SD_DAT_BUSY                     (1 << 12)
#define SD_DAT_ERROR                    (1 << 13)

#define DD_SCR_HARD_RESET               (1 << 0)
#define DD_SCR_HARD_RESET_CLEAR         (1 << 1)
#define DD_SCR_CMD_PENDING              (1 << 2)
#define DD_SCR_CMD_READY                (1 << 3)
#define DD_SCR_BM_PENDING               (1 << 4)
#define DD_SCR_BM_READY                 (1 << 5)
#define DD_SCR_DISK_INSERTED            (1 << 6)
#define DD_SCR_DISK_CHANGED             (1 << 7)
#define DD_SCR_BM_START                 (1 << 8)
#define DD_SCR_BM_START_CLEAR           (1 << 9)
#define DD_SCR_BM_STOP                  (1 << 10)
#define DD_SCR_BM_STOP_CLEAR            (1 << 11)
#define DD_SCR_BM_TRANSFER_MODE         (1 << 12)
#define DD_SCR_BM_TRANSFER_BLOCKS       (1 << 13)
#define DD_SCR_BM_TRANSFER_DATA         (1 << 14)
#define DD_SCR_BM_TRANSFER_C2           (1 << 15)
#define DD_SCR_BM_MICRO_ERROR           (1 << 16)
#define DD_SCR_BM_ACK                   (1 << 17)
#define DD_SCR_BM_ACK_CLEAR             (1 << 18)
#define DD_SCR_BM_CLEAR                 (1 << 19)

#define DD_TRACK_MASK                   (0x0FFF)
#define DD_HEAD_MASK                    (0x1000)
#define DD_HEAD_TRACK_MASK              (DD_HEAD_MASK | DD_TRACK_MASK)
#define DD_HEAD_TRACK_INDEX_LOCK        (1 << 13)


uint8_t fpga_id_get (void);
uint32_t fpga_reg_get (fpga_reg_t reg);
void fpga_reg_set (fpga_reg_t reg, uint32_t value);
void fpga_mem_read (uint32_t address, size_t length, uint8_t *buffer);
void fpga_mem_write (uint32_t address, size_t length, uint8_t *buffer);
void fpga_mem_copy (uint32_t src, uint32_t dst, size_t length);
uint8_t fpga_usb_status_get (void);
uint8_t fpga_usb_pop (void);
void fpga_usb_push (uint8_t data);


#endif

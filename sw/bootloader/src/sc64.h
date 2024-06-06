#ifndef SC64_H__
#define SC64_H__


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef enum {
    ERROR_TYPE_CFG = 0,
    ERROR_TYPE_SD_CARD = 1,
} sc64_error_type_t;

typedef enum {
    SC64_OK = 0,
    CFG_ERROR_UNKNOWN_COMMAND = 1,
    CFG_ERROR_INVALID_ARGUMENT = 2,
    CFG_ERROR_INVALID_ADDRESS = 3,
    CFG_ERROR_INVALID_ID = 4,
} sc64_cfg_error_t;

typedef enum {
    SD_OK = 0,
    SD_ERROR_NO_CARD_IN_SLOT = 1,
    SD_ERROR_NOT_INITIALIZED = 2,
    SD_ERROR_INVALID_ARGUMENT = 3,
    SD_ERROR_INVALID_ADDRESS = 4,
    SD_ERROR_INVALID_OPERATION = 5,
    SD_ERROR_CMD2_IO = 6,
    SD_ERROR_CMD3_IO = 7,
    SD_ERROR_CMD6_CHECK_IO = 8,
    SD_ERROR_CMD6_CHECK_CRC = 9,
    SD_ERROR_CMD6_CHECK_TIMEOUT = 10,
    SD_ERROR_CMD6_CHECK_RESPONSE = 11,
    SD_ERROR_CMD6_SWITCH_IO = 12,
    SD_ERROR_CMD6_SWITCH_CRC = 13,
    SD_ERROR_CMD6_SWITCH_TIMEOUT = 14,
    SD_ERROR_CMD6_SWITCH_RESPONSE = 15,
    SD_ERROR_CMD7_IO = 16,
    SD_ERROR_CMD8_IO = 17,
    SD_ERROR_CMD9_IO = 18,
    SD_ERROR_CMD10_IO = 19,
    SD_ERROR_CMD18_IO = 20,
    SD_ERROR_CMD18_CRC = 21,
    SD_ERROR_CMD18_TIMEOUT = 22,
    SD_ERROR_CMD25_IO = 23,
    SD_ERROR_CMD25_CRC = 24,
    SD_ERROR_CMD25_TIMEOUT = 25,
    SD_ERROR_ACMD6_IO = 26,
    SD_ERROR_ACMD41_IO = 27,
    SD_ERROR_ACMD41_OCR = 28,
    SD_ERROR_ACMD41_TIMEOUT = 29,
} sc64_sd_error_t;

typedef uint32_t sc64_error_t;

typedef enum {
    CFG_ID_BOOTLOADER_SWITCH,
    CFG_ID_ROM_WRITE_ENABLE,
    CFG_ID_ROM_SHADOW_ENABLE,
    CFG_ID_DD_MODE,
    CFG_ID_ISV_ADDRESS,
    CFG_ID_BOOT_MODE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_DD_SD_ENABLE,
    CFG_ID_DD_DRIVE_TYPE,
    CFG_ID_DD_DISK_STATE,
    CFG_ID_BUTTON_STATE,
    CFG_ID_BUTTON_MODE,
    CFG_ID_ROM_EXTENDED_ENABLE,
} sc64_cfg_id_t;

typedef enum {
    SETTING_ID_LED_ENABLE,
} sc64_setting_id_t;

typedef enum {
    DD_MODE_DISABLED = 0,
    DD_MODE_REGS = 1,
    DD_MODE_IPL = 2,
    DD_MODE_FULL = 3
} sc64_dd_mode_t;

typedef enum {
    BOOT_MODE_MENU = 0,
    BOOT_MODE_ROM = 1,
    BOOT_MODE_DDIPL = 2,
    BOOT_MODE_DIRECT_ROM = 3,
    BOOT_MODE_DIRECT_DDIPL = 4,
} sc64_boot_mode_t;

typedef enum {
    SAVE_TYPE_NONE = 0,
    SAVE_TYPE_EEPROM_4K = 1,
    SAVE_TYPE_EEPROM_16K = 2,
    SAVE_TYPE_SRAM = 3,
    SAVE_TYPE_FLASHRAM = 4,
    SAVE_TYPE_SRAM_BANKED = 5,
    SAVE_TYPE_SRAM_1M = 6
} sc64_save_type_t;

typedef enum {
    CIC_SEED_AUTO = 0xFFFF
} sc64_cic_seed_t;

typedef enum {
    TV_TYPE_PAL = 0,
    TV_TYPE_NTSC = 1,
    TV_TYPE_MPAL = 2,
    TV_TYPE_PASSTHROUGH = 3
} sc64_tv_type_t;

typedef enum {
    DRIVE_TYPE_RETAIL,
    DRIVE_TYPE_DEVELOPMENT,
} sc64_drive_type_t;

typedef enum {
    DISK_STATE_EJECTED,
    DISK_STATE_INSERTED,
    DISK_STATE_CHANGED,
} sc64_disk_state_t;

typedef enum {
    BUTTON_MODE_NONE,
    BUTTON_MODE_N64_IRQ,
    BUTTON_MODE_USB_PACKET,
    BUTTON_MODE_DD_DISK_SWAP,
} sc64_button_mode_t;

typedef enum {
    DIAGNOSTIC_ID_VOLTAGE_TEMPERATURE,
} sc64_diagnostic_id_t;

typedef struct {
    sc64_boot_mode_t boot_mode;
    sc64_cic_seed_t cic_seed;
    sc64_tv_type_t tv_type;
} sc64_boot_params_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} sc64_rtc_time_t;

typedef enum {
    SD_CARD_STATUS_INSERTED = (1 << 0),
    SD_CARD_STATUS_INITIALIZED = (1 << 1),
    SD_CARD_STATUS_TYPE_BLOCK = (1 << 2),
    SD_CARD_STATUS_50MHZ_MODE = (1 << 3),
    SD_CARD_STATUS_BYTE_SWAP = (1 << 4),
} sc64_sd_card_status_t;

typedef enum {
    SC64_IRQ_NONE = 0,
    SC64_IRQ_MCU = (1 << 0),
    SC64_IRQ_CMD = (1 << 1),
} sc64_irq_t;


typedef struct {
    volatile uint8_t BUFFER[8192];
    volatile uint8_t EEPROM[2048];
    volatile uint8_t DD_SECTOR[256];
    volatile uint8_t FLASHRAM[128];
} sc64_buffers_t;

#define SC64_BUFFERS_BASE   (0x1FFE0000UL)
#define SC64_BUFFERS        ((sc64_buffers_t *) SC64_BUFFERS_BASE)


const char *sc64_error_description (sc64_error_t error);

void sc64_unlock (void);
void sc64_lock (void);
bool sc64_check_presence (void);

void sc64_cmd_irq_enable (bool enable);
sc64_irq_t sc64_irq_pending (void);
void sc64_irq_callback (sc64_irq_t irq);

sc64_error_t sc64_get_identifier (uint32_t *identifier);
sc64_error_t sc64_get_version (uint16_t *major, uint16_t *minor, uint32_t *revision);

sc64_error_t sc64_get_config (sc64_cfg_id_t id, uint32_t *value);
sc64_error_t sc64_set_config (sc64_cfg_id_t id, uint32_t value);
sc64_error_t sc64_get_setting (sc64_setting_id_t id, uint32_t *value);
sc64_error_t sc64_set_setting (sc64_setting_id_t id, uint32_t value);
sc64_error_t sc64_get_boot_params (sc64_boot_params_t *params);

sc64_error_t sc64_get_time (sc64_rtc_time_t *t);
sc64_error_t sc64_set_time (sc64_rtc_time_t *t);

sc64_error_t sc64_usb_get_status (bool *reset_state, bool *cable_unplugged);
sc64_error_t sc64_usb_read_info (uint8_t *type, uint32_t *length);
sc64_error_t sc64_usb_read_busy (bool *read_busy);
sc64_error_t sc64_usb_write_busy (bool *write_busy);
sc64_error_t sc64_usb_read (void *address, uint32_t length);
sc64_error_t sc64_usb_write (void *address, uint8_t type, uint32_t length);

sc64_error_t sc64_sd_card_init (void);
sc64_error_t sc64_sd_card_deinit (void);
sc64_error_t sc64_sd_card_get_status (sc64_sd_card_status_t *sd_card_status);
sc64_error_t sc64_sd_card_get_info (void *address);
sc64_error_t sc64_sd_set_byte_swap (bool enabled);
sc64_error_t sc64_sd_read_sectors (void *address, uint32_t sector, uint32_t count);
sc64_error_t sc64_sd_write_sectors (void *address, uint32_t sector, uint32_t count);

sc64_error_t sc64_set_disk_mapping (void *address, uint32_t length);

sc64_error_t sc64_writeback_pending (bool *pending);
sc64_error_t sc64_writeback_enable (void *address);
sc64_error_t sc64_writeback_disable (void);

sc64_error_t sc64_flash_program (void *address, uint32_t length);
sc64_error_t sc64_flash_wait_busy (void);
sc64_error_t sc64_flash_get_erase_block_size (size_t *erase_block_size);
sc64_error_t sc64_flash_erase_block (void *address);

sc64_error_t sc64_get_diagnostic (sc64_diagnostic_id_t id, uint32_t *value);


#endif

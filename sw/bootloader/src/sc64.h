#ifndef SC64_H__
#define SC64_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    SC64_OK,
    SC64_ERROR_BAD_ARGUMENT,
    SC64_ERROR_BAD_ADDRESS,
    SC64_ERROR_BAD_CONFIG_ID,
    SC64_ERROR_TIMEOUT,
    SC64_ERROR_SD_CARD,
    SC64_ERROR_UNKNOWN_CMD = -1
} sc64_error_t;

typedef enum {
    CFG_ID_BOOTLOADER_SWITCH,
    CFG_ID_ROM_WRITE_ENABLE,
    CFG_ID_ROM_SHADOW_ENABLE,
    CFG_ID_DD_MODE,
    CFG_ID_ISV_ENABLE,
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
    BOOT_MODE_DIRECT = 3
} sc64_boot_mode_t;

typedef enum {
    SAVE_TYPE_NONE = 0,
    SAVE_TYPE_EEPROM_4K = 1,
    SAVE_TYPE_EEPROM_16K = 2,
    SAVE_TYPE_SRAM = 3,
    SAVE_TYPE_FLASHRAM = 4,
    SAVE_TYPE_SRAM_BANKED = 5
} sc64_save_type_t;

typedef enum {
    CIC_SEED_UNKNOWN = 0xFFFF
} sc64_cic_seed_t;

typedef enum {
    TV_TYPE_PAL = 0,
    TV_TYPE_NTSC = 1,
    TV_TYPE_MPAL = 2,
    TV_TYPE_UNKNOWN = 3
} sc64_tv_type_t;

typedef enum {
    BUTTON_MODE_NONE,
    BUTTON_MODE_N64_IRQ,
    BUTTON_MODE_USB_PACKET,
    BUTTON_MODE_DD_DISK_SWAP,
} sc64_button_mode_t;

typedef enum {
    SD_CARD_STATUS_INSERTED = (1 << 0),
    SD_CARD_STATUS_INITIALIZED = (1 << 1),
    SD_CARD_STATUS_TYPE_BLOCK = (1 << 2),
    SD_CARD_STATUS_50MHZ_MODE = (1 << 3),
} sc64_sd_card_status_t;

typedef struct {
    sc64_boot_mode_t boot_mode;
    sc64_cic_seed_t cic_seed;
    sc64_tv_type_t tv_type;
} sc64_boot_info_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} sc64_rtc_time_t;


typedef struct {
    volatile uint8_t BUFFER[8192];
    volatile uint8_t EEPROM[2048];
    volatile uint8_t DD_SECTOR[256];
    volatile uint8_t FLASHRAM[128];
} sc64_buffers_t;

#define SC64_BUFFERS_BASE   (0x1FFE0000UL)
#define SC64_BUFFERS        ((sc64_buffers_t *) SC64_BUFFERS_BASE)


sc64_error_t sc64_get_error (void);

void sc64_unlock (void);
void sc64_lock (void);
bool sc64_check_presence (void);

bool sc64_irq_pending (void);
void sc64_irq_clear (void);

uint32_t sc64_get_config (sc64_cfg_id_t id);
void sc64_set_config (sc64_cfg_id_t id, uint32_t value);
uint32_t sc64_get_setting (sc64_setting_id_t id);
void sc64_set_setting (sc64_setting_id_t id, uint32_t value);
void sc64_get_boot_info (sc64_boot_info_t *info);

void sc64_get_time (sc64_rtc_time_t *t);
void sc64_set_time (sc64_rtc_time_t *t);

bool sc64_usb_read_ready (uint8_t *type, uint32_t *length);
bool sc64_usb_read (void *address, uint32_t length);
bool sc64_usb_write_ready (void);
bool sc64_usb_write (void *address, uint8_t type, uint32_t length);

bool sc64_sd_card_init (void);
bool sc64_sd_card_deinit (void);
sc64_sd_card_status_t sc64_sd_card_get_status (void);
bool sc64_sd_card_get_info (void *address);
bool sc64_sd_write_sectors (void *address, uint32_t sector, uint32_t count);
bool sc64_sd_read_sectors (void *address, uint32_t sector, uint32_t count);
bool sc64_dd_set_sd_disk_info (void *address, uint32_t length);
bool sc64_writeback_enable (void *address);

bool sc64_flash_program (void *address, uint32_t length);
void sc64_flash_wait_busy (void);
uint32_t sc64_flash_get_erase_block_size (void);
bool sc64_flash_erase_block (void *address);


#endif

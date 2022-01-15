#ifndef SC64_H__
#define SC64_H__


#include <stdbool.h>
#include <stdint.h>
// #include <stdio.h>
#include "io.h"


#define SC64_CMD_CONFIG             ('C')
#define SC64_CMD_QUERY              ('Q')
// #define SC64_CMD_DEBUG_RX_DATA      ('E')
// #define SC64_CMD_DEBUG_RX_READY     ('A')
// #define SC64_CMD_DEBUG_RX_BUSY      ('F')
// #define SC64_CMD_DEBUG_TX_DATA      ('D')
// #define SC64_CMD_DEBUG_TX_READY     ('S')
#define SC64_CMD_UART_PUT           ('Z')

#define SC64_VERSION_2              (0x53437632)

// #define SC64_DEBUG_WRITE_ADDRESS    (0x13FF8000UL)
// #define SC64_DEBUG_READ_ADDRESS     (0x13FF0000UL)
// #define SC64_DEBUG_MAX_SIZE         (32 * 1024)

// #define SC64_DEBUG_ID_TEXT          (0x01)
// #define SC64_DEBUG_ID_FSD_READ      (0xF1)
// #define SC64_DEBUG_ID_FSD_WRITE     (0xF2)
// #define SC64_DEBUG_ID_FSD_SECTOR    (0xF3)


typedef enum {
    CFG_ID_SCR,
    CFG_ID_SDRAM_SWITCH,
    CFG_ID_SDRAM_WRITABLE,
    CFG_ID_DD_ENABLE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_SAVE_OFFEST,
    CFG_ID_DDIPL_OFFEST,
    CFG_ID_BOOT_MODE,
    CFG_ID_FLASH_SIZE,
    CFG_ID_FLASH_READ,
    CFG_ID_FLASH_PROGRAM,
    CFG_ID_RECONFIGURE,
    CFG_ID_DD_DRIVE_ID,
    CFG_ID_DD_DISK_STATE,
    CFG_ID_DD_THB_TABLE_OFFSET,
    CFG_ID_IS_VIEWER_ENABLE,
} cfg_id_t;

// typedef enum {
//     SAVE_TYPE_NONE = 0,
//     SAVE_TYPE_EEPROM_4K = 1,
//     SAVE_TYPE_EEPROM_16K = 2,
//     SAVE_TYPE_SRAM = 3,
//     SAVE_TYPE_FLASHRAM = 4,
//     SAVE_TYPE_SRAM_BANKED = 5,
//     SAVE_TYPE_FLASHRAM_PKST2 = 6,
// } save_type_t;

typedef enum {
    TV_TYPE_PAL = 0,
    TV_TYPE_NTSC = 1,
    TV_TYPE_MPAL = 2,
    TV_TYPE_UNKNOWN = 3,
} tv_type_t;

typedef enum {
    CIC_SEED_UNKNOWN = 0xFFFF,
} cic_seed_t;

typedef enum {
    BOOT_MODE_MENU_SD = 0,
    BOOT_MODE_MENU_USB = 1,
    BOOT_MODE_ROM = 2,
    BOOT_MODE_DDIPL = 3,
    BOOT_MODE_DIRECT = 4,
} boot_mode_t;

typedef struct {
    // bool dd_enabled;
    // bool is_viewer_enabled;
    // save_type_t save_type;
    boot_mode_t boot_mode;
    uint16_t cic_seed;
    tv_type_t tv_type;
    // io32_t *save_location;
    // io32_t *ddipl_location;
} sc64_info_t;


typedef enum {
    SC64_STORAGE_TYPE_SD,
    SC64_STORAGE_TYPE_USB,
} sc64_storage_type_t;

typedef enum {
    SC64_STORAGE_OK,
    SC64_STORAGE_ERROR,
} sc64_storage_error_t;


bool sc64_check_presence (void);
void sc64_wait_cpu_ready (void);
bool sc64_wait_cpu_busy (void);
bool sc64_perform_cmd (uint8_t cmd, uint32_t *args, uint32_t *result);
uint32_t sc64_get_config (cfg_id_t id);
void sc64_set_config (cfg_id_t id, uint32_t value);
void sc64_get_info (sc64_info_t *info);
// void sc64_wait_usb_rx_ready (uint32_t *type, uint32_t *length);
// void sc64_wait_usb_rx_busy (void);
// void sc64_usb_rx_data (io32_t *address, uint32_t length);
// void sc64_wait_usb_tx_ready (void);
// void sc64_usb_tx_data (io32_t *address, uint32_t length);
// void sc64_debug_write (uint8_t type, const void *data, uint32_t len);
// void sc64_debug_fsd_read (const void *data, uint32_t sector, uint32_t count);
// void sc64_debug_fsd_write (const void *data, uint32_t sector, uint32_t count);
// void sc64_init_is_viewer (void);
void sc64_uart_print_string (const char *text);
sc64_storage_error_t sc64_storage_read(sc64_storage_type_t storage_type, const void *buff, uint32_t sector, uint32_t count);
void sc64_init (void);


#endif

#ifndef SC64_H__
#define SC64_H__


#include <stdbool.h>
#include <stdint.h>
#include "io.h"


#define SC64_CMD_QUERY              ('Q')
#define SC64_CMD_CONFIG             ('C')
#define SC64_CMD_DRIVE_INIT         (0xF0)
#define SC64_CMD_DRIVE_BUSY         (0xF1)
#define SC64_CMD_DRIVE_READ         (0xF2)
#define SC64_CMD_DRIVE_WRITE        (0xF3)
#define SC64_CMD_UART_PUT           (0xFF)

#define SC64_VERSION_2              (0x53437632)


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

typedef enum {
    CIC_SEED_UNKNOWN = 0xFFFF,
} cic_seed_t;

typedef enum {
    TV_TYPE_PAL = 0,
    TV_TYPE_NTSC = 1,
    TV_TYPE_MPAL = 2,
    TV_TYPE_UNKNOWN = 3,
} tv_type_t;

typedef enum {
    BOOT_MODE_MENU_SD = 0,
    BOOT_MODE_MENU_USB = 1,
    BOOT_MODE_ROM = 2,
    BOOT_MODE_DDIPL = 3,
    BOOT_MODE_DIRECT = 4,
} boot_mode_t;

typedef struct {
    boot_mode_t boot_mode;
    uint16_t cic_seed;
    tv_type_t tv_type;
} sc64_info_t;

typedef enum {
    DRIVE_SD = 0,
    DRIVE_USB = 1,
    __DRIVE_COUNT = 2
} drive_id_t;


bool sc64_check_presence (void);
uint32_t sc64_query_config (cfg_id_t id);
void sc64_change_config (cfg_id_t id, uint32_t value);
void sc64_get_info (sc64_info_t *info);
void sc64_init (void);
bool sc64_storage_init (drive_id_t drive);
bool sc64_storage_read (drive_id_t drive, void *buffer, uint32_t sector, uint32_t count);
bool sc64_storage_write (drive_id_t drive, const void *buffer, uint32_t sector, uint32_t count);
void sc64_uart_print_string (const char *text);


#endif

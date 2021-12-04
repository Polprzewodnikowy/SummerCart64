#ifndef SC64_H__
#define SC64_H__


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "sys.h"


#ifdef DEBUG
#include <assert.h>
#define LOG_I(args...)              {iprintf("\033[32m" args);}
#define LOG_E(args...)              {iprintf("\033[31m" args);}
#else
#define LOG_I(args...)
#define LOG_E(args...)
#define assert(expr)
#endif


typedef struct {
    io32_t SR_CMD;
    io32_t DATA[2];
    io32_t VERSION;
} sc64_regs_t;

#define SC64_BASE                   (0x1FFF0000)
#define SC64                        ((sc64_regs_t *) SC64_BASE)

#define SC64_SR_CMD_ERROR           (1 << 28)
#define SC64_SR_CPU_BUSY            (1 << 30)
#define SC64_SR_CPU_READY           (1 << 31)

#define SC64_CMD_CONFIG             ('C')
#define SC64_CMD_QUERY              ('Q')
#define SC64_CMD_DEBUG_RX_DATA      ('E')
#define SC64_CMD_DEBUG_RX_READY     ('A')
#define SC64_CMD_DEBUG_RX_BUSY      ('F')
#define SC64_CMD_DEBUG_TX_DATA      ('D')
#define SC64_CMD_DEBUG_TX_READY     ('S')

#define SC64_VERSION_2              (0x53437632)

#define SC64_DEBUG_WRITE_ADDRESS    (0x13BD8000UL)
#define SC64_DEBUG_READ_ADDRESS     (0x13BD0000UL)
#define SC64_DEBUG_MAX_SIZE         (32 * 1024)

#define SC64_DEBUG_TYPE_TEXT        (0x01)
#define SC64_DEBUG_TYPE_FSD_READ    (0xF1)
#define SC64_DEBUG_TYPE_FSD_WRITE   (0xF2)


typedef enum {
    CFG_ID_SCR,
    CFG_ID_SDRAM_SWITCH,
    CFG_ID_SDRAM_WRITABLE,
    CFG_ID_DD_ENABLE,
    CFG_ID_SAVE_TYPE,
    CFG_ID_CIC_SEED,
    CFG_ID_TV_TYPE,
    CFG_ID_SAVE_OFFEST,
    CFG_ID_DD_OFFEST,
    CFG_ID_BOOT_MODE,
    CFG_ID_FLASH_SIZE,
    CFG_ID_FLASH_READ,
    CFG_ID_FLASH_PROGRAM,
    CFG_ID_RECONFIGURE,
} cfg_id_t;

typedef enum {
    SAVE_TYPE_NONE = 0,
    SAVE_TYPE_EEPROM_4K = 1,
    SAVE_TYPE_EEPROM_16K = 2,
    SAVE_TYPE_SRAM = 3,
    SAVE_TYPE_FLASHRAM = 4,
    SAVE_TYPE_SRAM_BANKED = 5,
    SAVE_TYPE_FLASHRAM_PKST2 = 6,
} save_type_t;

typedef enum {
    TV_TYPE_PAL = 0,
    TV_TYPE_NTSC = 1,
    TV_TYPE_MPAL = 2,
    TV_TYPE_UNKNOWN = 3,
} tv_type_t;

typedef enum {
    BOOT_MODE_MENU = 0,
    BOOT_MODE_ROM = 1,
    BOOT_MODE_DD = 2,
    BOOT_MODE_DIRECT = 3,
} boot_mode_t;

typedef struct {
    bool dd_enabled;
    save_type_t save_type;
    uint16_t cic_seed;
    tv_type_t tv_type;
    io32_t *save_offset;
    io32_t *dd_offset;
    boot_mode_t boot_mode;
    char bootloader_version[32];
} sc64_info_t;


bool sc64_check_presence(void);
void sc64_wait_cpu_ready(void);
bool sc64_wait_cpu_busy(void);
bool sc64_perform_cmd(uint8_t cmd, uint32_t *args, uint32_t *result);
uint32_t sc64_get_config(cfg_id_t id);
void sc64_set_config(cfg_id_t id, uint32_t value);
void sc64_get_info(sc64_info_t *info);
void sc64_wait_usb_rx_ready (uint32_t *type, uint32_t *length);
void sc64_wait_usb_rx_busy (void);
void sc64_usb_rx_data (io32_t *address, uint32_t length);
void sc64_wait_usb_tx_ready(void);
void sc64_usb_tx_data(io32_t *address, uint32_t length);
void sc64_debug_write(uint8_t type, const void *data, uint32_t len);
void sc64_debug_fsd_read(const void *data, uint32_t sector, uint32_t count);
void sc64_debug_fsd_write(const void *data, uint32_t sector, uint32_t count);
void sc64_init(void);


#endif

#ifndef DRIVER_H__
#define DRIVER_H__


#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


// DMA

typedef enum {
    DMA_DIR_TO_SDRAM,
    DMA_DIR_FROM_SDRAM,
} e_dma_dir_t;

typedef enum {
    DMA_ID_USB = 0,
    DMA_ID_SD = 1,
} e_dma_id_t;

bool dma_start (uint32_t address, uint32_t length, e_dma_id_t id, e_dma_dir_t dir);
void dma_abort (void);
bool dma_busy (void);


// USB

bool usb_rx_byte (uint8_t *data);
bool usb_rx_word (uint32_t *data);
bool usb_tx_byte (uint8_t data);
bool usb_tx_word (uint32_t data);
void usb_flush_rx (void);
void usb_flush_tx (void);


// FLASHRAM

#define FLASHRAM_SIZE               (128 * 1024)
#define FLASHRAM_SECTOR_SIZE        (16 * 1024)
#define FLASHRAM_PAGE_SIZE          (128)
#define FLASHRAM_ERASE_VALUE        (0xFFFFFFFF)

typedef enum {
    FLASHRAM_OP_NONE,
    FLASHRAM_OP_ERASE_ALL,
    FLASHRAM_OP_ERASE_SECTOR,
    FLASHRAM_OP_WRITE_PAGE
} e_flashram_op_t;

e_flashram_op_t flashram_get_pending_operation (void);
uint32_t flashram_get_operation_length (e_flashram_op_t op);
void flashram_set_operation_done (void);
uint32_t flashram_get_page (void);
volatile uint32_t * flashram_get_page_buffer (void);


// CFG

#define CFG_SAVE_SIZE_EEPROM_4K     (512)
#define CFG_SAVE_SIZE_EEPROM_16K    (2048)
#define CFG_SAVE_SIZE_SRAM          (32 * 1024)
#define CFG_SAVE_SIZE_FLASHRAM      (128 * 1024)
#define CFG_SAVE_SIZE_SRAM_BANKED   (3 * 32 * 1024)

#define CFG_SAVE_OFFSET_PKST2       (0x01608000UL)

#define CFG_DEFAULT_DD_OFFSET       (0x03BE0000UL)

typedef enum {
    CFG_SAVE_TYPE_NONE = 0,
    CFG_SAVE_TYPE_EEPROM_4K = 1,
    CFG_SAVE_TYPE_EEPROM_16K = 2,
    CFG_SAVE_TYPE_SRAM = 3,
    CFG_SAVE_TYPE_FLASHRAM = 4,
    CFG_SAVE_TYPE_SRAM_BANKED = 5,
    CFG_SAVE_TYPE_FLASHRAM_PKST2 = 6,
} e_cfg_save_type_t;

uint32_t cfg_get_status (void);
void cfg_set_cpu_ready (bool enabled);
void cfg_set_sdram_switch (bool enabled);
void cfg_set_sdram_writable (bool enabled);
void cfg_set_usb_waiting (bool value);
void cfg_set_dd_enable (bool enabled);
void cfg_set_save_type (e_cfg_save_type_t save_type);
void cfg_set_save_offset (uint32_t offset);
uint32_t cfg_get_save_offset (void);
void cfg_set_dd_offset (uint32_t offset);
uint32_t cfg_get_dd_offset (void);
bool cfg_get_command (uint8_t *cmd, uint32_t *args);
void cfg_set_response (uint32_t *args, bool error);


#endif

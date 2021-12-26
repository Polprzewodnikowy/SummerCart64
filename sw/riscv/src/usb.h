#ifndef USB_H__
#define USB_H__


#include "sys.h"


typedef enum {
    INT_DBG_ID_IS_VIEWER = 0,
    INT_DBG_ID_DD_BLOCK = 1,
} internal_debug_id_t;


bool usb_debug_rx_ready (uint32_t *type, size_t *length);
bool usb_debug_rx_busy (void);
bool usb_debug_rx_data (uint32_t address, size_t length);
bool usb_debug_tx_ready (void);
bool usb_debug_tx_data (uint32_t address, size_t length);
void usb_debug_reset (void);
bool usb_internal_debug_tx_ready (void);
bool usb_internal_debug_tx_data (internal_debug_id_t id, uint32_t address, size_t length);
void usb_init (void);
void process_usb (void);


#endif

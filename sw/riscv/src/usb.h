#ifndef USB_H__
#define USB_H__


#include "sys.h"


bool usb_debug_rx_ready (uint32_t *type, size_t *length);
bool usb_debug_rx_busy (void);
bool usb_debug_rx_data (uint32_t address, size_t length);
bool usb_debug_tx_ready (void);
bool usb_debug_tx_data (uint32_t address, size_t length);
void usb_debug_reset (void);
void usb_init (void);
void process_usb (void);


#endif

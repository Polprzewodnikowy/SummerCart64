#ifndef USB_H__
#define USB_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum packet_cmd {
    PACKET_CMD_DD_REQUEST = 'D',
    PACKET_CMD_ISV_OUTPUT = 'I',
} usb_packet_cmd_e;


typedef struct usb_tx_info {
    uint8_t cmd;
    uint32_t data_length;
    uint32_t data[4];
    uint32_t dma_length;
    uint32_t dma_address;
} usb_tx_info_t;


bool usb_enqueue_packet (usb_tx_info_t *info);
void usb_init (void);
void usb_process (void);


#endif

#ifndef USB_H__
#define USB_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum packet_cmd {
    PACKET_CMD_DD_REQUEST = 'D',
    PACKET_CMD_DEBUG_OUTPUT = 'U',
    PACKET_CMD_ISV_OUTPUT = 'I',
    PACKET_CMD_UPDATE_STATUS = 'F',
} usb_packet_cmd_e;


typedef struct usb_tx_info {
    uint8_t cmd;
    uint32_t data_length;
    uint32_t data[4];
    uint32_t dma_length;
    uint32_t dma_address;
    void (*done_callback)(void);
} usb_tx_info_t;


void usb_create_packet (usb_tx_info_t *info, usb_packet_cmd_e cmd);
bool usb_enqueue_packet (usb_tx_info_t *info);
bool usb_prepare_read (uint32_t *args);
void usb_get_read_info (uint32_t *args);
void usb_init (void);
void usb_process (void);


#endif

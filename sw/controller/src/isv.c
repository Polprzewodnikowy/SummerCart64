#include "fpga.h"
#include "isv.h"
#include "usb.h"


#define ISV_READ_POINTER_ADDRESS    (0x03FF0014)
#define ISV_BUFFER_ADDRESS          (0x03FF0020)
#define ISV_BUFFER_SIZE             ((64 * 1024) - 0x20)


struct process {
    bool enabled;
    uint32_t current_read_pointer;
};

static struct process p;


static uint32_t isv_get_read_pointer (void) {
    uint32_t read_pointer;
    fpga_mem_read(ISV_READ_POINTER_ADDRESS, 4, (uint8_t *) (&read_pointer));
    return (
        (read_pointer & 0x000000FF) << 24 |
        (read_pointer & 0x0000FF00) << 8 |
        (read_pointer & 0x00FF0000) >> 8 |
        (read_pointer & 0xFF000000) >> 24
    );
}


void isv_set_enabled (bool enabled) {
    if (enabled) {
        p.enabled = true;
        p.current_read_pointer = 0;
    } else {
        p.enabled = false;
    }
}

bool isv_get_enabled (void) {
    return p.enabled;
}

void isv_init (void) {
    p.enabled = false;
}

void isv_process (void) {
    if (p.enabled) {
        uint32_t read_pointer = isv_get_read_pointer();
        if (read_pointer < ISV_BUFFER_SIZE && read_pointer != p.current_read_pointer) {
            bool wrap = read_pointer < p.current_read_pointer;

            uint32_t length = ((wrap ? ISV_BUFFER_SIZE : read_pointer) - p.current_read_pointer);
            uint32_t offset = ISV_BUFFER_ADDRESS + p.current_read_pointer;

            usb_tx_info_t packet_info;
            packet_info.cmd = PACKET_CMD_ISV_OUTPUT;
            packet_info.data_length = 0;
            packet_info.dma_length = length;
            packet_info.dma_address = offset;
            if (usb_enqueue_packet(&packet_info)) {
                p.current_read_pointer = wrap ? 0 : read_pointer;
            }
        }
    }
}

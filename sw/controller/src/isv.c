#include <stdint.h>
#include "fpga.h"
#include "isv.h"
#include "usb.h"


#define ISV_READ_POINTER_ADDRESS    (0x03FF0004)
#define ISV_WRITE_POINTER_ADDRESS   (0x03FF0014)
#define ISV_BUFFER_ADDRESS          (0x03FF0020)
#define ISV_BUFFER_SIZE             ((64 * 1024) - 0x20)


struct process {
    bool enabled;
    uint32_t current_read_pointer;
};


static struct process p;


static void isv_set_pointer (uint32_t address, uint32_t data) {
    data = SWAP32(data);
    fpga_mem_write(address, 4, (uint8_t *) (&data));
}

static uint32_t isv_get_pointer (uint32_t address) {
    uint32_t data;
    fpga_mem_read(address, 4, (uint8_t *) (&data));
    return SWAP32(data);
}

static void isv_update_read_pointer (void) {
    isv_set_pointer(ISV_READ_POINTER_ADDRESS, p.current_read_pointer);
}


void isv_set_enabled (bool enabled) {
    if (enabled) {
        p.enabled = true;
        p.current_read_pointer = 0;
        isv_set_pointer(ISV_READ_POINTER_ADDRESS, 0);
        isv_set_pointer(ISV_WRITE_POINTER_ADDRESS, 0);
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
        uint32_t write_pointer = isv_get_pointer(ISV_WRITE_POINTER_ADDRESS);

        if (p.current_read_pointer == write_pointer) {
            return;
        }
        if (write_pointer >= ISV_BUFFER_SIZE) {
            return;
        }

        bool wrap = write_pointer < p.current_read_pointer;
        uint32_t length = (wrap ? ISV_BUFFER_SIZE : write_pointer) - p.current_read_pointer;
        uint32_t offset = ISV_BUFFER_ADDRESS + p.current_read_pointer;

        usb_tx_info_t packet_info;
        usb_create_packet(&packet_info, PACKET_CMD_ISV_OUTPUT);
        packet_info.dma_length = length;
        packet_info.dma_address = offset;
        packet_info.done_callback = isv_update_read_pointer;
        if (usb_enqueue_packet(&packet_info)) {
            p.current_read_pointer = wrap ? 0 : write_pointer;
        }
    }
}

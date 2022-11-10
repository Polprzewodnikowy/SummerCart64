#include <stdint.h>
#include "fpga.h"
#include "isv.h"
#include "usb.h"


#define ISV_TOKEN                   (0x49533634)

#define ISV_SETUP_TOKEN_ADDRESS     (0x00000100)
#define ISV_SETUP_OFFSET_ADDRESS    (0x00000104)
#define ISV_SETUP_READY_ADDRESS     (0x0000010C)

#define ISV_TOKEN_OFFSET            (0x00000000)
#define ISV_READ_POINTER_OFFSET     (0x00000004)
#define ISV_WRITE_POINTER_OFFSET    (0x00000014)
#define ISV_BUFFER_OFFSET           (0x00000020)

#define ISV_BUFFER_SIZE             ((64 * 1024) - ISV_BUFFER_OFFSET)


struct process {
    bool ready;
    uint32_t address;
    uint32_t next_read_pointer;
};


static struct process p;


static void isv_set_value (uint32_t address, uint32_t data) {
    data = SWAP32(data);
    fpga_mem_write(address, 4, (uint8_t *) (&data));
}

static uint32_t isv_get_value (uint32_t address) {
    uint32_t data;
    fpga_mem_read(address, 4, (uint8_t *) (&data));
    return SWAP32(data);
}

static void isv_update_read_pointer (void) {
    p.ready = true;
    isv_set_value(p.address + ISV_READ_POINTER_OFFSET, p.next_read_pointer);
}


bool isv_set_address (uint32_t address) {
    if ((address > 0x03FF0000) || (address % 4)) {
        return true;
    }
    p.address = address;
    return false;
}

uint32_t isv_get_address (void) {
    return p.address;
}

void isv_init (void) {
    p.address = 0;
    p.ready = true;
}

void isv_process (void) {
    if ((p.address != 0) && p.ready) {
        if (isv_get_value(ISV_SETUP_TOKEN_ADDRESS) == ISV_TOKEN) {
            isv_set_value(ISV_SETUP_TOKEN_ADDRESS, 0);
            isv_set_value(ISV_SETUP_OFFSET_ADDRESS, (p.address | 0x10000000));
            isv_set_value(ISV_SETUP_READY_ADDRESS, ISV_TOKEN);
            return;
        }

        if (isv_get_value(p.address + ISV_TOKEN_OFFSET) != ISV_TOKEN) {
            return;
        }

        uint32_t read_pointer = isv_get_value(p.address + ISV_READ_POINTER_OFFSET);
        if (read_pointer >= ISV_BUFFER_SIZE) {
            return;
        }

        uint32_t write_pointer = isv_get_value(p.address + ISV_WRITE_POINTER_OFFSET);
        if (write_pointer >= ISV_BUFFER_SIZE) {
            return;
        }

        if (read_pointer == write_pointer) {
            return;
        }

        bool wrap = write_pointer < read_pointer;
        uint32_t length = (wrap ? ISV_BUFFER_SIZE : write_pointer) - read_pointer;
        uint32_t offset = p.address + ISV_BUFFER_OFFSET + read_pointer;

        usb_tx_info_t packet_info;
        usb_create_packet(&packet_info, PACKET_CMD_ISV_OUTPUT);
        packet_info.dma_length = length;
        packet_info.dma_address = offset;
        packet_info.done_callback = isv_update_read_pointer;
        if (usb_enqueue_packet(&packet_info)) {
            p.ready = false;
            p.next_read_pointer = wrap ? 0 : write_pointer;
        }
    }
}

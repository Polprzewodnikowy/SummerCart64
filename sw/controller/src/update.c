#include "fpga.h"
#include "hw.h"
#include "update.h"
#include "usb.h"
#include "vendor.h"


#define UPDATE_MAGIC_START  (0x54535055)


typedef enum {
    UPDATE_STATUS_START = 1,
    UPDATE_STATUS_MCU_START = 2,
    UPDATE_STATUS_MCU_DONE = 3,
    UPDATE_STATUS_FPGA_START = 4,
    UPDATE_STATUS_FPGA_DONE = 5,
    UPDATE_STATUS_DONE = 6,
    UPDATE_STATUS_ERROR = 0xFF,
} update_status_t;

typedef enum {
    CHUNK_ID_UPDATE_INFO = 1,
    CHUNK_ID_MCU_DATA = 2,
    CHUNK_ID_FPGA_DATA = 3,
} chunk_id_t;


static loader_parameters_t parameters;
static const uint8_t update_token[16] = "SC64 Update v2.0";
static uint8_t status_data[12] = {
    'P', 'K', 'T', PACKET_CMD_UPDATE_STATUS,
    0, 0, 0, 4,
    0, 0, 0, UPDATE_STATUS_START,
};


static uint32_t update_checksum (uint32_t address, uint32_t length) {
    uint32_t remaining = length;
    uint32_t block_size;
    uint32_t checksum = 0;
    uint8_t buffer[32];

    hw_crc32_reset();

    for (uint32_t i = 0; i < length; i += sizeof(buffer)) {
        block_size = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        fpga_mem_read(address, block_size, buffer);
        checksum = hw_crc32_calculate(buffer, block_size);
        remaining -= block_size;
    }

    return checksum;
}

static uint32_t update_write_token (uint32_t *address) {
    uint32_t length = sizeof(update_token);
    fpga_mem_write(*address, sizeof(update_token), (uint8_t *) (update_token));
    *address += length;
    return length;
}

static uint32_t update_prepare_chunk (uint32_t *address, chunk_id_t chunk_id) {
    uint32_t id = (uint32_t) (chunk_id);
    uint32_t length = sizeof(id) + (2 * sizeof(uint32_t));
    fpga_mem_write(*address, sizeof(id), (uint8_t *) (&id));
    *address += length;
    return length;
}

static uint32_t update_finalize_chunk (uint32_t *address, uint32_t length) {
    uint32_t checksum = update_checksum(*address, length);
    fpga_mem_write(*address - (2 * sizeof(uint32_t)), sizeof(length), (uint8_t *) (&length));
    fpga_mem_write(*address - sizeof(uint32_t), sizeof(checksum), (uint8_t *) (&checksum));
    *address += length;
    return length;
}

static bool update_check_token (uint32_t *address) {
    uint8_t buffer[sizeof(update_token)];
    fpga_mem_read(*address, sizeof(update_token), buffer);
    for (int i = 0; i < sizeof(update_token); i++) {
        if (buffer[i] != update_token[i]) {
            return true;
        }
    }
    *address += sizeof(update_token);
    return false;
}

static bool update_get_chunk (uint32_t *address, chunk_id_t *chunk_id, uint32_t *data_address, uint32_t *data_length) {
    uint32_t id;
    uint32_t checksum;
    fpga_mem_read(*address, sizeof(id), (uint8_t *) (&id));
    *address += sizeof(id);
    fpga_mem_read(*address, sizeof(*data_length), (uint8_t *) (data_length));
    *address += sizeof(*data_length);
    fpga_mem_read(*address, sizeof(checksum), (uint8_t *) (&checksum));
    *address += sizeof(checksum);
    *data_address = *address;
    *address += *data_length;
    if (checksum != update_checksum(*data_address, *data_length)) {
        return true;
    }
    return false;
}

static void update_blink_led (uint32_t on, uint32_t off, int repeat) {
    for (int i = 0; i < repeat; i++) {
        hw_gpio_set(GPIO_ID_LED);
        hw_delay_ms(on);
        hw_gpio_reset(GPIO_ID_LED);
        hw_delay_ms(off);
    }
}

static void update_status_notify (update_status_t status) {
    status_data[sizeof(status_data) - 1] = (uint8_t) (status);
    for (int i = 0; i < sizeof(status_data); i++) {
        while (!(fpga_usb_status_get() & USB_STATUS_TXE));
        fpga_usb_push(status_data[i]);
    }
    fpga_reg_set(REG_USB_SCR, USB_SCR_WRITE_FLUSH);
    if (status != UPDATE_STATUS_ERROR) {
        update_blink_led(100, 250, status);
        hw_delay_ms(1000);
    } else {
        update_blink_led(1000, 1000, 30);
    }
}


update_error_t update_backup (uint32_t address, uint32_t *length) {
    hw_flash_t buffer;
    uint32_t mcu_length;
    uint32_t fpga_length;

    *length += update_write_token(&address);

    *length += update_prepare_chunk(&address, CHUNK_ID_MCU_DATA);
    mcu_length = hw_flash_size();
    for (uint32_t offset = 0; offset < mcu_length; offset += sizeof(hw_flash_t)) {
        buffer = hw_flash_read(offset);
        fpga_mem_write(address + offset, sizeof(hw_flash_t), (uint8_t *) (&buffer));
    }
    *length += update_finalize_chunk(&address, mcu_length);

    *length += update_prepare_chunk(&address, CHUNK_ID_FPGA_DATA);
    if (vendor_backup(address, &fpga_length) != VENDOR_OK) {
        return UPDATE_ERROR_READ;
    }
    *length += update_finalize_chunk(&address, fpga_length);

    return UPDATE_OK;
}

update_error_t update_prepare (uint32_t address, uint32_t length) {
    uint32_t end_address = (address + length);
    chunk_id_t id;
    uint32_t data_address;
    uint32_t data_length;

    if (update_check_token(&address)) {
        return UPDATE_ERROR_TOKEN;
    }

    parameters.mcu_address = 0;
    parameters.mcu_length = 0;
    parameters.fpga_address = 0;
    parameters.fpga_length = 0;

    while (address < end_address) {
        if (update_get_chunk(&address, &id, &data_address, &data_length)) {
            return UPDATE_ERROR_CHECKSUM;
        }

        switch (id) {
            case CHUNK_ID_UPDATE_INFO:
                break;

            case CHUNK_ID_MCU_DATA:
                if (data_length > hw_flash_size()) {
                    return UPDATE_ERROR_SIZE;
                }
                parameters.mcu_address = data_address;
                parameters.mcu_length = data_length;
                break;

            case CHUNK_ID_FPGA_DATA:
                if (data_length > vendor_flash_size()) {
                    return UPDATE_ERROR_SIZE;
                }
                parameters.fpga_address = data_address;
                parameters.fpga_length = data_length;
                break;

            default:
                return UPDATE_ERROR_UNKNOWN_CHUNK;
        }
    }

    return UPDATE_OK;
}

void update_start (void) {
    parameters.magic = UPDATE_MAGIC_START;
    hw_loader_reset(&parameters);
}

bool update_check (void) {
    hw_loader_get_parameters(&parameters);
    return (parameters.magic == UPDATE_MAGIC_START);
}

void update_perform (void) {
    hw_flash_t buffer;

    update_status_notify(UPDATE_STATUS_START);

    if (parameters.mcu_length != 0) {
        update_status_notify(UPDATE_STATUS_MCU_START);

        hw_flash_erase();

        for (uint32_t offset = 0; offset < parameters.mcu_length; offset += sizeof(hw_flash_t)) {
            fpga_mem_read(parameters.mcu_address + offset, sizeof(hw_flash_t), (uint8_t *) (&buffer));
            hw_flash_program(offset, buffer);

            if (hw_flash_read(offset) != buffer) {
                update_status_notify(UPDATE_STATUS_ERROR);
                while (1);  // TODO: jump to STM32 bootloader?
            }
        }

        update_status_notify(UPDATE_STATUS_MCU_DONE);
    }

    if (parameters.fpga_length != 0) {
        update_status_notify(UPDATE_STATUS_FPGA_START);

        if (vendor_update(parameters.fpga_address, parameters.fpga_length) != VENDOR_OK) {
            update_status_notify(UPDATE_STATUS_ERROR);
            while (1);  // TODO: jump to STM32 bootloader?
        }

        update_status_notify(UPDATE_STATUS_FPGA_DONE);
    }

    update_status_notify(UPDATE_STATUS_DONE);

    vendor_reconfigure();

    parameters.magic = 0;
    hw_loader_reset(&parameters);
}

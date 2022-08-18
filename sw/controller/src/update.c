#include "flash.h"
#include "fpga.h"
#include "hw.h"
#include "update.h"
#include "usb.h"
#include "vendor.h"


#define UPDATE_MAGIC_START  (0x54535055UL)
#define BOOTLOADER_ADDRESS  (0x04E00000UL)
#define BOOTLOADER_LENGTH   (0x001E0000UL)


typedef enum {
    UPDATE_STATUS_MCU = 1,
    UPDATE_STATUS_FPGA = 2,
    UPDATE_STATUS_BOOTLOADER = 3,
    UPDATE_STATUS_DONE = 0x80,
    UPDATE_STATUS_ERROR = 0xFF,
} update_status_t;

typedef enum {
    CHUNK_ID_UPDATE_INFO = 1,
    CHUNK_ID_MCU_DATA = 2,
    CHUNK_ID_FPGA_DATA = 3,
    CHUNK_ID_BOOTLOADER_DATA = 4,
} chunk_id_t;


static loader_parameters_t parameters;
static const uint8_t update_token[16] = "SC64 Update v2.0";
static uint8_t status_data[12] = {
    'P', 'K', 'T', PACKET_CMD_UPDATE_STATUS,
    0, 0, 0, 4,
    0, 0, 0, UPDATE_STATUS_ERROR,
};


static uint32_t update_align (uint32_t value) {
    if ((value % 16) != 0) {
        value += (16 - (value % 16));
    }
    return value;
}

static uint32_t update_checksum (uint32_t address, uint32_t length) {
    uint8_t buffer[32];
    uint32_t block_size;
    uint32_t checksum = 0;
    hw_crc32_reset();
    while (length > 0) {
        block_size = (length > sizeof(buffer)) ? sizeof(buffer) : length;
        fpga_mem_read(address, block_size, buffer);
        checksum = hw_crc32_calculate(buffer, block_size);
        address += block_size;
        length -= block_size;
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
    uint32_t length = (4 * sizeof(uint32_t));
    fpga_mem_write(*address, sizeof(id), (uint8_t *) (&id));
    *address += length;
    return length;
}

static uint32_t update_finalize_chunk (uint32_t *address, uint32_t length) {
    uint32_t chunk_length = ((4 * sizeof(uint32_t)) + length);
    uint32_t aligned_chunk_length = update_align(chunk_length);
    uint32_t aligned_length = aligned_chunk_length - (2 * sizeof(uint32_t));
    uint32_t checksum = update_checksum(*address, length);
    fpga_mem_write(*address - (3 * sizeof(uint32_t)), sizeof(aligned_length), (uint8_t *) (&aligned_length));
    fpga_mem_write(*address - (2 * sizeof(uint32_t)), sizeof(checksum), (uint8_t *) (&checksum));
    fpga_mem_write(*address - sizeof(uint32_t), sizeof(length), (uint8_t *) (&length));
    length += (aligned_chunk_length - chunk_length);
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
    uint32_t chunk_length;
    uint32_t checksum;
    fpga_mem_read(*address, sizeof(id), (uint8_t *) (&id));
    *chunk_id = (chunk_id_t) (id);
    *address += sizeof(id);
    fpga_mem_read(*address, sizeof(chunk_length), (uint8_t *) (&chunk_length));
    *address += sizeof(chunk_length);
    fpga_mem_read(*address, sizeof(checksum), (uint8_t *) (&checksum));
    *address += sizeof(checksum);
    fpga_mem_read(*address, sizeof(*data_length), (uint8_t *) (data_length));
    *address += sizeof(*data_length);
    *data_address = *address;
    *address += (chunk_length - (2 * sizeof(uint32_t)));
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
    if (status == UPDATE_STATUS_DONE) {
        update_blink_led(15, 85, 10);
    } else if (status == UPDATE_STATUS_ERROR) {
        update_blink_led(1000, 1000, 30);
    } else {
        update_blink_led(15, 185, 2);
        hw_delay_ms(500);
    }
}


update_error_t update_backup (uint32_t address, uint32_t *length) {
    uint32_t mcu_length;
    uint32_t fpga_length;
    uint32_t bootloader_length;

    *length = update_write_token(&address);

    *length += update_prepare_chunk(&address, CHUNK_ID_MCU_DATA);
    mcu_length = hw_flash_size();
    for (uint32_t offset = 0; offset < mcu_length; offset += sizeof(hw_flash_t)) {
        hw_flash_t buffer = hw_flash_read(offset);
        fpga_mem_write(address + offset, sizeof(hw_flash_t), (uint8_t *) (&buffer));
    }
    *length += update_finalize_chunk(&address, mcu_length);

    *length += update_prepare_chunk(&address, CHUNK_ID_FPGA_DATA);
    if (vendor_backup(address, &fpga_length) != VENDOR_OK) {
        return UPDATE_ERROR_READ;
    }
    *length += update_finalize_chunk(&address, fpga_length);

    *length += update_prepare_chunk(&address, CHUNK_ID_BOOTLOADER_DATA);
    bootloader_length = BOOTLOADER_LENGTH;
    for (uint32_t offset = 0; offset < bootloader_length; offset += FPGA_MAX_MEM_TRANSFER) {
        fpga_mem_copy(BOOTLOADER_ADDRESS + offset, address + offset, FPGA_MAX_MEM_TRANSFER);
    }
    *length += update_finalize_chunk(&address, bootloader_length);

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

    parameters.flags = 0;
    parameters.mcu_address = 0;
    parameters.fpga_address = 0;
    parameters.bootloader_address = 0;

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
                parameters.flags |= LOADER_FLAGS_UPDATE_MCU;
                parameters.mcu_address = data_address;
                break;

            case CHUNK_ID_FPGA_DATA:
                if (data_length > vendor_flash_size()) {
                    return UPDATE_ERROR_SIZE;
                }
                parameters.flags |= LOADER_FLAGS_UPDATE_FPGA;
                parameters.fpga_address = data_address;
                break;

            case CHUNK_ID_BOOTLOADER_DATA:
                if (data_length > BOOTLOADER_LENGTH) {
                    return UPDATE_ERROR_SIZE;
                }
                parameters.flags |= LOADER_FLAGS_UPDATE_BOOTLOADER;
                parameters.bootloader_address = data_address;
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
    uint32_t length;

    if (parameters.flags & LOADER_FLAGS_UPDATE_MCU) {
        hw_flash_t buffer;
        update_status_notify(UPDATE_STATUS_MCU);
        fpga_mem_read(parameters.mcu_address - 4, sizeof(length), (uint8_t *) (&length));
        hw_flash_erase();
        for (uint32_t offset = 0; offset < length; offset += sizeof(hw_flash_t)) {
            fpga_mem_read(parameters.mcu_address + offset, sizeof(hw_flash_t), (uint8_t *) (&buffer));
            hw_flash_program(offset, buffer);
            if (hw_flash_read(offset) != buffer) {
                update_status_notify(UPDATE_STATUS_ERROR);
                while (1);
            }
        }
    }

    if (parameters.flags & LOADER_FLAGS_UPDATE_FPGA) {
        update_status_notify(UPDATE_STATUS_FPGA);
        fpga_mem_read(parameters.fpga_address - 4, sizeof(length), (uint8_t *) (&length));
        if (vendor_update(parameters.fpga_address, length) != VENDOR_OK) {
            update_status_notify(UPDATE_STATUS_ERROR);
            while (1);
        }
    }

    if (parameters.flags & LOADER_FLAGS_UPDATE_BOOTLOADER) {
        update_status_notify(UPDATE_STATUS_BOOTLOADER);
        fpga_mem_read(parameters.bootloader_address - 4, sizeof(length), (uint8_t *) (&length));
        for (uint32_t offset = 0; offset < BOOTLOADER_LENGTH; offset += FLASH_ERASE_BLOCK_SIZE) {
            flash_erase_block(BOOTLOADER_ADDRESS + offset);
        }
        for (uint32_t offset = 0; offset < length; offset += FPGA_MAX_MEM_TRANSFER) {
            fpga_mem_copy(parameters.bootloader_address + offset, BOOTLOADER_ADDRESS + offset, FPGA_MAX_MEM_TRANSFER);
        }
    }

    update_status_notify(UPDATE_STATUS_DONE);

    vendor_reconfigure();

    parameters.magic = 0;
    hw_loader_reset(&parameters);
}

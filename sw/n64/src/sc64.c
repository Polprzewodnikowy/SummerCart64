#include "sc64.h"


extern char header_text_info __attribute__((section(".data")));


bool sc64_check_presence (void) {
    uint32_t version = pi_io_read(&SC64->VERSION);
    return (version == SC64_VERSION_2);
}

void sc64_wait_cpu_ready (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (!(sr & SC64_SR_CPU_READY));
}

bool sc64_wait_cpu_busy (void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (sr & SC64_SR_CPU_BUSY);
    return sr & SC64_SR_CMD_ERROR;
}

bool sc64_perform_cmd (uint8_t cmd, uint32_t *args, uint32_t *result) {
    if (args != NULL) {
        pi_io_write(&SC64->DATA[0], args[0]);
        pi_io_write(&SC64->DATA[1], args[1]);
    }
    pi_io_write(&SC64->SR_CMD, ((uint32_t) (cmd)) & 0xFF);
    bool error = sc64_wait_cpu_busy();
    if (result != NULL) {
        result[0] = pi_io_read(&SC64->DATA[0]);
        result[1] = pi_io_read(&SC64->DATA[1]);
    }
    return error;
}

uint32_t sc64_get_config (cfg_id_t id) {
    uint32_t args[2] = { id, 0 };
    uint32_t result[2];
    sc64_perform_cmd(SC64_CMD_QUERY, args, result);
    return result[1];
}

void sc64_set_config (cfg_id_t id, uint32_t value) {
    uint32_t args[2] = { id, value };
    sc64_perform_cmd(SC64_CMD_CONFIG, args, NULL);
}

void sc64_get_info (sc64_info_t *info) {
    io32_t *src = (io32_t *) UNCACHED(&header_text_info);
    uint32_t *dst = (uint32_t *) info->bootloader_version;
    
    bool sdram_switched = sc64_get_config(CFG_ID_SDRAM_SWITCH);

    if (sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, false);
    }

    for (int i = 0; i < sizeof(info->bootloader_version); i += sizeof(uint32_t)) {
        *dst++ = pi_io_read(src++);
    }

    if (sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, true);
    }

    info->dd_enabled = (bool) sc64_get_config(CFG_ID_DD_ENABLE);
    info->save_type = (save_type_t) sc64_get_config(CFG_ID_SAVE_TYPE);
    info->cic_seed = (uint16_t) sc64_get_config(CFG_ID_CIC_SEED);
    info->tv_type = (tv_type_t) sc64_get_config(CFG_ID_TV_TYPE);
    info->save_location = (io32_t *) (0x10000000 | sc64_get_config(CFG_ID_SAVE_OFFEST));
    info->ddipl_location = (io32_t *) (0x10000000 | sc64_get_config(CFG_ID_DDIPL_OFFEST));
    info->boot_mode = (boot_mode_t) sc64_get_config(CFG_ID_BOOT_MODE);
}

void sc64_wait_usb_rx_ready (uint32_t *type, uint32_t *length) {
    uint32_t result[2];
    do {
        sc64_perform_cmd(SC64_CMD_DEBUG_RX_READY, NULL, result);
    } while (result[0] == 0 && result[1] == 0);
    *type = result[0];
    *length = result[1];
}

void sc64_wait_usb_rx_busy (void) {
    uint32_t result[2];
    do {
        sc64_perform_cmd(SC64_CMD_DEBUG_RX_BUSY, NULL, result);
    } while (result[0]);
}

void sc64_usb_rx_data (io32_t *address, uint32_t length) {
    uint32_t args[2] = { (uint32_t) (address), ALIGN(length, 2) };
    sc64_perform_cmd(SC64_CMD_DEBUG_RX_DATA, args, NULL);
}

void sc64_wait_usb_tx_ready (void) {
    uint32_t result[2];
    do {
        sc64_perform_cmd(SC64_CMD_DEBUG_TX_READY, NULL, result);
    } while (!result[0]);
}

void sc64_usb_tx_data (io32_t *address, uint32_t length) {
    uint32_t args[2] = { (uint32_t) (address), ALIGN(length, 2) };
    sc64_perform_cmd(SC64_CMD_DEBUG_TX_DATA, args, NULL);
}

void sc64_debug_write (uint8_t type, const void *data, uint32_t len) {
    char *dma = "DMA@";
    char *cmp = "CMPH";

    io32_t *sdram = (io32_t *) (SC64_DEBUG_WRITE_ADDRESS);

    uint32_t *src = (uint32_t *) (data);
    io32_t *dst = sdram;

    uint32_t copy_length = ALIGN(len, 4);

    sc64_wait_usb_tx_ready();

    bool writable = sc64_get_config(CFG_ID_SDRAM_WRITABLE);
    bool sdram_switched = sc64_get_config(CFG_ID_SDRAM_SWITCH);

    if (!writable) {
        sc64_set_config(CFG_ID_SDRAM_WRITABLE, true);
    }
    if (!sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, true);
    }

    pi_io_write(dst++, *((uint32_t *) (dma)));
    pi_io_write(dst++, (type << 24) | len);

    while (src < ((uint32_t *) (data + copy_length))) {
        pi_io_write(dst++, *src++);
        if (dst >= (io32_t *) ((void *) (sdram) + SC64_DEBUG_MAX_SIZE)) {
            sc64_usb_tx_data(sdram, (dst - sdram) * sizeof(uint32_t));
            sc64_wait_usb_tx_ready();
            dst = sdram;
        }
    }

    pi_io_write(dst++, *((uint32_t *) (cmp)));

    if (!writable) {
        sc64_set_config(CFG_ID_SDRAM_WRITABLE, false);
    }
    if (!sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, false);
    }

    sc64_usb_tx_data(sdram, (dst - sdram) * sizeof(uint32_t));
}

void sc64_debug_fsd_read (const void *data, uint32_t sector, uint32_t count) {
    uint32_t type;
    uint32_t length;

    io32_t *sdram = (io32_t *) (SC64_DEBUG_READ_ADDRESS);

    io32_t *src = sdram;
    uint32_t *dst = (uint32_t *) (data);

    uint32_t read_length = count * 512;

    sc64_debug_write(SC64_DEBUG_ID_FSD_SECTOR, &sector, 4);
    sc64_debug_write(SC64_DEBUG_ID_FSD_READ, &read_length, 4);
    sc64_wait_usb_rx_ready(&type, &length);
    sc64_usb_rx_data(sdram, length);
    sc64_wait_usb_rx_busy();

    uint32_t copy_length = ALIGN(length, 4);

    bool sdram_switched = sc64_get_config(CFG_ID_SDRAM_SWITCH);

    if (!sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, true);
    }

    for (int i = 0; i < copy_length; i += 4) {
        *dst++ = pi_io_read(src++);
    }

    if (!sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, false);
    }
}

void sc64_debug_fsd_write (const void *data, uint32_t sector, uint32_t count) {
    sc64_debug_write(SC64_DEBUG_ID_FSD_SECTOR, &sector, 4);
    sc64_debug_write(SC64_DEBUG_ID_FSD_WRITE, data, count * 512);
}

void sc64_init (void) {
    while (!sc64_check_presence());
    sc64_wait_cpu_ready();
    sc64_set_config(CFG_ID_SDRAM_SWITCH, true);
}

#include <string.h>
#include "sc64.h"


bool sc64_check_presence(void) {
    uint32_t version = pi_io_read(&SC64->VERSION);
    return (version == SC64_VERSION_2);
}

void sc64_wait_cpu_ready(void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (!(sr & SC64_SR_CPU_READY));
}

bool sc64_wait_cpu_busy(void) {
    uint32_t sr;
    do {
        sr = pi_io_read(&SC64->SR_CMD);
    } while (sr & SC64_SR_CPU_BUSY);
    return sr & SC64_SR_CMD_ERROR;
}

bool sc64_perform_cmd(uint8_t cmd, uint32_t *args, uint32_t *result) {
    pi_io_write(&SC64->DATA[0], args[0]);
    pi_io_write(&SC64->DATA[1], args[1]);
    pi_io_write(&SC64->SR_CMD, (uint32_t) cmd);
    bool error = sc64_wait_cpu_busy();
    if (result != NULL) {
        result[0] = pi_io_read(&SC64->DATA[0]);
        result[1] = pi_io_read(&SC64->DATA[1]);
    }
    return error;
}

uint32_t sc64_get_config(cfg_id_t id) {
    uint32_t args[2] = { id, 0 };
    uint32_t result[2];
    sc64_perform_cmd(SC64_CMD_QUERY, args, result);
    return result[1];
}

void sc64_set_config(cfg_id_t id, uint32_t value) {
    uint32_t args[2] = { id, value };
    sc64_perform_cmd(SC64_CMD_CONFIG, args, NULL);
}

void sc64_get_info(info_t *info) {
    io32_t *src = UNCACHED(SC64_BL_VERSION_BASE);
    uint32_t *dst = (uint32_t *) info->bootloader_version;

    sc64_set_config(CFG_ID_SDRAM_SWITCH, false);

    for (int i = 0; i < sizeof(info->bootloader_version); i += sizeof(uint32_t)) {
        *dst++ = pi_io_read(src++);
    }

    sc64_set_config(CFG_ID_SDRAM_SWITCH, true);

    info->dd_enabled = (bool) sc64_get_config(CFG_ID_DD_ENABLE);
    info->save_type = (save_type_t) sc64_get_config(CFG_ID_SAVE_TYPE);
    info->cic_seed = (uint8_t) sc64_get_config(CFG_ID_CIC_SEED);
    info->tv_type = (tv_type_t) sc64_get_config(CFG_ID_TV_TYPE);
    info->save_offset = (io32_t *) sc64_get_config(CFG_ID_SAVE_OFFEST);
    info->dd_offset = (io32_t *) sc64_get_config(CFG_ID_DD_OFFEST);
    info->boot_mode = (boot_mode_t) sc64_get_config(CFG_ID_BOOT_MODE);
}

void sc64_wait_usb_tx_ready(void) {
    uint32_t args[2] = { 0, 0 };
    uint32_t result[2];
    do {
        sc64_perform_cmd(SC64_CMD_DEBUG_TX_READY, args, result);
    } while (!result[0]);
}

void sc64_usb_tx_data(io32_t *address, uint32_t length) {
    uint32_t args[2] = { (uint32_t) (address), length };
    sc64_perform_cmd(SC64_CMD_DEBUG_TX_DATA, args, NULL);
}

void sc64_debug_write(uint8_t type, const void *data, uint32_t len) {
    char *dma = "DMA@";
    char *cmp = "CMPH";

    io32_t *sdram = UNCACHED(SC64_DEBUG_WRITE_ADDRESS);

    io32_t *src = (io32_t *) (data);
    io32_t *dst = sdram;

    uint32_t copy_length = ALIGN(len, 4);
    uint32_t transfer_length = 4 + 4 + copy_length + 4;

    bool writable = sc64_get_config(CFG_ID_SDRAM_WRITABLE);

    sc64_wait_usb_tx_ready();

    if (!writable) {
        sc64_set_config(CFG_ID_SDRAM_WRITABLE, true);
    }

    pi_io_write(dst++, *((uint32_t *) (dma)));
    pi_io_write(dst++, (type << 24) | len);

    for (uint32_t i = 0; i < copy_length; i += 4) {
        pi_io_write(dst++, *src++);
    }

    pi_io_write(dst++, *((uint32_t *) (cmp)));

    if (!writable) {
        sc64_set_config(CFG_ID_SDRAM_WRITABLE, false);
    }

    sc64_usb_tx_data(sdram, transfer_length);
}

void sc64_debug_print(const char *text) {
    sc64_debug_write(SC64_DEBUG_TYPE_TEXT, text, strlen(text));
}

void sc64_init(void) {
    while (!sc64_check_presence());
    sc64_wait_cpu_ready();
    sc64_set_config(CFG_ID_SDRAM_SWITCH, true);
}

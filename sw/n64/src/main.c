#include "sc64.h"


void main(void) {
    sc64_init();

    rom_header_t header;

    io32_t *src = (io32_t *) (ROM_HEADER_CART);
    uint32_t *dst = (uint32_t *) (&header);

    for (int i = 0; i < sizeof(rom_header_t); i += 4) {
        *dst++ = pi_io_read(src++);
    }

    LOG_I("Booting ROM:\r\n");
    LOG_I("  PI Config:         0x%08lX\r\n", header.pi_config);
    LOG_I("  Clock rate:        0x%08lX\r\n", header.clock_rate);
    LOG_I("  Load address:      0x%08lX\r\n", header.load_addr);
    LOG_I("  SDK vesrion:       %d.%d%c\r\n", header.sdk_version.major / 10, header.sdk_version.major % 10, header.sdk_version.minor);
    LOG_I("  1MB CRC:           0x%08lX, 0x%08lX\r\n", header.crc[0], header.crc[1]);
    LOG_I("  Name:              %.20s\r\n", header.name);
    LOG_I("  ID:                %.4s\r\n", header.id);
    LOG_I("  Mask ROM version:  %d\r\n", header.version);
    LOG_I("\r\n");

    info_t info;

    sc64_get_info(&info);

    LOG_I("SC64 settings:\r\n");
    LOG_I("  DD enabled:        %d\r\n", info.dd_enabled);
    LOG_I("  Save type:         %d\r\n", info.save_type);
    LOG_I("  CIC seed:          0x%02X\r\n", info.cic_seed);
    LOG_I("  TV type:           %d\r\n", info.tv_type);
    LOG_I("  Save offset:       0x%08lX\r\n", (uint32_t) info.save_offset);
    LOG_I("  DD offset:         0x%08lX\r\n", (uint32_t) info.dd_offset);
    LOG_I("  Boot mode:         %d\r\n", info.boot_mode);
    LOG_I("  Bootloader ver:    %s\r\n", info.bootloader_version);
    LOG_I("\r\n");
}

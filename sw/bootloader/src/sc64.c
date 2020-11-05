#include <libdragon.h>
#include <inttypes.h>

#include "sc64.h"
#include "sc64_regs.h"

static void sc64_enable_peripheral(uint32_t mask) {
    uint32_t config = io_read(SC64_CONFIG_REG);

    config |= mask;

    io_write(SC64_CONFIG_REG, config);
}

static void sc64_disable_peripheral(uint32_t mask) {
    uint32_t config = io_read(SC64_CONFIG_REG);

    config &= ~mask;

    io_write(SC64_CONFIG_REG, config);
}

void sc64_enable_sdram(void) {
    sc64_enable_peripheral(SC64_CONFIG_SDRAM_ENABLE);
}

void sc64_disable_sdram(void) {
    sc64_disable_peripheral(SC64_CONFIG_SDRAM_ENABLE);
}

cic_type_t sc64_get_cic_type(void) {
    uint32_t boot = io_read(SC64_BOOT_REG);

    cic_type_t cic_type = (cic_type_t)(SC64_BOOT_CIC_TYPE(boot));

    return (cic_type >= E_CIC_TYPE_END) ? E_CIC_TYPE_UNKNOWN : cic_type;
}

tv_type_t sc64_get_tv_type(void) {
    uint32_t boot = io_read(SC64_BOOT_REG);

    if ((SC64_BOOT_CIC_TYPE(boot) == E_CIC_TYPE_UNKNOWN) || (SC64_BOOT_CIC_TYPE(boot) >= E_CIC_TYPE_END)) {
        return E_TV_TYPE_UNKNOWN;
    } else {
        return (tv_type_t)(SC64_BOOT_TV_TYPE(boot));
    }
}

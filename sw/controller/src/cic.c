#include "cic.h"
#include "fpga.h"
#include "hw.h"
#include "led.h"
#include "rtc.h"


typedef enum {
    REGION_NTSC,
    REGION_PAL,
    __REGION_MAX
} cic_region_t;


static bool cic_error_active = false;


void cic_reset_parameters (void) {
    cic_region_t region = rtc_get_region();

    const uint8_t default_seed = 0x3F;
    const uint64_t default_checksum = 0xA536C0F1D859ULL;

    uint32_t cfg[2] = {
        (default_seed << CIC_SEED_BIT) | ((default_checksum >> 32) & 0xFFFF),
        (default_checksum & 0xFFFFFFFFUL)
    };

    if (region == REGION_PAL) {
        cfg[0] |= CIC_REGION;
    }

    fpga_reg_set(REG_CIC_0, cfg[0]);
    fpga_reg_set(REG_CIC_1, cfg[1]);
}

void cic_set_parameters (uint32_t *args) {
    uint32_t cfg[2] = {
        args[0] & (0x00FFFFFF),
        args[1]
    };

    cfg[0] |= fpga_reg_get(REG_CIC_0) & (CIC_64DD_MODE | CIC_REGION);

    if (args[0] & (1 << 24)) {
        cfg[0] |= CIC_DISABLED;
    }

    fpga_reg_set(REG_CIC_0, cfg[0]);
    fpga_reg_set(REG_CIC_1, cfg[1]);
}

void cic_set_dd_mode (bool enabled) {
    uint32_t cfg = fpga_reg_get(REG_CIC_0);

    if (enabled) {
        cfg |= CIC_64DD_MODE;
    } else {
        cfg &= ~(CIC_64DD_MODE);
    }

    fpga_reg_set(REG_CIC_0, cfg);
}


void cic_init (void) {
    cic_reset_parameters();
}


void cic_process (void) {
    uint32_t cfg = fpga_reg_get(REG_CIC_0);

    if (cfg & CIC_INVALID_REGION_DETECTED) {
        cfg ^= CIC_REGION;
        cfg |= CIC_INVALID_REGION_RESET;
        fpga_reg_set(REG_CIC_0, cfg);

        cic_region_t region = (cfg & CIC_REGION) ? REGION_PAL : REGION_NTSC;
        rtc_set_region(region);

        cic_error_active = true;
        led_blink_error(LED_ERROR_CIC);
    }

    if (cic_error_active && (!hw_gpio_get(GPIO_ID_N64_RESET))) {
        cic_error_active = false;
        led_clear_error(LED_ERROR_CIC);
    }
}

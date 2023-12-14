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


static bool cic_initialized = false;


static void cic_irq_reset_falling (void) {
    led_clear_error(LED_ERROR_CIC);
}


void cic_reset_parameters (void) {
    cic_region_t region = rtc_get_region();

    const uint8_t default_seed = 0x3F;
    const uint64_t default_checksum = 0xA536C0F1D859ULL;

    uint32_t cic_config_0 = (default_seed << CIC_SEED_BIT) | ((default_checksum >> 32) & 0xFFFF);
    uint32_t cic_config_1 = (default_checksum & 0xFFFFFFFFUL);

    if (region == REGION_PAL) {
        cic_config_0 |= CIC_REGION;
    }
    
    fpga_reg_set(REG_CIC_0, cic_config_0);
    fpga_reg_set(REG_CIC_1, cic_config_1);
}

void cic_set_parameters (uint32_t *args) {
    uint32_t cic_config_0 = args[0] & (0x00FFFFFF);
    uint32_t cic_config_1 = args[1];

    cic_config_0 |= fpga_reg_get(REG_CIC_0) & (CIC_64DD_MODE | CIC_REGION);

    if (args[0] & (1 << 24)) {
        cic_config_0 |= CIC_DISABLED;
    }

    fpga_reg_set(REG_CIC_0, cic_config_0);
    fpga_reg_set(REG_CIC_1, cic_config_1);
}

void cic_set_dd_mode (bool enabled) {
    uint32_t cic_config_0 = fpga_reg_get(REG_CIC_0);

    if (enabled) {
        cic_config_0 |= CIC_64DD_MODE;
    } else {
        cic_config_0 &= ~(CIC_64DD_MODE);
    }

    fpga_reg_set(REG_CIC_0, cic_config_0);
}


void cic_init (void) {
    hw_gpio_irq_setup(GPIO_ID_N64_RESET, GPIO_IRQ_FALLING, cic_irq_reset_falling);
}


void cic_process (void) {
    if (!cic_initialized) {
        if (rtc_is_initialized()) {
            cic_reset_parameters();
            cic_initialized = true;
        } else {
            return;
        }
    }

    uint32_t cic_config_0 = fpga_reg_get(REG_CIC_0);

    if (cic_config_0 & CIC_INVALID_REGION_DETECTED) {
        cic_config_0 ^= CIC_REGION;
        cic_config_0 |= CIC_INVALID_REGION_RESET;
        fpga_reg_set(REG_CIC_0, cic_config_0);

        if (cic_config_0 & CIC_REGION) {
            rtc_set_region(REGION_PAL);
        } else {
            rtc_set_region(REGION_NTSC);
        }

        led_blink_error(LED_ERROR_CIC);
    }
}

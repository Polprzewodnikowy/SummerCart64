#include "boot/boot.h"

typedef struct sc64_cart_registers {
    __IO reg_t SCR;
    __IO reg_t COMMAND;
    __IO reg_t ARG[2];
    __IO reg_t RESPONSE;
    __IO reg_t BOOTSTRAP;
    __IO reg_t ___unused;
    __IO reg_t VERSION;
} sc64_cfg_registers_t;

#define SC64_CFG_BASE                       (0x1FFF0000)

#define SC64_CFG                            ((__IO sc64_cfg_registers_t *) SC64_CFG_BASE)

#define SC64_CFG_SCR_CPU_BOOTSTRAPPED       (1 << 31)
#define SC64_CFG_SCR_CPU_BUSY               (1 << 30)

#define CMD_SDRAM_SWITCH                    'S'
#define CMD_DEBUG                           'D'

void print_debug(uint32_t a1, uint32_t a2) {
    
    uint32_t scr = 0;
    do {
        scr = platform_pi_io_read(&SC64_CFG->SCR);
    } while (scr & SC64_CFG_SCR_CPU_BUSY);

    platform_pi_io_write(&SC64_CFG->ARG[0], a1);
    platform_pi_io_write(&SC64_CFG->ARG[1], a2);
    platform_pi_io_write(&SC64_CFG->COMMAND, (uint32_t) CMD_DEBUG);
}

int main(void) {
    OS_BOOT_CONFIG->tv_type = TV_NTSC;

    uint32_t scr = 0;

    do {
        scr = platform_pi_io_read(&SC64_CFG->SCR);
    } while (!(scr & SC64_CFG_SCR_CPU_BOOTSTRAPPED));

    do {
        scr = platform_pi_io_read(&SC64_CFG->SCR);
    } while (scr & SC64_CFG_SCR_CPU_BUSY);

    platform_pi_io_write(&SC64_CFG->ARG[0], 1);
    platform_pi_io_write(&SC64_CFG->COMMAND, (uint32_t) CMD_SDRAM_SWITCH);

    do {
        scr = platform_pi_io_read(&SC64_CFG->SCR);
    } while (scr & SC64_CFG_SCR_CPU_BUSY);

    cart_header_t *cart_header = boot_load_cart_header();
    uint16_t cic_seed = boot_get_cic_seed(cart_header);
    tv_type_t tv_type = boot_get_tv_type(cart_header);

    print_debug(cic_seed << 16 | (tv_type & 0x03), cart_header->pi_conf);

    boot(cart_header, cic_seed, tv_type);
}

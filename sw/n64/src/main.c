#include "sc64.h"
#include "boot/boot.h"


int main(void) {
    OS_BOOT_CONFIG->tv_type = TV_NTSC;

    while (!sc64_check_presence());

    sc64_wait_cpu_ready();

    sc64_set_config(SC64_CONFIG_SDRAM_SWITCH, true);

    cart_header_t *cart_header = boot_load_cart_header();
    uint16_t cic_seed = boot_get_cic_seed(cart_header);
    tv_type_t tv_type = boot_get_tv_type(cart_header);

    // sc64_print_debug(cic_seed << 16 | (tv_type & 0x03), cart_header->pi_conf, cart_header->boot_addr);

    boot(cart_header, cic_seed, tv_type);
}

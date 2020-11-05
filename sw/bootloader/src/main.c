#include <libdragon.h>

#include "boot.h"
#include "sc64.h"

int main(void) {
    init_interrupts();

    sc64_enable_sdram();

    cic_type_t cic_type = sc64_get_cic_type();
    tv_type_t tv_type = sc64_get_tv_type();

    cart_header_t *cart_header = boot_load_cart_header();

    // Try to guess CIC and TV type from ROM in SDRAM if no override is provided
    if (cic_type == E_CIC_TYPE_UNKNOWN || tv_type == E_TV_TYPE_UNKNOWN) {
        if (cic_type == E_CIC_TYPE_UNKNOWN) {
            cic_type = boot_get_cic_type(cart_header);
        }

        if (tv_type == E_TV_TYPE_UNKNOWN) {
            tv_type = boot_get_tv_type(cart_header);
        }
    }

    disable_interrupts();

    boot(cart_header, cic_type, tv_type);
}

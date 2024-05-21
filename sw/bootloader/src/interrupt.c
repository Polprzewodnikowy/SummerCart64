#include "display.h"
#include "sc64.h"
#include "version.h"
#include "../assets/assets.h"


typedef enum {
    INTERRUPT_SW_0 = (1 << 0),
    INTERRUPT_SW_1 = (1 << 1),
    INTERRUPT_RCP = (1 << 2),
    INTERRUPT_CART = (1 << 3),
    INTERRUPT_PRENMI = (1 << 4),
    INTERRUPT_HW_5 = (1 << 5),
    INTERRUPT_HW_6 = (1 << 6),
    INTERRUPT_TIMER = (1 << 7),
} interrupt_t;


static void exception_interrupt_unhandled (uint8_t interrupt) {
    display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

    version_print();
    display_printf("[ Unhandled interrupt ]\n");
    display_printf("Pending (0x%02X):\n", interrupt);
    for (int i = 0; i < 8; i++) {
        switch (interrupt & (1 << i)) {
            case INTERRUPT_SW_0: display_printf(" Software interrupt (0)\n"); break;
            case INTERRUPT_SW_1: display_printf(" Software interrupt (1)\n"); break;
            case INTERRUPT_RCP: display_printf(" RCP interrupt (2)\n"); break;
            case INTERRUPT_CART: display_printf(" CART interrupt (3)\n"); break;
            case INTERRUPT_PRENMI: display_printf(" Pre NMI interrupt (4)\n"); break;
            case INTERRUPT_HW_5: display_printf(" Hardware interrupt (5)\n"); break;
            case INTERRUPT_HW_6: display_printf(" Hardware interrupt (6)\n"); break;
            case INTERRUPT_TIMER: display_printf(" Timer interrupt (7)\n"); break;
            default: break;
        }
    }

    while (1);
}


void exception_interrupt_handler (uint8_t interrupt) {
    if (interrupt & INTERRUPT_CART) {
        sc64_irq_t irq = sc64_irq_pending();

        if (irq != SC64_IRQ_NONE) {
            return sc64_irq_callback(irq);
        }
    }

    if (interrupt & INTERRUPT_PRENMI) {
        if (display_ready()) {
            display_init(NULL);
            display_printf("Resetting...\n");
        }
        while (1);
    }

    if (interrupt & INTERRUPT_TIMER) {
        display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));
        version_print();
        display_printf("[ Watchdog ]\n");
        display_printf("SC64 bootloader did not finish loading in 5 seconds\n");
        while (1);
    }

    exception_interrupt_unhandled(interrupt);
}

#include "display.h"
#include "sc64.h"
#include "version.h"
#include "../assets/assets.h"


typedef enum {
    INTERRUPT_NONE = 0,
    INTERRUPT_SW_0 = (1 << 0),
    INTERRUPT_SW_1 = (1 << 1),
    INTERRUPT_RCP = (1 << 2),
    INTERRUPT_CART = (1 << 3),
    INTERRUPT_PRENMI = (1 << 4),
    INTERRUPT_HW_5 = (1 << 5),
    INTERRUPT_HW_6 = (1 << 6),
    INTERRUPT_TIMER = (1 << 7),
} interrupt_t;


void interrupts_handler (uint8_t interrupts) {
    if (interrupts == INTERRUPT_NONE) {
        display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

        version_print();
        display_printf("[ Empty interrupt ]\n");
        display_printf("There is no interrupt to handle\n");

        while (true);
    }

    if (interrupts & INTERRUPT_CART) {
        interrupts &= ~(INTERRUPT_CART);

        sc64_irq_t irq = sc64_irq_pending();

        if (irq != SC64_IRQ_NONE) {
            sc64_irq_callback(irq);
        }
    }

    if (interrupts & INTERRUPT_PRENMI) {
        interrupts &= ~(INTERRUPT_PRENMI);

        if (display_ready()) {
            display_init(NULL);

            display_printf("Resetting...\n");
        }

        while (true);
    }

    if (interrupts & INTERRUPT_TIMER) {
        interrupts &= ~(INTERRUPT_TIMER);

        display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

        version_print();
        display_printf("[ Watchdog ]\n");
        display_printf("SC64 bootloader did not finish loading in 5 seconds\n");

        while (true);
    }

    if (interrupts != INTERRUPT_NONE) {
        display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

        version_print();
        display_printf("[ Unhandled interrupt(s) ]\n");
        display_printf("Pending (0x%02X):\n", interrupts);
        for (int i = 0; i < 8; i++) {
            switch (interrupts & (1 << i)) {
                case INTERRUPT_SW_0: display_printf(" (0) Software interrupt\n"); break;
                case INTERRUPT_SW_1: display_printf(" (1) Software interrupt\n"); break;
                case INTERRUPT_RCP: display_printf(" (2) RCP interrupt\n"); break;
                case INTERRUPT_CART: display_printf(" (3) CART interrupt\n"); break;
                case INTERRUPT_PRENMI: display_printf(" (4) Pre NMI interrupt\n"); break;
                case INTERRUPT_HW_5: display_printf(" (5) Hardware interrupt\n"); break;
                case INTERRUPT_HW_6: display_printf(" (6) Hardware interrupt\n"); break;
                case INTERRUPT_TIMER: display_printf(" (7) Timer interrupt\n"); break;
                default: break;
            }
        }

        while (true);
    }
}

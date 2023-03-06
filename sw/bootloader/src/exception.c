#include <stdarg.h>
#include "display.h"
#include "exception_regs.h"
#include "exception.h"
#include "io.h"
#include "version.h"
#include "vr4300.h"
#include "../assets/assets.h"


#define EXCEPTION_INTERRUPT     (0)
#define EXCEPTION_SYSCALL       (8)

#define INTERRUPT_MASK_TIMER    (1 << 7)

#define SYSCALL_CODE_MASK       (0x03FFFFC0UL)
#define SYSCALL_CODE_BIT        (6)


static const char *exception_get_description (uint8_t exception_code) {
    switch (exception_code) {
        case 0:  return "Interrupt";
        case 1:  return "TLB Modification exception";
        case 2:  return "TLB Miss exception (load or instruction fetch)";
        case 3:  return "TLB Miss exception (store)";
        case 4:  return "Address Error exception (load or instruction fetch)";
        case 5:  return "Address Error exception (store)";
        case 6:  return "Bus Error exception (instruction fetch)";
        case 7:  return "Bus Error exception (data reference: load or store)";
        case 8:  return "Syscall exception";
        case 9:  return "Breakpoint exception";
        case 10: return "Reserved Instruction exception";
        case 11: return "Coprocessor Unusable exception";
        case 12: return "Arithmetic Overflow exception";
        case 13: return "Trap exception";
        case 15: return "Floating-Point exception";
        case 23: return "Watch exception";
    }

    return "Unknown exception";
}


void exception_fatal_handler (uint32_t exception_code, uint32_t interrupt_mask, exception_t *e) {
    version_t *version = version_get();
    uint32_t *instruction_address = (((uint32_t *) (e->epc.u32)) + ((e->cr & C0_CR_BD) ? 1 : 0));

    display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

    display_printf("[ SC64 bootloader metadata ]\n");
    display_printf("branch: %s | tag: %s\n", version->git_branch, version->git_tag);
    display_printf("sha: %s\n", version->git_sha);
    display_printf("msg: %s\n\n", version->git_message);

    if (exception_code != EXCEPTION_SYSCALL) {
        display_printf("%s\n", exception_get_description(exception_code));
        display_printf(" pc: 0x%08lX  sr: 0x%08lX  cr: 0x%08lX  va: 0x%08lX\n", e->epc.u32, e->sr, e->cr, e->badvaddr.u32);
        display_printf(" zr: 0x%08lX  at: 0x%08lX  v0: 0x%08lX  v1: 0x%08lX\n", e->zr.u32, e->at.u32, e->v0.u32, e->v1.u32);
        display_printf(" a0: 0x%08lX  a1: 0x%08lX  a2: 0x%08lX  a3: 0x%08lX\n", e->a0.u32, e->a1.u32, e->a2.u32, e->a3.u32);
        display_printf(" t0: 0x%08lX  t1: 0x%08lX  t2: 0x%08lX  t3: 0x%08lX\n", e->t0.u32, e->t1.u32, e->t2.u32, e->t3.u32);
        display_printf(" t4: 0x%08lX  t5: 0x%08lX  t6: 0x%08lX  t7: 0x%08lX\n", e->t4.u32, e->t5.u32, e->t6.u32, e->t7.u32);
        display_printf(" s0: 0x%08lX  s1: 0x%08lX  s2: 0x%08lX  s3: 0x%08lX\n", e->s0.u32, e->s1.u32, e->s2.u32, e->s3.u32);
        display_printf(" s4: 0x%08lX  s5: 0x%08lX  s6: 0x%08lX  s7: 0x%08lX\n", e->s4.u32, e->s5.u32, e->s6.u32, e->s7.u32);
        display_printf(" t8: 0x%08lX  t9: 0x%08lX  k0: 0x%08lX  k1: 0x%08lX\n", e->t8.u32, e->t9.u32, e->k0.u32, e->k1.u32);
        display_printf(" gp: 0x%08lX  sp: 0x%08lX  s8: 0x%08lX  ra: 0x%08lX\n\n", e->gp.u32, e->sp.u32, e->s8.u32, e->ra.u32);
    } else {
        display_printf("[ Runtime error ]\n");
    }

    if (exception_code == EXCEPTION_INTERRUPT) {
        if (interrupt_mask & INTERRUPT_MASK_TIMER) {
            exception_disable_watchdog();
            display_printf("Still loading after 5 second limit...\n\n");
            return;
        }
    } else if (exception_code == EXCEPTION_SYSCALL) {
        uint32_t code = (((*instruction_address) & SYSCALL_CODE_MASK) >> SYSCALL_CODE_BIT);

        if (code == TRIGGER_CODE_ERROR) {
            const char *fmt = (const char *) (e->a0.u32);
            va_list args = *((va_list *) (e->sp.u32));
            display_vprintf(fmt, args);
            display_printf("\n");
        }
    }

    while (1);
}

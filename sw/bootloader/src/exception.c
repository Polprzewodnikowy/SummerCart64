#include <stdarg.h>
#include "display.h"
#include "exception.h"
#include "io.h"
#include "version.h"
#include "vr4300.h"
#include "../assets/assets.h"


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


void exception_fatal_handler (uint32_t exception_code, exception_t *e) {
    display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

    uint32_t exception_address = e->epc.u32 + (e->cr & C0_CR_BD ? 4 : 0);

    version_print();
    display_printf("[ Unhandled exception ] @ 0x%08X\n", exception_address);
    display_printf("%s\n", exception_get_description(exception_code));
    display_printf(" pc: 0x%08lX  sr: 0x%08lX  cr: 0x%08lX  va: 0x%08lX\n", e->epc.u32, e->sr, e->cr, e->badvaddr.u32);
    display_printf(" zr: 0x%08lX  at: 0x%08lX  v0: 0x%08lX  v1: 0x%08lX\n", e->zr.u32, e->at.u32, e->v0.u32, e->v1.u32);
    display_printf(" a0: 0x%08lX  a1: 0x%08lX  a2: 0x%08lX  a3: 0x%08lX\n", e->a0.u32, e->a1.u32, e->a2.u32, e->a3.u32);
    display_printf(" t0: 0x%08lX  t1: 0x%08lX  t2: 0x%08lX  t3: 0x%08lX\n", e->t0.u32, e->t1.u32, e->t2.u32, e->t3.u32);
    display_printf(" t4: 0x%08lX  t5: 0x%08lX  t6: 0x%08lX  t7: 0x%08lX\n", e->t4.u32, e->t5.u32, e->t6.u32, e->t7.u32);
    display_printf(" s0: 0x%08lX  s1: 0x%08lX  s2: 0x%08lX  s3: 0x%08lX\n", e->s0.u32, e->s1.u32, e->s2.u32, e->s3.u32);
    display_printf(" s4: 0x%08lX  s5: 0x%08lX  s6: 0x%08lX  s7: 0x%08lX\n", e->s4.u32, e->s5.u32, e->s6.u32, e->s7.u32);
    display_printf(" t8: 0x%08lX  t9: 0x%08lX  k0: 0x%08lX  k1: 0x%08lX\n", e->t8.u32, e->t9.u32, e->k0.u32, e->k1.u32);
    display_printf(" gp: 0x%08lX  sp: 0x%08lX  s8: 0x%08lX  ra: 0x%08lX\n", e->gp.u32, e->sp.u32, e->s8.u32, e->ra.u32);
    display_printf(" hi: 0x%016lX\n", e->hi.u64);
    display_printf(" lo: 0x%016lX\n", e->lo.u64);

    while (true);
}
